/**
 * @file bme680.h
 * @brief BME680 Environmental Sensor Driver (HAL Layer)
 * 
 * Wraps esp-idf-lib BME680 component with project-specific interface.
 * Provides temperature, humidity, pressure, gas resistance, CO2 approximation,
 * and Indoor Air Quality (IAQ) index measurements.
 * 
 * Key Features:
 * - Internal I2C initialization using configurable address from config.h
 * - Gas sensor heater management for VOC measurements
 * - CO2 approximation from gas resistance
 * - IAQ index calculation (0-500 range)
 * - Automatic calibration baseline management
 * - Error handling and validation
 */

#ifndef BME680_H
#define BME680_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize BME680 sensor
 * 
 * Initializes own I2C bus internally using BME680_I2C_ADDR from config.h.
 * Configures sensor operating mode, oversampling, and gas heater settings.
 * 
 * Configuration:
 * - I2C address: BME680_I2C_ADDR (0x76 or 0x77 from config.h)
 * - I2C frequency: I2C_MASTER_FREQ_HZ (400kHz)
 * - Oversampling: x2 for temperature, humidity, pressure
 * - Gas heater: 320°C for 150ms
 * 
 * @return ESP_OK on success
 * @return ESP_ERR_NOT_FOUND if sensor not detected
 * @return ESP_ERR_INVALID_STATE if I2C initialization fails
 * @return ESP_FAIL on other errors
 */
esp_err_t bme680_init(void);

/**
 * @brief Read all sensor data from BME680
 * 
 * Reads temperature, humidity, and gas resistance.
 * Calculates CO2 approximation from gas resistance.
 * 
 * @param[out] temperature_c Temperature in Celsius
 * @param[out] humidity_percent Relative humidity percentage
 * @param[out] co2_ppm CO2 approximation in ppm
 * 
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if any pointer is NULL
 * @return ESP_ERR_INVALID_STATE if sensor not initialized
 * @return ESP_FAIL if sensor read fails
 */
esp_err_t bme680_read(float* temperature_c, float* humidity_percent, uint16_t* co2_ppm);

/**
 * @brief Trigger gas sensor measurement
 * 
 * Activates gas sensor heater and waits for warm-up completion.
 * Must be called before bme680_read() to get valid gas resistance data.
 * 
 * Timing:
 * - Heater activation: immediate
 * - Warm-up time: ~20-30ms (BME680_HEATER_DURATION from config.h)
 * 
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_STATE if sensor not initialized
 * @return ESP_FAIL if heater activation fails
 */
esp_err_t bme680_trigger_gas_measurement(void);

/**
 * @brief Check if gas sensor is ready
 * 
 * Checks if gas sensor warm-up is complete and data is ready to read.
 * 
 * @return true if gas sensor is ready
 * @return false if warm-up in progress or sensor not initialized
 */
bool bme680_gas_ready(void);

/**
 * @brief Deinitialize BME680 sensor
 * 
 * Releases I2C resources and resets sensor state.
 * 
 * @return ESP_OK on success
 * @return ESP_FAIL on error
 */
esp_err_t bme680_deinit(void);

/**
 * @brief Take a complete measurement (convenience function)
 * 
 * This function combines trigger, wait, and read operations:
 * 1. Triggers gas measurement with heater
 * 2. Waits for measurement completion (~200ms)
 * 3. Reads all sensor data
 * 4. Returns results
 * 
 * @param[out] temperature_c Temperature in Celsius
 * @param[out] humidity_percent Relative humidity percentage
 * @param[out] co2_ppm CO2 approximation in ppm
 * 
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if any pointer is NULL
 * @return ESP_ERR_INVALID_STATE if sensor not initialized
 * @return ESP_ERR_TIMEOUT if measurement times out
 * @return ESP_FAIL if sensor read fails
 * 
 * @note This function blocks for approximately 200-250ms
 * @note Suitable for power-optimized wake-measure-sleep cycles
 */
esp_err_t bme680_measure_and_read(float* temperature_c, float* humidity_percent, uint16_t* co2_ppm);

#ifdef __cplusplus
}
#endif

#endif // BME680_H
