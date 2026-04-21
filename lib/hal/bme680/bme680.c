/**
 * @file bme680.c
 * @brief BME680 Environmental Sensor Driver Implementation
 * 
 * Wraps esp-idf-lib BME680 component with project-specific interface.
 * Implements CO2 approximation and IAQ calculation from gas resistance.
 * 
 * DEEP SLEEP COMPATIBLE:
 * - Designed for 15-minute wake/sleep cycles
 * - Stores calibration baseline in NVS for persistence
 * - Quick initialization for fast wake cycles
 * - Minimal power consumption during active time
 */

#include "bme680.h"
#include "config.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <math.h>
#include <string.h>

// Log tag
static const char* TAG = LOG_TAG_BME680;

// I2C port for BME680 (using I2C_NUM_0)
#define BME680_I2C_PORT I2C_NUM_0

// NVS keys for persistent storage
#define NVS_NAMESPACE_BME680    "bme680"
#define NVS_KEY_GAS_BASELINE    "gas_baseline"
#define NVS_KEY_CALIBRATED      "calibrated"

// BME680 sensor state
static bool s_initialized = false;
static bool s_gas_measurement_triggered = false;
static uint32_t s_gas_baseline = 0;  // Gas resistance baseline for IAQ calculation
static bool s_baseline_calibrated = false;

// Calibration sample count
#define CALIBRATION_SAMPLES 10
static uint32_t s_calibration_count = 0;
static uint64_t s_calibration_sum = 0;

/**
 * @brief Load calibration baseline from NVS
 * 
 * Restores gas baseline from NVS to maintain calibration across deep sleep cycles.
 * 
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if no baseline stored
 */
static esp_err_t load_calibration_from_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE_BME680, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "No calibration data in NVS (first boot)");
        return ESP_ERR_NOT_FOUND;
    }
    
    // Read gas baseline
    ret = nvs_get_u32(nvs_handle, NVS_KEY_GAS_BASELINE, &s_gas_baseline);
    if (ret == ESP_OK && s_gas_baseline > 0) {
        s_baseline_calibrated = true;
        ESP_LOGI(TAG, "Loaded gas baseline from NVS: %lu Ohms", s_gas_baseline);
    } else {
        ESP_LOGD(TAG, "No valid baseline in NVS");
        ret = ESP_ERR_NOT_FOUND;
    }
    
    nvs_close(nvs_handle);
    return ret;
}

/**
 * @brief Save calibration baseline to NVS
 * 
 * Stores gas baseline in NVS for persistence across deep sleep cycles.
 * 
 * @return ESP_OK on success, error code otherwise
 */
static esp_err_t save_calibration_to_nvs(void)
{
    if (!s_baseline_calibrated || s_gas_baseline == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE_BME680, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Write gas baseline
    ret = nvs_set_u32(nvs_handle, NVS_KEY_GAS_BASELINE, s_gas_baseline);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write baseline to NVS: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }
    
    // Commit changes
    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Saved gas baseline to NVS: %lu Ohms", s_gas_baseline);
    }
    
    nvs_close(nvs_handle);
    return ret;
}

/**
 * @brief Initialize I2C bus for BME680
 * 
 * Handles re-initialization gracefully for deep sleep wake cycles.
 * 
 * @return ESP_OK on success, error code otherwise
 */
static esp_err_t bme680_i2c_init(void)
{
    // Check if I2C driver is already installed
    // After deep sleep, driver will NOT be installed (full reset)
    
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GPIO_NUM_21,  // Default SDA pin for ESP32
        .scl_io_num = GPIO_NUM_22,  // Default SCL pin for ESP32
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    esp_err_t ret = i2c_param_config(BME680_I2C_PORT, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = i2c_driver_install(BME680_I2C_PORT, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        // If driver already installed, that's OK (shouldn't happen after deep sleep)
        if (ret == ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "I2C driver already installed (unexpected after deep sleep)");
            return ESP_OK;
        }
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "I2C initialized on port %d (SDA: GPIO%d, SCL: GPIO%d, Freq: %d Hz)",
             BME680_I2C_PORT, GPIO_NUM_21, GPIO_NUM_22, I2C_MASTER_FREQ_HZ);
    
    return ESP_OK;
}

/**
 * @brief Calculate CO2 approximation from gas resistance
 * 
 * Uses empirical formula to estimate CO2 levels from BME680 gas resistance.
 * This is an approximation - for accurate CO2 measurement, use dedicated CO2 sensor.
 * 
 * Formula: CO2 (ppm) = baseline - (gas_resistance / sensitivity_factor)
 * 
 * @param gas_resistance Gas resistance in Ohms
 * @return Estimated CO2 in ppm (400-5000 range)
 */
