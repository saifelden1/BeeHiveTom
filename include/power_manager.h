/**
 * @file power_manager.h
 * @brief Power Manager Service Interface
 * 
 * Manages deep sleep and peripheral power control.
 */

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize power manager
 * 
 * Configures deep sleep wake sources.
 * 
 * @return ESP_OK on success
 * @return ESP_FAIL on configuration error
 */
esp_err_t power_manager_init(void);

/**
 * @brief Enter deep sleep
 * 
 * Deinitializes all sensors, disables peripherals, enters deep sleep.
 * Sleep duration configured in config.h (SLEEP_DURATION_SEC).
 * 
 * @return ESP_OK on success (never returns if sleep successful)
 * @return ESP_FAIL if sleep entry fails
 */
esp_err_t power_manager_sleep(void);

#ifdef __cplusplus
}
#endif

#endif // POWER_MANAGER_H
