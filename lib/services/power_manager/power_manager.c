/**
 * @file power_manager.c
 * @brief Power Manager Service Implementation
 */

#include "power_manager.h"
#include "config.h"
#include "bme680.h"
#include "ds3231.h"
#include "vibration.h"
#include "battery.h"
#include "esp_sleep.h"
#include "esp_log.h"

static const char* TAG = LOG_TAG_POWER_MGR;

static bool initialized = false;

esp_err_t power_manager_init(void)
{
    if (initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    // Configure timer wake source
    esp_sleep_enable_timer_wakeup(DEEP_SLEEP_WAKEUP_TIME_US);

    initialized = true;
    ESP_LOGI(TAG, "Initialized (wake interval: %u seconds)", SLEEP_DURATION_SEC);
    return ESP_OK;
}

esp_err_t power_manager_sleep(void)
{
    if (!initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Preparing for deep sleep");

    // Deinitialize all sensors to save power
    bme680_deinit();
    ds3231_deinit();
    vibration_deinit();
    battery_deinit();

    ESP_LOGI(TAG, "Entering deep sleep for %u seconds", SLEEP_DURATION_SEC);

    // Enter deep sleep (does not return)
    esp_deep_sleep_start();

    // Should never reach here
    return ESP_FAIL;
}
