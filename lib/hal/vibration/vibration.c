/**
 * @file vibration.c
 * @brief Piezo Vibration Sensor Driver Implementation
 * 
 * Implements analog piezo sensor reading with frequency and amplitude analysis.
 * Uses zero-crossing detection and median filtering for continuous vibration.
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
#include <string.h>
#include <math.h>

// Log tag
static const char* TAG = LOG_TAG_VIBRATION;

// ADC handle
static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static bool s_initialized = false;

// Static sample buffer (avoid dynamic allocation)
static int32_t s_sample_buffer[VIBRATION_BUFFER_SIZE];

/**
 * @brief Compare function for qsort (median calculation)
 */
static int compare_int32(const void* a, const void* b)
{
    int32_t val_a = *(const int32_t*)a;
    int32_t val_b = *(const int32_t*)b;
    
    if (val_a < val_b) return -1;
    if (val_a > val_b) return 1;
    return 0;
}

/**
 * @brief Calculate median value from array
 * 
 * @param values Array of values
 * @param count Number of values
 * @return Median value
 */
static int32_t calculate_median(int32_t* values, uint32_t count)
{
    if (count == 0) {
        return 0;
    }
    
    // Sort array
    qsort(values, count, sizeof(int32_t), compare_int32);
    
    // Return median
    if (count % 2 == 0) {
        return (values[count / 2 - 1] + values[count / 2]) / 2;
    } else {
        return values[count / 2];
    }
}

/**
 * @brief Detect zero crossings and calculate frequency
 * 
 * Counts zero crossings in the sample buffer to determine frequency.
 * Uses median of detected frequencies for stability.
 * 
 * @param samples Sample buffer
 * @param count Number of samples
 * @param baseline DC offset baseline
 * @param frequencies Output array for detected frequencies
 * @param max_freq_count Maximum number of frequencies to store
 * @return Number of frequencies detected
 */
static uint32_t detect_zero_crossings(const int32_t* samples, uint32_t count, 
                                      int32_t baseline, int32_t* frequencies,
                                      uint32_t max_freq_count)
{
    if (count < 2) {
        return 0;
    }
    
    uint32_t crossing_count = 0;
    uint32_t freq_index = 0;
    int64_t last_crossing_time = 0;
    
    // Detect zero crossings
    for (uint32_t i = 1; i < count && freq_index < max_freq_count; i++) {
        int32_t prev_val = samples[i - 1] - baseline;
        int32_t curr_val = samples[i] - baseline;
        
        // Check for zero crossing (sign change)
        if ((prev_val < 0 && curr_val >= 0) || (prev_val >= 0 && curr_val < 0)) {
            crossing_count++;
            
            // Calculate frequency from time between crossings
            if (last_crossing_time > 0) {
                // Time between crossings in microseconds
                int64_t time_diff = (i - last_crossing_time) * VIBRATION_SAMPLE_INTERVAL_US;
                
                // Frequency = 1 / (2 * period) because we count both positive and negative crossings
                // Convert to Hz: 1000000 / (2 * time_diff_us)
                int32_t freq_hz = (int32_t)(500000 / time_diff);
                
                // Validate frequency range
                if (freq_hz >= VIBRATION_MIN_FREQ_HZ && freq_hz <= VIBRATION_MAX_FREQ_HZ) {
                    frequencies[freq_index++] = freq_hz;
                }
            }
            
            last_crossing_time = i;
        }
    }
    
    return freq_index;
}

/**
 * @brief Calculate amplitude from samples
 * 
 * Calculates peak-to-peak amplitude and normalizes to 0.0-1.0 range.
 * 
 * @param samples Sample buffer
 * @param count Number of samples
 * @param baseline DC offset baseline
 * @return Normalized amplitude (0.0-1.0)
 */