static uint16_t calculate_co2_from_gas(float gas_resistance)
{
    // Empirical CO2 approximation formula
    // Baseline: ~450 ppm (typical indoor CO2)
    // Sensitivity: Higher resistance = lower CO2
    
    const float baseline_co2 = 450.0f;
    const float sensitivity = 1000.0f;  // Sensitivity factor
    
    if (gas_resistance <= 0.0f) {
        return BME680_CO2_MIN_PPM;  // Return minimum if invalid
    }
    
    // Calculate CO2 approximation
    // Lower gas resistance indicates higher VOC/CO2
    float co2_estimate = baseline_co2 + (100000.0f - gas_resistance) / sensitivity;
    
    // Clamp to valid range
    if (co2_estimate < BME680_CO2_MIN_PPM) {
        co2_estimate = BME680_CO2_MIN_PPM;
    } else if (co2_estimate > BME680_CO2_MAX_PPM) {
        co2_estimate = BME680_CO2_MAX_PPM;
    }
    
    return (uint16_t)co2_estimate;
}

/**
 * @brief Calculate IAQ index from gas resistance
 * 
 * Indoor Air Quality index (0-500):
 * - 0-50: Excellent
 * - 51-100: Good
 * - 101-150: Lightly polluted
 * - 151-200: Moderately polluted
 * - 201-250: Heavily polluted
 * - 251-350: Severely polluted
 * - 351-500: Extremely polluted
 * 
 * @param gas_resistance Gas resistance in Ohms
 * @return IAQ index (0-500)
 */
static uint8_t calculate_iaq_from_gas(float gas_resistance)
{
    // Use baseline for IAQ calculation
    if (!s_baseline_calibrated || s_gas_baseline == 0) {
        // No baseline yet - return neutral IAQ
        return 100;  // "Good" air quality
    }
    
    // Calculate IAQ based on deviation from baseline
    // Higher resistance = better air quality (lower IAQ)
    // Lower resistance = worse air quality (higher IAQ)
    
    float ratio = (float)s_gas_baseline / gas_resistance;
    float iaq = 0.0f;
    
    if (ratio < 0.5f) {
        // Much better than baseline - excellent
        iaq = 25.0f;
    } else if (ratio < 0.75f) {
        // Better than baseline - good
        iaq = 50.0f + (ratio - 0.5f) * 200.0f;
    } else if (ratio < 1.25f) {
        // Near baseline - good to lightly polluted
        iaq = 100.0f + (ratio - 0.75f) * 100.0f;
    } else if (ratio < 2.0f) {
        // Worse than baseline - moderately polluted
        iaq = 150.0f + (ratio - 1.25f) * 133.0f;
    } else {
        // Much worse than baseline - heavily polluted
        iaq = 250.0f + fminf((ratio - 2.0f) * 100.0f, 250.0f);
    }
    
    // Clamp to valid range
    if (iaq < BME680_IAQ_MIN) {
        iaq = BME680_IAQ_MIN;
    } else if (iaq > BME680_IAQ_MAX) {
        iaq = BME680_IAQ_MAX;
    }
    
    return (uint8_t)iaq;
}

/**
 * @brief Update gas baseline calibration
 * 
 * Collects gas resistance samples to establish baseline for IAQ calculation.
 * Requires CALIBRATION_SAMPLES readings to complete calibration.
 * Saves baseline to NVS when calibration completes.
 * 
 * @param gas_resistance Current gas resistance reading
 */
static void update_gas_baseline(float gas_resistance)
{
    if (s_baseline_calibrated) {
        return;  // Already calibrated
    }
    
    s_calibration_sum += (uint64_t)gas_resistance;
    s_calibration_count++;
    
    if (s_calibration_count >= CALIBRATION_SAMPLES) {
        s_gas_baseline = (uint32_t)(s_calibration_sum / s_calibration_count);
        s_baseline_calibrated = true;
        ESP_LOGI(TAG, "Gas baseline calibrated: %lu Ohms (from %lu samples)",
                 s_gas_baseline, s_calibration_count);
        
        // Save to NVS for persistence across deep sleep
        save_calibration_to_nvs();
    } else {
        ESP_LOGD(TAG, "Calibration progress: %lu/%d samples", 
                 s_calibration_count, CALIBRATION_SAMPLES);
    }
}

esp_err_t bme680_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "BME680 already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing BME680 sensor (deep sleep compatible)...");
    ESP_LOGI(TAG, "I2C Address: 0x%02X", BME680_I2C_ADDR);
    ESP_LOGI(TAG, "Heater: %d°C for %dms", BME680_HEATER_TEMP, BME680_HEATER_DURATION);
    
    // Initialize I2C bus
    esp_err_t ret = bme680_i2c_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C initialization failed");
        return ESP_ERR_INVALID_STATE;
    }
    
    // TODO: Initialize esp-idf-lib BME680 component
    // This will be implemented once the component is available
    // For now, we'll set up the basic structure
    
    // Try to load calibration baseline from NVS
    ret = load_calibration_from_nvs();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Restored calibration from previous session");
    } else {
        // Reset calibration state (first boot or no saved data)
        s_gas_baseline = 0;
        s_baseline_calibrated = false;
        s_calibration_count = 0;
        s_calibration_sum = 0;
        ESP_LOGI(TAG, "Starting fresh calibration (will complete after %d readings)", 
                 CALIBRATION_SAMPLES);
    }
    
    s_gas_measurement_triggered = false;
    s_initialized = true;
    
    ESP_LOGI(TAG, "BME680 initialized successfully");
    
    return ESP_OK;
}

