/**
 * @file data_manager.h
 * @brief Data Manager Service Layer Interface
 * 
 * Manages persistent storage of sensor readings in NVS (Non-Volatile Storage).
 * Provides JSON formatting for transmission and batch operations.
 * 
 * FEATURES:
 * - Unlimited NVS storage (no circular buffer, stores until sent)
 * - Sequential key storage: "reading_0", "reading_1", etc.
 * - Batch JSON formatting with configurable format (config.h)
 * - Static buffer allocation (no heap fragmentation)
 * - Automatic retry on NVS write failures
 * - Batch deletion after successful transmission
 * 
 * WORKFLOW:
 * 1. Store readings when WiFi unavailable
 * 2. Format all readings as JSON when WiFi available
 * 3. Send JSON payload via comm_manager
 * 4. Clear all readings after successful transmission
 * 
 * USAGE:
 * 1. Call data_manager_init() once at startup
 * 2. Call data_manager_store_reading() after each sensor read
 * 3. Call data_manager_get_count() to check pending readings
 * 4. Call data_manager_format_json() before transmission
 * 5. Call data_manager_clear() after successful transmission
 */

#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include "datatypes.h"
#include "esp_err.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize data manager
 * 
 * Initializes NVS namespace for sensor data storage.
 * Creates namespace if it doesn't exist.
 * Loads metadata (reading count) from NVS.
 * 
 * @return ESP_OK on success
 * @return ESP_ERR_NVS_NOT_INITIALIZED if NVS not initialized
 * @return ESP_ERR_NO_MEM if memory allocation fails
 * @return ESP_FAIL on other errors
 */
esp_err_t data_manager_init(void);

/**
 * @brief Store sensor reading in NVS
 * 
 * Stores reading with sequential key: "reading_0", "reading_1", etc.
 * Updates metadata with new count.
 * Retries on failure (NVS_WRITE_MAX_RETRIES from config.h).
 * Skips and logs error if all retries fail.
 * 
 * @param[in] reading Pointer to sensor_reading_t to store
 * 
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if reading is NULL or invalid
 * @return ESP_ERR_NVS_NOT_ENOUGH_SPACE if NVS partition full
 * @return ESP_FAIL if write fails after retries (reading skipped)
 * 
 * @note If write fails after retries, error is logged and ESP_FAIL returned.
 *       System continues operation (reading is lost).
 */
esp_err_t data_manager_store_reading(const sensor_reading_t* reading);

/**
 * @brief Get count of stored readings
 * 
 * Returns number of readings currently stored in NVS.
 * 
 * @return Number of stored readings (0 if none or error)
 */
uint32_t data_manager_get_count(void);

/**
 * @brief Format all stored readings as JSON
 * 
 * Reads all stored readings from NVS and formats as JSON string.
 * Uses configurable format from config.h (JSON_FORMAT_TYPE).
 * Uses static buffer to avoid heap fragmentation.
 * 
 * JSON structure (standard format):
 * {
 *   "device_id": "esp32_sensor_001",
 *   "readings": [
 *     {
 *       "timestamp": 1234567890,
 *       "temperature_c": 25.5,
 *       "humidity_percent": 60.0,
 *       "pressure_hpa": 1013.25,
 *       "gas_resistance_ohms": 50000.0,
 *       "co2_ppm": 450,
 *       "iaq_index": 50,
 *       "vibration_frequency_hz": 0.0,
 *       "vibration_amplitude": 0.0,
 *       "battery_level": 100
 *     }
 *   ]
 * }
 * 
 * @param[out] buffer Pointer to buffer to store JSON string
 * @param[in] size Size of buffer in bytes
 * 
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if buffer is NULL or size is 0
 * @return ESP_ERR_INVALID_SIZE if buffer too small for JSON
 * @return ESP_ERR_NOT_FOUND if no readings stored
 * @return ESP_FAIL on JSON formatting error
 * 
 * @note Buffer should be at least JSON_BUFFER_SIZE bytes (from config.h)
 * @note If readings exceed JSON_MAX_READINGS_PER_BATCH, only first batch formatted
 */
esp_err_t data_manager_format_json(char* buffer, size_t size);

/**
 * @brief Clear all stored readings from NVS
 * 
 * Deletes all reading keys and resets metadata.
 * Call after successful transmission to free NVS space.
 * 
 * @return ESP_OK on success
 * @return ESP_FAIL on error
 * 
 * @note This operation cannot be undone
 */
esp_err_t data_manager_clear(void);

/**
 * @brief Check if NVS storage is full
 * 
 * Checks available NVS space.
 * Returns true if insufficient space for new readings.
 * 
 * @return true if NVS partition full
 * @return false if space available
 * 
 * @note This is a best-effort check. Actual write may still fail.
 */
bool data_manager_is_full(void);

/**
 * @brief Get specific reading by index
 * 
 * Retrieves a specific reading from NVS by index.
 * Useful for debugging or partial transmission.
 * 
 * @param[in] index Reading index (0 to count-1)
 * @param[out] reading Pointer to sensor_reading_t to populate
 * 
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if reading is NULL or index out of range
 * @return ESP_ERR_NOT_FOUND if reading not found
 * @return ESP_FAIL on read error
 */
esp_err_t data_manager_get_reading(uint32_t index, sensor_reading_t* reading);

/**
 * @brief Deinitialize data manager
 * 
 * Closes NVS handle and releases resources.
 * Call before deep sleep or system shutdown.
 * 
 * @return ESP_OK on success
 */
esp_err_t data_manager_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // DATA_MANAGER_H
