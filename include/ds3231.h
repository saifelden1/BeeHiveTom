/**
 * @file ds3231.h
 * @brief DS3231 Real-Time Clock Driver Interface
 * 
 * Simple RTC driver for reading and setting time.
 * Provides basic time management for sensor data timestamping.
 * 
 * FEATURES:
 * - Read current time (structured or Unix timestamp)
 * - Set time from Unix timestamp
 * - I2C communication using esp-idf-lib component
 * - Deep sleep compatible
 * 
 * USAGE:
 * 1. Call rtc_init() once after wake-up
 * 2. Call rtc_get_timestamp() to get Unix timestamp for sensor data
 * 3. Call rtc_set_time() to update from server (when WiFi connects)
 * 4. Call rtc_deinit() before deep sleep
 */

#ifndef DS3231_H
#define DS3231_H

#include "datatypes.h"
#include "esp_err.h"

/**
 * @brief Initialize DS3231 RTC
 * 
 * Configures I2C for DS3231 communication.
 * Uses DS3231_I2C_ADDR from config.h.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t rtc_init(void);

/**
 * @brief Read current time from RTC
 * 
 * Reads the current date and time from DS3231.
 * Returns structured time (year, month, day, hour, minute, second).
 * 
 * @param time Pointer to rtc_time_t structure to populate
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if time is NULL,
 *         ESP_ERR_INVALID_STATE if not initialized,
 *         ESP_FAIL on communication error
 */
esp_err_t rtc_get_time(rtc_time_t* time);

/**
 * @brief Get Unix timestamp from RTC
 * 
 * Reads current time and converts to Unix timestamp (seconds since 1970).
 * Use this for sensor data timestamping.
 * 
 * @param timestamp Pointer to uint64_t to store timestamp
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if timestamp is NULL,
 *         ESP_ERR_INVALID_STATE if not initialized,
 *         ESP_FAIL on communication error
 */
esp_err_t rtc_get_timestamp(uint64_t* timestamp);

/**
 * @brief Set RTC time from Unix timestamp
 * 
 * Sets the DS3231 time from Unix timestamp (seconds since 1970).
 * Use this to update RTC when server time is received.
 * 
 * @param timestamp Unix timestamp (seconds since 1970-01-01 00:00:00 UTC)
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if timestamp is invalid,
 *         ESP_ERR_INVALID_STATE if not initialized,
 *         ESP_FAIL on communication error
 */
esp_err_t rtc_set_time(uint64_t timestamp);

/**
 * @brief Deinitialize RTC
 * 
 * Releases I2C resources before deep sleep.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t rtc_deinit(void);

#endif // DS3231_H