esp_err_t bme680_trigger_gas_measurement(void)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "BME680 not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGD(TAG, "Triggering gas measurement (heater: %d°C, %dms)",
             BME680_HEATER_TEMP, BME680_HEATER_DURATION);
    
    // TODO: Trigger gas heater via esp-idf-lib BME680 component
    // For now, simulate warm-up delay
    vTaskDelay(pdMS_TO_TICKS(BME680_HEATER_DURATION));
    
    s_gas_measurement_triggered = true;
    
    return ESP_OK;
}

bool bme680_gas_ready(void)
{
    return s_initialized && s_gas_measurement_triggered;
}

esp_err_t bme680_read(bme680_data_t* data)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "BME680 not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (data == NULL) {
        ESP_LOGE(TAG, "Invalid data pointer");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Initialize data structure
    memset(data, 0, sizeof(bme680_data_t));
    data->valid = false;
    
    ESP_LOGD(TAG, "Reading BME680 sensor data...");
    
    // TODO: Read actual sensor data via esp-idf-lib BME680 component
    // For now, provide placeholder values for testing
    
    // Placeholder values (will be replaced with actual sensor readings)
    data->temperature_c = 22.5f;
    data->humidity_percent = 55.0f;
    data->pressure_hpa = 1013.25f;
    data->gas_resistance_ohms = 125000.0f;  // Typical good air quality value
    
    // Validate sensor readings
    bool valid = true;
    
    if (data->temperature_c < BME680_TEMP_MIN_C || data->temperature_c > BME680_TEMP_MAX_C) {
        ESP_LOGW(TAG, "Temperature out of range: %.2f°C", data->temperature_c);
        valid = false;
    }
    
    if (data->humidity_percent < BME680_HUM_MIN_PCT || data->humidity_percent > BME680_HUM_MAX_PCT) {
        ESP_LOGW(TAG, "Humidity out of range: %.2f%%", data->humidity_percent);
        valid = false;
    }
    
    if (data->pressure_hpa < BME680_PRES_MIN_HPA || data->pressure_hpa > BME680_PRES_MAX_HPA) {
        ESP_LOGW(TAG, "Pressure out of range: %.2f hPa", data->pressure_hpa);
        valid = false;
    }
    
    if (data->gas_resistance_ohms <= 0.0f) {
        ESP_LOGW(TAG, "Invalid gas resistance: %.2f Ohms", data->gas_resistance_ohms);
        valid = false;
    }
    
    if (valid) {
        // Calculate CO2 approximation from gas resistance
        data->co2_ppm = calculate_co2_from_gas(data->gas_resistance_ohms);
        
        // Update gas baseline calibration
        update_gas_baseline(data->gas_resistance_ohms);
        
        // Calculate IAQ index
        data->iaq_index = calculate_iaq_from_gas(data->gas_resistance_ohms);
        
        // Validate calculated values
        if (data->co2_ppm < BME680_CO2_MIN_PPM || data->co2_ppm > BME680_CO2_MAX_PPM) {
            ESP_LOGW(TAG, "CO2 out of range: %d ppm", data->co2_ppm);
            valid = false;
        }
        
        if (data->iaq_index < BME680_IAQ_MIN || data->iaq_index > BME680_IAQ_MAX) {
            ESP_LOGW(TAG, "IAQ out of range: %d", data->iaq_index);
            valid = false;
        }
        
        data->valid = valid;
        
        ESP_LOGI(TAG, "BME680 Reading: T=%.2f°C, H=%.2f%%, P=%.2f hPa, "
                      "Gas=%.0f Ohms, CO2=%d ppm, IAQ=%d %s",
                 data->temperature_c, data->humidity_percent, data->pressure_hpa,
                 data->gas_resistance_ohms, data->co2_ppm, data->iaq_index,
                 s_baseline_calibrated ? "(calibrated)" : "(calibrating)");
    } else {
        data->valid = false;
        ESP_LOGE(TAG, "BME680 reading validation failed");
    }
    
    // Reset gas measurement trigger
    s_gas_measurement_triggered = false;
    
    return valid ? ESP_OK : ESP_FAIL;
}

esp_err_t bme680_deinit(void)
{
    if (!s_initialized) {
        ESP_LOGW(TAG, "BME680 not initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Deinitializing BME680 sensor...");
    
    // TODO: Deinitialize esp-idf-lib BME680 component
    
    // Delete I2C driver
    esp_err_t ret = i2c_driver_delete(BME680_I2C_PORT);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "I2C driver delete failed: %s", esp_err_to_name(ret));
    }
    
    // Note: We do NOT reset calibration state here
    // Calibration is stored in NVS and will be restored on next init
    
    s_initialized = false;
    s_gas_measurement_triggered = false;
    
    ESP_LOGI(TAG, "BME680 deinitialized (calibration preserved in NVS)");
    
    return ESP_OK;
}
