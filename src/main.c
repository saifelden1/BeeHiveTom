/**
 * @file main.c
 * @brief Main application - Sequential sensor reading and transmission
 * 
 * Simple flow: init → read → store → transmit (if ready) → sleep → repeat
 */

#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"

#include "config.h"
#include "datatypes.h"
#include "bme680.h"
#include "ds3231.h"
#include "vibration.h"
#include "battery.h"
#include "data_manager.h"
#include "comm_manager.h"
#include "power_manager.h"

static const char* TAG = LOG_TAG_MAIN;

void app_main(void)
{
    esp_err_t ret;
    sensor_reading_t reading = {0};
    
    ESP_LOGI(TAG, "=== Sensor Data Collection System ===");
    ESP_LOGI(TAG, "Device: %s, Firmware: %s", DEVICE_ID, FIRMWARE_VERSION);
    
    // ========================================================================
    // 1. INITIALIZE NVS
    // ========================================================================
    ESP_LOGI(TAG, "Initializing NVS...");
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Erasing NVS partition");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // ========================================================================
    // 2. INITIALIZE SENSORS
    // ========================================================================
    ESP_LOGI(TAG, "Initializing sensors...");
    
    if (bme680_init() != ESP_OK) {
        ESP_LOGE(TAG, "BME680 init failed");
    }
    
    if (rtc_init() != ESP_OK) {
        ESP_LOGE(TAG, "RTC init failed");
    }
    
    if (vibration_init() != ESP_OK) {
        ESP_LOGE(TAG, "Vibration init failed");
    }
    
    if (battery_init() != ESP_OK) {
        ESP_LOGE(TAG, "Battery init failed");
    }
    
    // ========================================================================
    // 3. INITIALIZE SERVICES
    // ========================================================================
    ESP_LOGI(TAG, "Initializing services...");
    
    if (data_manager_init() != ESP_OK) {
        ESP_LOGE(TAG, "Data manager init failed");
    }
    
    if (comm_manager_init() != ESP_OK) {
        ESP_LOGE(TAG, "Comm manager init failed");
    }
    
    if (power_manager_init() != ESP_OK) {
        ESP_LOGE(TAG, "Power manager init failed");
    }
    
    // ========================================================================
    // 4. READ SENSORS
    // ========================================================================
    ESP_LOGI(TAG, "Reading sensors...");
    
    // Get timestamp from RTC
    if (rtc_get_timestamp(&reading.timestamp) == ESP_OK) {
        ESP_LOGI(TAG, "Timestamp: %llu", (unsigned long long)reading.timestamp);
    } else {
        ESP_LOGW(TAG, "Failed to read RTC, using 0");
        reading.timestamp = 0;
    }
    
    // Read BME680 (temperature, humidity, CO2)
    if (bme680_read(&reading.temperature_c, &reading.humidity_percent, &reading.co2_ppm) == ESP_OK) {
        ESP_LOGI(TAG, "BME680: %.1f°C, %.1f%%, %uppm", 
                 reading.temperature_c, reading.humidity_percent, reading.co2_ppm);
    } else {
        ESP_LOGW(TAG, "BME680 read failed");
        reading.temperature_c = 0.0f;
        reading.humidity_percent = 0.0f;
        reading.co2_ppm = 0;
    }
    
    // Read vibration
    if (vibration_read(&reading.vibration_amplitude) == ESP_OK) {
        ESP_LOGI(TAG, "Vibration: %.3f", reading.vibration_amplitude);
    } else {
        ESP_LOGW(TAG, "Vibration read failed");
        reading.vibration_amplitude = 0.0f;
    }
    
    // Read battery level
    if (battery_read_level(&reading.battery_level) == ESP_OK) {
        ESP_LOGI(TAG, "Battery: %u%%", reading.battery_level);
    } else {
        ESP_LOGW(TAG, "Battery read failed");
        reading.battery_level = 0;
    }
    
    reading.valid = true;
    
    // ========================================================================
    // 5. STORE READING
    // ========================================================================
    ESP_LOGI(TAG, "Storing reading...");
    
    if (data_manager_store_reading(&reading) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to store reading");
    }
    
    uint32_t stored_count = data_manager_get_count();
    ESP_LOGI(TAG, "Stored readings: %lu", (unsigned long)stored_count);
    
    // ========================================================================
    // 6. TRANSMIT (ALWAYS TRY, REGARDLESS OF COUNT)
    // ========================================================================
    ESP_LOGI(TAG, "Attempting transmission (%lu readings stored)...", (unsigned long)stored_count);
    
    // Try to connect to WiFi
    if (comm_manager_connect() == ESP_OK) {
        ESP_LOGI(TAG, "WiFi connected");
        
        // Allocate JSON buffer
        char* json_buffer = malloc(JSON_BUFFER_SIZE);
        if (json_buffer != NULL) {
            // Format JSON with all stored readings
            if (data_manager_format_json(json_buffer, JSON_BUFFER_SIZE) == ESP_OK) {
                ESP_LOGI(TAG, "JSON formatted (%zu bytes)", strlen(json_buffer));
                
                // Send data
                if (comm_manager_send_json(json_buffer, strlen(json_buffer)) == ESP_OK) {
                    ESP_LOGI(TAG, "Data sent successfully");
                    
                    // Clear NVS on success
                    if (data_manager_clear() == ESP_OK) {
                        ESP_LOGI(TAG, "NVS cleared");
                    }
                } else {
                    ESP_LOGW(TAG, "Failed to send data, keeping in NVS for next cycle");
                }
            } else {
                ESP_LOGE(TAG, "Failed to format JSON");
            }
            
            free(json_buffer);
        } else {
            ESP_LOGE(TAG, "Failed to allocate JSON buffer");
        }
        
        // Disconnect WiFi
        comm_manager_disconnect();
        
    } else {
        ESP_LOGW(TAG, "WiFi connection failed, skipping transmission");
        ESP_LOGI(TAG, "Reading stored in NVS, will retry next cycle");
    }
    
    // ========================================================================
    // 7. SLEEP
    // ========================================================================
    ESP_LOGI(TAG, "Entering deep sleep for %u minutes...", READING_INTERVAL_MIN);
    
    // This deinitializes sensors and enters deep sleep (does not return)
    power_manager_sleep();
    
    // Should never reach here
    ESP_LOGE(TAG, "Failed to enter deep sleep!");
}
