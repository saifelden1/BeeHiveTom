/**
 * @file vibration.h
 * @brief Piezo Vibration Sensor Driver Interface
 * 
 * Simple analog piezo sensor driver with ADC averaging.
 * Measures vibration amplitude (intensity) from piezo voltage.
 * 
 * SENSOR: Piezo film sensor with mass attachment
 * - Generates AC voltage (up to ±90V) when vibrating
 * - Voltage reduced to ADC levels (0-3.3V) via resistor divider
 * - Higher vibration = higher voltage = higher ADC reading
 * 
 * FEATURES:
 * - Blocking ADC sampling at configurable rate (default: 100 Hz)
 * - Configurable sampling duration (default: 500ms, 50 samples)
 * - Returns normalized vibration amplitude (0.0-1.0)
 * - Deep sleep compatible
 * 
 * USAGE:
 * 1. Call vibration_init() once after wake-up
 * 2. Call vibration_read() to get vibration amplitude
 * 3. Call vibration_deinit() before deep sleep
 */

#ifndef VIBRATION_H
#define VIBRATION_H

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
 * @brief Read vibration sensor amplitude
 * 
 * Performs blocking ADC sampling for configured duration (default: 500ms).
 * Takes readings at configured intervals (default: 10ms).
 * Returns normalized vibration amplitude (0.0-1.0).
 * 
 * @param[out] amplitude Normalized vibration amplitude (0.0 = no vibration, 1.0 = max)
 * 
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if amplitude is NULL
 * @return ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t vibration_read(float* amplitude);

/**
 * @brief Deinitialize vibration sensor
 * 
 * Releases ADC resources before deep sleep.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t vibration_deinit(void);

#endif // VIBRATION_H
