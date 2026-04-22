/**
 * @file bme680.c
 * @brief BME680 Environmental Sensor Driver Implementation
 * 
 * Wraps esp-idf-lib BME680 component with project interface.
 * Implements CO2 approximation and IAQ calculation from gas resistance.
 */

#include "bme680.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <bme680.h>  // esp-idf-lib component
#include <string.h>

static const char* TAG = LOG_TAG_BME680;

// NVS keys for calibration persistence
#define NVS_NAMESPACE_BME680    "bme680"
#define NVS_KEY_GAS_BASELINE    "gas_baseline"

// BME680 device descriptor (static allocation)
static bme680_t s_dev;
static bool s_initialized = false;

// I2C initialization tracking (shared across I2C devices)
static bool s_i2c_initialized = false;

// Gas baseline for IAQ calculation
static uint32_t s_gas_baseline = 0;
static bool s_baseline_calibrated = false;

// Calibration tracking
#define CALIBRATION_SAMPLES 10
static uint32_t s_calibration_count = 0;
static uint64_t s_calibration_sum = 0;

/**
 * @brief Initialize I2C bus if not already initialized
 */
static esp_err_t ensure_i2c_initialized(void)
{
    if (s_i2c_initialized) {
        return ESP_OK;
    }
    
    // I2C is initialized by esp-idf-lib bme680_init_desc
    // Just mark as initialized after first successful init
    s_i2c_initialized = true;
    return ESP_OK;
}

/**
 * @brief Load gas baseline from NVS
 */
static esp_err_t load_baseline_from_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE_BME680, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        return ESP_ERR_NOT_FOUND;
    }
    
    ret = nvs_get_u32(nvs_handle, NVS_KEY_GAS_BASELINE, &s_gas_baseline);
    if (ret == ESP_OK && s_gas_baseline > 0) {
        s_baseline_calibrated = true;
        ESP_LOGI(TAG, "Loaded gas baseline: %lu Ohms", s_gas_baseline);
    }
    
    nvs_close(nvs_handle);
    return ret;
}

/**
 * @brief Save gas baseline to NVS
 */
static esp_err_t save_baseline_to_nvs(void)
{
    if (!s_baseline_calibrated || s_gas_baseline == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE_BME680, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    ret = nvs_set_u32(nvs_handle, NVS_KEY_GAS_BASELINE, s_gas_baseline);
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs_handle);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Saved gas baseline: %lu Ohms", s_gas_baseline);
        }
    }
    
    nvs_close(nvs_handle);
    return ret;
}

/**
 * @brief Calculate CO2 approximation from gas resistance
 */
static uint16_t calculate_co2_from_gas(float gas_resistance)
{
    if (gas_resistance <= 0.0f) {
        return BME680_CO2_MIN_PPM;
    }
    
    // Use constants from config.h
    float co2_estimate = BME680_CO2_BASELINE_PPM + 
                        (BME680_CO2_REFERENCE_OHM - gas_resistance) / BME680_CO2_SENSITIVITY;
    
    // Clamp to valid range
    if (co2_estimate < BME680_CO2_MIN_PPM) {
        co2_estimate = BME680_CO2_MIN_PPM;
    } else if (co2_estimate > BME680_CO2_MAX_PPM) {
        co2_estimate = BME680_CO2_MAX_PPM;
    }
    
    return (uint16_t)co2_estimate;
}

/**
 * @brief Calculate IAQ index from gas resistance (removed - not used)
 */

/**
 * @brief Update gas baseline calibration
 */
static void update_gas_baseline(float gas_resistance)
{
    if (s_baseline_calibrated) {
        return;
    }
    
    s_calibration_sum += (uint64_t)gas_resistance;
    s_calibration_count++;
    
    if (s_calibration_count >= CALIBRATION_SAMPLES) {
        s_gas_baseline = (uint32_t)(s_calibration_sum / s_calibration_count);
        s_baseline_calibrated = true;
        ESP_LOGI(TAG, "Gas baseline calibrated: %lu Ohms", s_gas_baseline);
        save_baseline_to_nvs();
    }
}

