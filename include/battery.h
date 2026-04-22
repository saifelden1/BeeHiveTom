/**
 * @file battery.h
 * @brief Battery Level HAL Interface
 * 
 * Reads battery voltage via ADC and converts to percentage.
 * Single ADC read per call, no averaging.
 */

#ifndef BATTERY_H
#define BATTERY_H

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize battery level monitoring
 * 
 * Configures ADC1 for battery voltage reading.
 * 
 * @return ESP_OK on success
 * @return ESP_FAIL on ADC configuration error
 */
esp_err_t battery_init(void);

/**
 * @brief Read battery level as percentage
 * 
 * Performs single ADC read, converts to voltage, calculates percentage.
 * 
 * @param[out] level_percent Battery level (0-100%)
 * 
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if level_percent is NULL
 * @return ESP_FAIL on ADC read error
 */
esp_err_t battery_read_level(uint8_t* level_percent);

/**
 * @brief Deinitialize battery monitoring
 * 
 * Releases ADC resources.
 * 
 * @return ESP_OK on success
 */
esp_err_t battery_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // BATTERY_H
