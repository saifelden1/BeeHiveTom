/**
 * @file battery.c
 * @brief Battery Level HAL Implementation
 */

#include "battery.h"
#include "config.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

static const char* TAG = "BATTERY";

static adc_oneshot_unit_handle_t adc_handle = NULL;
static bool initialized = false;

esp_err_t battery_init(void)
{
    if (initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };

    if (adc_oneshot_new_unit(&init_config, &adc_handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC unit");
        return ESP_FAIL;
    }

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = BATTERY_ADC_WIDTH,
        .atten = BATTERY_ADC_ATTEN,
    };

    if (adc_oneshot_config_channel(adc_handle, BATTERY_ADC_CHANNEL, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel");
        adc_oneshot_del_unit(adc_handle);
        return ESP_FAIL;
    }

    initialized = true;
    ESP_LOGI(TAG, "Initialized");
    return ESP_OK;
}

esp_err_t battery_read_level(uint8_t* level_percent)
{
    if (!initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (level_percent == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    int adc_raw = 0;
    if (adc_oneshot_read(adc_handle, BATTERY_ADC_CHANNEL, &adc_raw) != ESP_OK) {
        ESP_LOGE(TAG, "ADC read failed");
        return ESP_FAIL;
    }

    // Convert ADC to voltage (mV) at ADC pin
    uint32_t adc_voltage_mv = (adc_raw * BATTERY_VREF_MV) / BATTERY_ADC_MAX_VALUE;
    
    // Calculate actual battery voltage (account for divider)
    uint32_t battery_voltage_mv = adc_voltage_mv * BATTERY_VOLTAGE_DIVIDER;

    // Clamp to valid range
    if (battery_voltage_mv > BATTERY_MAX_VOLTAGE_MV) {
        battery_voltage_mv = BATTERY_MAX_VOLTAGE_MV;
    }
    if (battery_voltage_mv < BATTERY_MIN_VOLTAGE_MV) {
        battery_voltage_mv = BATTERY_MIN_VOLTAGE_MV;
    }

    // Calculate percentage
    uint32_t voltage_range = BATTERY_MAX_VOLTAGE_MV - BATTERY_MIN_VOLTAGE_MV;
    uint32_t voltage_above_min = battery_voltage_mv - BATTERY_MIN_VOLTAGE_MV;
    *level_percent = (uint8_t)((voltage_above_min * 100) / voltage_range);

    ESP_LOGD(TAG, "ADC=%d, Voltage=%lumV, Level=%u%%", 
             adc_raw, (unsigned long)battery_voltage_mv, *level_percent);

    return ESP_OK;
}

esp_err_t battery_deinit(void)
{
    if (!initialized) {
        return ESP_OK;
    }

    if (adc_handle != NULL) {
        adc_oneshot_del_unit(adc_handle);
        adc_handle = NULL;
    }

    initialized = false;
    ESP_LOGI(TAG, "Deinitialized");
    return ESP_OK;
}