esp_err_t bme680_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "BME680 already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing BME680 (addr: 0x%02X)", BME680_I2C_ADDR);
    
    // Initialize device descriptor
    memset(&s_dev, 0, sizeof(bme680_t));
    esp_err_t ret = bme680_init_desc(&s_dev, BME680_I2C_ADDR, I2C_NUM_0, GPIO_NUM_21, GPIO_NUM_22);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init descriptor: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Initialize sensor
    ret = bme680_init_sensor(&s_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init sensor: %s", esp_err_to_name(ret));
        bme680_free_desc(&s_dev);
        return ret;
    }
    
    // Configure oversampling
    ret = bme680_set_oversampling_rates(&s_dev, BME680_OSR_2X, BME680_OSR_2X, BME680_OSR_2X);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set oversampling: %s", esp_err_to_name(ret));
    }
    
    // Configure heater profile 0
    ret = bme680_set_heater_profile(&s_dev, 0, BME680_HEATER_TEMP, BME680_HEATER_DURATION);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set heater profile: %s", esp_err_to_name(ret));
    }
    
    // Activate heater profile 0
    ret = bme680_use_heater_profile(&s_dev, 0);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to activate heater: %s", esp_err_to_name(ret));
    }
    
    // Load calibration baseline
    if (load_baseline_from_nvs() != ESP_OK) {
        s_gas_baseline = 0;
        s_baseline_calibrated = false;
        s_calibration_count = 0;
        s_calibration_sum = 0;
        ESP_LOGI(TAG, "Starting fresh calibration (%d samples needed)", CALIBRATION_SAMPLES);
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "BME680 initialized successfully");
    
    return ESP_OK;
}

esp_err_t bme680_trigger_gas_measurement(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    return bme680_force_measurement(&s_dev);
}

bool bme680_gas_ready(void)
{
    if (!s_initialized) {
        return false;
    }
    
    bool busy = true;
    esp_err_t ret = bme680_is_measuring(&s_dev, &busy);
    return (ret == ESP_OK && !busy);
}

esp_err_t bme680_read(float* temperature_c, float* humidity_percent, uint16_t* co2_ppm)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (temperature_c == NULL || humidity_percent == NULL || co2_ppm == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Read sensor values (floating point)
    bme680_values_float_t values;
    esp_err_t ret = bme680_get_results_float(&s_dev, &values);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read sensor: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Copy values
    *temperature_c = values.temperature;
    *humidity_percent = values.humidity;
    
    // Validate ranges
    bool valid = true;
    if (*temperature_c < BME680_TEMP_MIN_C || *temperature_c > BME680_TEMP_MAX_C) {
        ESP_LOGW(TAG, "Temperature out of range: %.2f°C", *temperature_c);
        valid = false;
    }
    if (*humidity_percent < BME680_HUM_MIN_PCT || *humidity_percent > BME680_HUM_MAX_PCT) {
        ESP_LOGW(TAG, "Humidity out of range: %.2f%%", *humidity_percent);
        valid = false;
    }
    
    if (valid && values.gas_resistance > 0.0f) {
        // Calculate CO2
        *co2_ppm = calculate_co2_from_gas(values.gas_resistance);
        update_gas_baseline(values.gas_resistance);
        
        ESP_LOGI(TAG, "T=%.1f°C H=%.1f%% Gas=%.0fΩ CO2=%dppm %s",
                 *temperature_c, *humidity_percent, values.gas_resistance, 
                 *co2_ppm, s_baseline_calibrated ? "" : "(calibrating)");
    } else {
        ESP_LOGE(TAG, "Invalid sensor data");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t bme680_measure_and_read(float* temperature_c, float* humidity_percent, uint16_t* co2_ppm)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (temperature_c == NULL || humidity_percent == NULL || co2_ppm == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Use convenience function from esp-idf-lib
    bme680_values_float_t values;
    esp_err_t ret = bme680_measure_float(&s_dev, &values);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Measurement failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Copy and process values
    *temperature_c = values.temperature;
    *humidity_percent = values.humidity;
    
    // Calculate CO2
    if (values.gas_resistance > 0.0f) {
        *co2_ppm = calculate_co2_from_gas(values.gas_resistance);
        update_gas_baseline(values.gas_resistance);
    } else {
        *co2_ppm = BME680_CO2_MIN_PPM;
    }
    
    ESP_LOGI(TAG, "T=%.1f°C H=%.1f%% Gas=%.0fΩ CO2=%dppm",
             *temperature_c, *humidity_percent, values.gas_resistance, *co2_ppm);
    
    return ESP_OK;
}

esp_err_t bme680_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Deinitializing BME680");
    
    bme680_free_desc(&s_dev);
    s_initialized = false;
    
    // Calibration preserved in NVS
    ESP_LOGI(TAG, "BME680 deinitialized (calibration preserved)");
    
    return ESP_OK;
}
