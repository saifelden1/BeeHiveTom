/**
 * @file vibration.h
 * @brief Piezo Vibration Sensor Driver Interface
 * 
 * Analog piezo sensor driver for continuous vibration monitoring.
 * Measures vibration frequency and amplitude using ADC sampling.
 * 
 * FEATURES:
 * - Blocking ADC sampling at configurable rate (default: 1kHz)
 * - Median frequency calculation for stable readings
 * - Zero-crossing detection for frequency measurement
 * - Amplitude normalization (0.0-1.0)
 * - Configurable sampling duration (default: 500ms)
 * 
 * DEEP SLEEP COMPATIBLE:
 * - Designed for 15-minute wake/sleep cycles
 * - Quick initialization for fast wake cycles
 * - Minimal power consumption during active time
 * 
 * USAGE:
 * 1. Call vibration_init() once after wake-up
 * 2. Call vibration_read() to get vibration data
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
 * Analyzes samples to extract:
 * - Dominant frequency via zero-crossing detection
 * - Amplitude (normalized 0.0-1.0)
 * - Vibration detection status
 * 
 * Uses median frequency calculation for stable readings under continuous vibration.
 * 
 * @param data Pointer to vibration_data_t structure to populate
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
