/**
 * @file comm_manager.h
 * @brief Communication Manager Service Interface (Placeholder)
 * 
 * Handles WiFi connection and HTTP transmission.
 * Currently placeholder - returns success without implementation.
 */

#ifndef COMM_MANAGER_H
#define COMM_MANAGER_H

#include "esp_err.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize communication manager
 * 
 * Placeholder - returns ESP_OK.
 * 
 * @return ESP_OK (always)
 */
esp_err_t comm_manager_init(void);

/**
 * @brief Send JSON data to server
 * 
 * Placeholder - returns ESP_OK without transmission.
 * Future: WiFi connect, HTTP POST, WiFi disconnect.
 * 
 * @param[in] json_data JSON string to send
 * @param[in] length Length of JSON string
 * 
 * @return ESP_OK (always)
 */
esp_err_t comm_manager_send_json(const char* json_data, size_t length);

/**
 * @brief Deinitialize communication manager
 * 
 * Placeholder - returns ESP_OK.
 * 
 * @return ESP_OK (always)
 */
esp_err_t comm_manager_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // COMM_MANAGER_H
