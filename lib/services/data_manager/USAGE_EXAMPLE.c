/**
 * @file USAGE_EXAMPLE.c
 * @brief Data Manager Usage Example
 * 
 * This file demonstrates how to use the data_manager service
 * in the sensor data collection system.
 */

#include "data_manager.h"
#include "config.h"
#include "esp_log.h"
#include <string.h>

static const char* TAG = "DATA_MGR_EXAMPLE";

/**
 * @brief Example: Store and retrieve sensor readings
 */
void example_store_and_retrieve(void)
{
    ESP_LOGI(TAG, "=== Store and Retrieve Example ===");
    
    // Initialize data manager
    esp_err_t err = data_manager_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize data manager");
        return;
    }

    // Create sample reading
    sensor_reading_t reading = {
        .timestamp = 1234567890,
        .env = {
            .temperature_c = 25.5f,
            .humidity_percent = 60.0f,
            .pressure_hpa = 1013.25f,
            .gas_resistance_ohms = 50000.0f,
            .co2_ppm = 450,
            .iaq_index = 50,
            .valid = true
        },
        .vibration = {
            .dominant_frequency_hz = 0.0f,
            .amplitude = 0.0f,
            .sample_count = 0,
            .vibration_detected = false,
            .last_vibration_time = 0
        },
        .battery_level = 100,
        .valid = true
    };

    // Store reading
    err = data_manager_store_reading(&reading);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Successfully stored reading");
    } else {
        ESP_LOGE(TAG, "Failed to store reading");
    }

    // Get count
    uint32_t count = data_manager_get_count();
    ESP_LOGI(TAG, "Total stored readings: %lu", (unsigned long)count);

    // Retrieve reading
    sensor_reading_t retrieved;
    err = data_manager_get_reading(0, &retrieved);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Retrieved reading: temp=%.2f°C, hum=%.2f%%, co2=%u ppm",
                retrieved.env.temperature_c,
                retrieved.env.humidity_percent,
                retrieved.env.co2_ppm);
    }

    // Cleanup
    data_manager_deinit();
}

/**
 * @brief Example: Format JSON for transmission
 */
void example_format_json(void)
{
    ESP_LOGI(TAG, "=== Format JSON Example ===");
    
    // Initialize
    data_manager_init();

    // Store multiple readings
    for (int i = 0; i < 3; i++) {
        sensor_reading_t reading = {
            .timestamp = 1234567890 + (i * 900),  // 15 min intervals
            .env = {
                .temperature_c = 25.0f + i,
                .humidity_percent = 60.0f + i,
                .pressure_hpa = 1013.25f,
                .gas_resistance_ohms = 50000.0f,
                .co2_ppm = 450 + (i * 10),
                .iaq_index = 50,
                .valid = true
            },
            .vibration = {
                .dominant_frequency_hz = 0.0f,
                .amplitude = 0.0f,
                .sample_count = 0,
                .vibration_detected = false,
                .last_vibration_time = 0
            },
            .battery_level = 100 - i,
            .valid = true
        };
        
        data_manager_store_reading(&reading);
    }

    ESP_LOGI(TAG, "Stored %lu readings", (unsigned long)data_manager_get_count());

    // Format as JSON
    static char json_buffer[JSON_BUFFER_SIZE];
    esp_err_t err = data_manager_format_json(json_buffer, sizeof(json_buffer));
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "JSON formatted successfully:");
        ESP_LOGI(TAG, "%s", json_buffer);
    } else {
        ESP_LOGE(TAG, "Failed to format JSON: %s", esp_err_to_name(err));
    }

    // Cleanup
    data_manager_deinit();
}

/**
 * @brief Example: Complete workflow (store, transmit, clear)
 */
