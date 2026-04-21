/**
 * @file vibration.h
 * @brief Piezo Vibration Sensor Driver Interface
 * 
 * Simple analog piezo sensor driver with ADC averaging.
 * Takes multiple ADC samples and returns the average value.
 * 
 * FEATURES:
 * - Blocking ADC sampling at configurable rate (default: 100 Hz, 10ms intervals)
 * - Configurable sampling duration (default: 500ms, 50 samples)
 * - Returns average ADC value (0-4095 range)
 * - Deep sleep compatible
 * 
 * USAGE:
 * 1. Call vibration_init() once after wake-up
 * 2. Call vibration_read() to get average vibration reading
 * 3. Call vibration_deinit() before deep sleep
 */

#ifndef VIBRATION_H
#define VIBRATION_H

#include "datatypes.h"
#include "esp_err.h"

/**
 * @brief Initialize vibration sensor
 * 
 * Configures ADC1 for piezo sensor reading:
 * - 12-bit resolution (0-4095)
 * - 11dB attenuation (0-3.3V range)
 * - GPIO pin from config.h
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t vibration_init(void);

/**
 * @brief Read vibration sensor data
 * 
 * Performs blocking ADC sampling for configured duration (default: 500ms).
 * Takes readings at configured intervals (default: 10ms).
 * Returns the average of all ADC readings.
 * 
 * @param data Pointer to vibration_data_t structure to populate
 *             - dominant_frequency_hz: Average ADC value (0-4095)
 *             - sample_count: Number of samples collected
 *             - last_vibration_time: Timestamp of reading
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if data is NULL,
 *         ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t vibration_read(vibration_data_t* data);

/**
 * @brief Deinitialize vibration sensor
 * 
 * Releases ADC resources before deep sleep.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t vibration_deinit(void);

#endif // VIBRATION_H
