/**
 * @file main.c
 * @brief Main application entry point for Sensor Data Collection System
 * 
 * Implements sequential state machine for sensor reading, data storage,
 * and transmission with deep sleep power management.
 * 
 * Architecture: Sequential execution (no FreeRTOS tasks)
 * Flow: INIT → READ_SENSORS → STORE_DATA → TRANSMIT_CHECK → 
 *       TRANSMIT_DATA (conditional) → PREPARE_SLEEP → SLEEP
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

// Include system-wide data types and configuration
#include "datatypes.h"
#include "config.h"

// Log tag for main application
static const char* TAG = LOG_TAG_MAIN;

/**
 * @brief Initialize all system components
 * 
 * Initializes NVS, sensors, WiFi, and power management.
 * 
 * @return ESP_OK on success, error code otherwise
 */
static esp_err_t system_init(void)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "=== Sensor Data Collection System ===");
    ESP_LOGI(TAG, "Device ID: %s", DEVICE_ID);
    ESP_LOGI(TAG, "Firmware Version: %s", FIRMWARE_VERSION);
    ESP_LOGI(TAG, "Board: %s", BOARD_NAME);
    
    // Initialize NVS (Non-Volatile Storage)
    ESP_LOGI(TAG, "Initializing NVS...");
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        ESP_LOGW(TAG, "NVS partition needs to be erased");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized successfully");
    
    // TODO: Initialize sensor drivers (Task 4, 5, 6)
    // - BME680 sensor driver
    // - Vibration sensor driver
    // - RTC driver
    
    // TODO: Initialize service layer (Task 8, 9, 10)
    // - Data manager
    // - Communication manager
    // - Power manager
    
    ESP_LOGI(TAG, "System initialization complete");
    return ESP_OK;
}

/**
 * @brief Check if transmission cycle has been reached
 * 
 * @return true if should transmit, false otherwise
 */
static bool should_transmit(void)
{
    // TODO: Implement transmission cycle check (Task 12)
    // Check if TRANSMISSION_INTERVAL readings have been collected
    return false;
}

/**
 * @brief Main application entry point
 * 
 * Implements sequential state machine for system operation.
 * No FreeRTOS tasks - single execution thread only.
 */
void app_main(void)
{
    system_state_t state = STATE_INIT;
    
    ESP_LOGI(TAG, "Starting Sensor Data Collection System...");
    
    // Sequential state machine loop
    while (1) {
        switch (state) {
            case STATE_INIT:
                ESP_LOGI(TAG, "STATE: INIT");
                
                // Initialize all system components
                if (system_init() == ESP_OK) {
                    // NVS automatically loads existing data on init
                    state = STATE_READ_SENSORS;
                } else {
                    ESP_LOGE(TAG, "System initialization failed");
                    // Retry after delay
                    vTaskDelay(pdMS_TO_TICKS(5000));
                }
                break;
                
            case STATE_READ_SENSORS:
                ESP_LOGI(TAG, "STATE: READ_SENSORS");
                
                // TODO: Implement sensor reading (Task 12)
                // - Get timestamp from RTC
                // - Read BME680 sensor
                // - Read vibration sensor
                // - Populate sensor_reading_t structure
                
                state = STATE_STORE_DATA;
                break;
                
            case STATE_STORE_DATA:
                ESP_LOGI(TAG, "STATE: STORE_DATA");
                
                // TODO: Implement data storage (Task 12)
                // - Store reading directly in NVS via data_manager
                
                state = STATE_TRANSMIT_CHECK;
                break;
                
            case STATE_TRANSMIT_CHECK:
                ESP_LOGI(TAG, "STATE: TRANSMIT_CHECK");
                
                // Check if transmission cycle reached
                if (should_transmit()) {
                    state = STATE_TRANSMIT_DATA;
                } else {
                    state = STATE_PREPARE_SLEEP;
                }
                break;
                
            case STATE_TRANSMIT_DATA:
                ESP_LOGI(TAG, "STATE: TRANSMIT_DATA");
                
                // TODO: Implement data transmission (Task 12)
                // - Format accumulated readings as JSON
                // - Connect to WiFi
                // - Send data via HTTP POST
                // - Clear NVS on success
                // - Disconnect WiFi
                
                state = STATE_PREPARE_SLEEP;
                break;
                
            case STATE_PREPARE_SLEEP:
                ESP_LOGI(TAG, "STATE: PREPARE_SLEEP");
                
                // TODO: Implement sleep preparation (Task 12)
                // - Disable peripherals to save power
                
                state = STATE_SLEEP;
                break;
                
            case STATE_SLEEP:
                ESP_LOGI(TAG, "STATE: SLEEP");
                ESP_LOGI(TAG, "Entering deep sleep for %d minutes...", READING_INTERVAL_MIN);
                
                // TODO: Implement deep sleep (Task 12)
                // - Configure wake-up timer
                // - Enter deep sleep mode
                // After wake, execution restarts from app_main()
                
                // For now, just delay to simulate sleep
                vTaskDelay(pdMS_TO_TICKS(5000));
                
                // After wake, restart from INIT
                state = STATE_INIT;
                break;
                
            default:
                ESP_LOGE(TAG, "Unknown state: %d", state);
                state = STATE_INIT;
                break;
        }
    }
}