void example_complete_workflow(void)
{
    ESP_LOGI(TAG, "=== Complete Workflow Example ===");
    
    // Initialize
    data_manager_init();

    // Simulate multiple wake cycles (no WiFi)
    ESP_LOGI(TAG, "Simulating wake cycles without WiFi...");
    for (int cycle = 0; cycle < 4; cycle++) {
        sensor_reading_t reading = {
            .timestamp = 1234567890 + (cycle * 900),
            .env = {
                .temperature_c = 25.0f + cycle * 0.5f,
                .humidity_percent = 60.0f,
                .pressure_hpa = 1013.25f,
                .gas_resistance_ohms = 50000.0f,
                .co2_ppm = 450,
                .iaq_index = 50,
                .valid = true
            },
            .vibration = {
                .dominant_frequency_hz = 0.0f,
                .amplitude = 0.0f,
                .sample_count = 0,
                .vibration_detected = false,
                .last_vibration_time = 0
            },
            .battery_level = 100,
            .valid = true
        };
        
        data_manager_store_reading(&reading);
        ESP_LOGI(TAG, "Cycle %d: Stored reading (total: %lu)", 
                cycle + 1, (unsigned long)data_manager_get_count());
    }

    // Check if transmission needed
    uint32_t count = data_manager_get_count();
    if (count >= TRANSMISSION_INTERVAL) {
        ESP_LOGI(TAG, "Transmission threshold reached (%lu >= %d)", 
                (unsigned long)count, TRANSMISSION_INTERVAL);
        
        // Format JSON
        static char json_buffer[JSON_BUFFER_SIZE];
        esp_err_t err = data_manager_format_json(json_buffer, sizeof(json_buffer));
        
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "JSON ready for transmission (%zu bytes)", strlen(json_buffer));
            
            // Simulate successful transmission
            ESP_LOGI(TAG, "Simulating successful transmission...");
            
            // Clear after successful transmission
            data_manager_clear();
            ESP_LOGI(TAG, "Cleared all readings (remaining: %lu)", 
                    (unsigned long)data_manager_get_count());
        }
    }

    // Cleanup
    data_manager_deinit();
}

/**
 * @brief Example: Error handling
 */
void example_error_handling(void)
{
    ESP_LOGI(TAG, "=== Error Handling Example ===");
    
    // Initialize
    data_manager_init();

    // Test invalid reading (valid = false)
    sensor_reading_t invalid_reading = {
        .valid = false
    };
    
    esp_err_t err = data_manager_store_reading(&invalid_reading);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Correctly rejected invalid reading");
    }

    // Test NULL pointer
    err = data_manager_store_reading(NULL);
    if (err == ESP_ERR_INVALID_ARG) {
        ESP_LOGI(TAG, "Correctly rejected NULL pointer");
    }

    // Test format JSON with no readings
    char buffer[256];
    err = data_manager_format_json(buffer, sizeof(buffer));
    if (err == ESP_ERR_NOT_FOUND) {
        ESP_LOGI(TAG, "Correctly reported no readings to format");
    }

    // Test buffer too small
    char small_buffer[10];
    
    // Store a reading first
    sensor_reading_t reading = {
        .timestamp = 1234567890,
        .env = { .temperature_c = 25.0f, .valid = true },
        .valid = true
    };
    data_manager_store_reading(&reading);
    
    err = data_manager_format_json(small_buffer, sizeof(small_buffer));
    if (err == ESP_ERR_INVALID_SIZE) {
        ESP_LOGI(TAG, "Correctly detected buffer too small");
    }

    // Cleanup
    data_manager_clear();
    data_manager_deinit();
}

/**
 * @brief Example: Check NVS capacity
 */
void example_check_capacity(void)
{
    ESP_LOGI(TAG, "=== Check Capacity Example ===");
    
    // Initialize
    data_manager_init();

    // Check if NVS is full
    bool is_full = data_manager_is_full();
    ESP_LOGI(TAG, "NVS status: %s", is_full ? "FULL" : "Available");

    if (is_full) {
        ESP_LOGW(TAG, "NVS nearly full, should trigger transmission");
        
        // In real application, trigger transmission here
        // comm_manager_connect();
        // comm_manager_send_data(...);
        // data_manager_clear();
    }

    // Cleanup
    data_manager_deinit();
}

/**
 * @brief Main example function
 */
void data_manager_examples(void)
{
    ESP_LOGI(TAG, "Starting Data Manager Examples");
    
    // Run examples
    example_store_and_retrieve();
    example_format_json();
    example_complete_workflow();
    example_error_handling();
    example_check_capacity();
    
    ESP_LOGI(TAG, "All examples completed");
}