static float calculate_amplitude(const int32_t* samples, uint32_t count, int32_t baseline)
{
    if (count == 0) {
        return 0.0f;
    }
    
    int32_t min_val = samples[0];
    int32_t max_val = samples[0];
    
    // Find min and max values
    for (uint32_t i = 1; i < count; i++) {
        if (samples[i] < min_val) {
            min_val = samples[i];
        }
        if (samples[i] > max_val) {
            max_val = samples[i];
        }
    }
    
    // Calculate peak-to-peak amplitude
    int32_t peak_to_peak = max_val - min_val;
    
    // Normalize to 0.0-1.0 range
    float normalized = (float)peak_to_peak / (float)VIBRATION_ADC_MAX_VALUE;
    
    // Clamp to valid range
    if (normalized < 0.0f) {
        normalized = 0.0f;
    } else if (normalized > 1.0f) {
        normalized = 1.0f;
    }
    
    return normalized;
}

esp_err_t vibration_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Vibration sensor already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing vibration sensor...");
    ESP_LOGI(TAG, "GPIO: %d, ADC Channel: %d", VIBRATION_GPIO_PIN, VIBRATION_ADC_CHANNEL);
    ESP_LOGI(TAG, "Sample rate: %d Hz, Duration: %d ms, Buffer: %d samples",
             VIBRATION_SAMPLE_RATE_HZ, VIBRATION_SAMPLE_DURATION_MS, VIBRATION_BUFFER_SIZE);
    
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
    
    ESP_LOGI(TAG, "Reading vibration sensor (sampling for %d ms)...", 
             VIBRATION_SAMPLE_DURATION_MS);
    
    // Collect ADC samples
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
        int64_t next_sample_time = start_time + (sample_count * VIBRATION_SAMPLE_INTERVAL_US);
        int64_t wait_time = next_sample_time - esp_timer_get_time();
        
        if (wait_time > 0) {
            esp_rom_delay_us((uint32_t)wait_time);
        }
    }
    
    data->sample_count = sample_count;
    
    if (sample_count < 10) {
        ESP_LOGE(TAG, "Insufficient samples collected: %lu", sample_count);
        return ESP_FAIL;
    }
    
    ESP_LOGD(TAG, "Collected %lu samples", sample_count);
    
    // Calculate DC offset (baseline)
    int64_t sum = 0;
    for (uint32_t i = 0; i < sample_count; i++) {
        sum += s_sample_buffer[i];
    }
    int32_t baseline = (int32_t)(sum / sample_count);
    
    ESP_LOGD(TAG, "DC baseline: %ld", baseline);
    
    // Calculate amplitude
    data->amplitude = calculate_amplitude(s_sample_buffer, sample_count, baseline);
    
    // Detect vibration based on threshold
    data->vibration_detected = (data->amplitude >= VIBRATION_THRESHOLD);
    
    // Detect zero crossings and calculate frequencies
    int32_t frequencies[VIBRATION_BUFFER_SIZE / 4]; // Max possible frequencies
    uint32_t freq_count = detect_zero_crossings(s_sample_buffer, sample_count, 
                                                 baseline, frequencies, 
                                                 VIBRATION_BUFFER_SIZE / 4);
    
    if (freq_count > 0) {
        // Calculate median frequency for stability
        data->dominant_frequency_hz = (float)calculate_median(frequencies, freq_count);
        
        ESP_LOGI(TAG, "Vibration detected: Freq=%.1f Hz, Amp=%.3f, Samples=%lu, "
                      "Freq count=%lu",
                 data->dominant_frequency_hz, data->amplitude, sample_count, freq_count);
    } else {
        data->dominant_frequency_hz = 0.0f;
        
        if (data->vibration_detected) {
            ESP_LOGW(TAG, "Vibration amplitude detected (%.3f) but no valid frequency found",
                     data->amplitude);
        } else {
            ESP_LOGD(TAG, "No vibration detected (amplitude: %.3f, threshold: %.3f)",
                     data->amplitude, VIBRATION_THRESHOLD);
        }
    }
    
    // Update last vibration time if detected
    if (data->vibration_detected) {
        data->last_vibration_time = esp_timer_get_time() / 1000000; // Convert to seconds
    }
    
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
