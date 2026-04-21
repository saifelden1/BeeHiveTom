/**
 * @file vibration.c
 * @brief Piezo Vibration Sensor Driver Implementation
 * 
 * Simple analog piezo sensor reading with averaging.
 * Takes multiple ADC samples and returns the average value.
 * 
 * DEEP SLEEP COMPATIBLE:
 * - Designed for 15-minute wake/sleep cycles
 * - Quick initialization for fast wake cycles
 * - Minimal power consumption during active time
 */

#include "vibration.h"
#include "config.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

// Log tag
static const char* TAG = LOG_TAG_VIBRATION;

// ADC handle
static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static bool s_initialized = false;

// Static sample buffer (avoid dynamic allocation)
static int32_t s_sample_buffer[VIBRATION_BUFFER_SIZE];

esp_err_t vibration_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Vibration sensor already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing vibration sensor (piezo)...");
    ESP_LOGI(TAG, "GPIO: %d, ADC Channel: %d", VIBRATION_GPIO_PIN, VIBRATION_ADC_CHANNEL);
    ESP_LOGI(TAG, "Sample rate: %d Hz (%d ms interval), Duration: %d ms, Buffer: %d samples",
             VIBRATION_SAMPLE_RATE_HZ, VIBRATION_SAMPLE_INTERVAL_MS, 
             VIBRATION_SAMPLE_DURATION_MS, VIBRATION_BUFFER_SIZE);
    
    // Configure ADC oneshot unit
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    
    esp_err_t ret = adc_oneshot_new_unit(&init_config, &s_adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC unit: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configure ADC channel
    adc_oneshot_chan_cfg_t chan_config = {
        .atten = VIBRATION_ADC_ATTEN,
        .bitwidth = VIBRATION_ADC_WIDTH,
    };
    
    ret = adc_oneshot_config_channel(s_adc_handle, VIBRATION_ADC_CHANNEL, &chan_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel: %s", esp_err_to_name(ret));
        adc_oneshot_del_unit(s_adc_handle);
        s_adc_handle = NULL;
        return ret;
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "Vibration sensor initialized successfully");
    
    return ESP_OK;
}

esp_err_t vibration_read(vibration_data_t* data)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "Vibration sensor not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (data == NULL) {
        ESP_LOGE(TAG, "Invalid data pointer");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Initialize data structure
    memset(data, 0, sizeof(vibration_data_t));
    
    ESP_LOGI(TAG, "Reading vibration sensor (sampling for %d ms at %d ms intervals)...", 
             VIBRATION_SAMPLE_DURATION_MS, VIBRATION_SAMPLE_INTERVAL_MS);
    
    // Collect ADC samples at configured intervals
    uint32_t sample_count = 0;
    int64_t start_time = esp_timer_get_time();
    int64_t target_duration_us = VIBRATION_SAMPLE_DURATION_MS * 1000;
    
    while (sample_count < VIBRATION_BUFFER_SIZE) {
        int64_t current_time = esp_timer_get_time();
        int64_t elapsed_time = current_time - start_time;
        
        // Check if sampling duration exceeded
        if (elapsed_time >= target_duration_us) {
            break;
        }
        
        // Read ADC value
        int adc_raw = 0;
        esp_err_t ret = adc_oneshot_read(s_adc_handle, VIBRATION_ADC_CHANNEL, &adc_raw);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "ADC read failed: %s", esp_err_to_name(ret));
            continue;
        }
        
        s_sample_buffer[sample_count++] = adc_raw;
        
        // Wait for next sample interval
        vTaskDelay(pdMS_TO_TICKS(VIBRATION_SAMPLE_INTERVAL_MS));
    }
    
    data->sample_count = sample_count;
    
    if (sample_count < 2) {
        ESP_LOGE(TAG, "Insufficient samples collected: %lu", sample_count);
        return ESP_FAIL;
    }
    
    ESP_LOGD(TAG, "Collected %lu samples", sample_count);
    
    // Calculate average of all ADC readings
    int64_t sum = 0;
    for (uint32_t i = 0; i < sample_count; i++) {
        sum += s_sample_buffer[i];
    }
    float average_adc = (float)sum / (float)sample_count;
    
    // Store average as the vibration reading
    data->dominant_frequency_hz = average_adc;
    
    // Update timestamp
    data->last_vibration_time = esp_timer_get_time() / 1000000; // Convert to seconds
    
    ESP_LOGI(TAG, "Vibration reading: %.1f (average of %lu samples over %d ms)",
             data->dominant_frequency_hz, sample_count, VIBRATION_SAMPLE_DURATION_MS);
    
    return ESP_OK;
}

esp_err_t vibration_deinit(void)
{
    if (!s_initialized) {
        ESP_LOGW(TAG, "Vibration sensor not initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Deinitializing vibration sensor...");
    
    // Delete ADC unit
    if (s_adc_handle != NULL) {
        esp_err_t ret = adc_oneshot_del_unit(s_adc_handle);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to delete ADC unit: %s", esp_err_to_name(ret));
        }
        s_adc_handle = NULL;
    }
    
    s_initialized = false;
    
    ESP_LOGI(TAG, "Vibration sensor deinitialized");
    
    return ESP_OK;
}
