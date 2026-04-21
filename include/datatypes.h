/**
 * @file datatypes.h
 * @brief System-wide data type definitions for Sensor Data Collection System
 * 
 * This file contains ALL data structures and enumerations used across the system.
 * All components must include this header for type definitions.
 */

#ifndef DATATYPES_H
#define DATATYPES_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// STATE MACHINE TYPES
// ============================================================================

/**
 * @brief System state machine states
 * 
 * Sequential execution order:
 * INIT → READ_SENSORS → STORE_DATA → TRANSMIT_CHECK → 
 * TRANSMIT_DATA (conditional) → PREPARE_SLEEP → SLEEP
 */
typedef enum {
    STATE_INIT,              // Initialize all system components
    STATE_READ_SENSORS,      // Read all sensors sequentially
    STATE_STORE_DATA,        // Store reading in NVS
    STATE_TRANSMIT_CHECK,    // Check if transmission cycle reached
    STATE_TRANSMIT_DATA,     // WiFi connect, send data, disconnect
    STATE_PREPARE_SLEEP,     // Disable peripherals to save power
    STATE_SLEEP              // Enter deep sleep mode
} system_state_t;

// ============================================================================
// SENSOR DATA TYPES
// ============================================================================

/**
 * @brief BME680 environmental sensor data
 * 
 * Contains temperature, humidity, pressure, gas resistance, CO2 approximation,
 * and Indoor Air Quality (IAQ) index.
 */
typedef struct {
    float temperature_c;          // Temperature in Celsius (-40 to 85°C)
    float humidity_percent;       // Relative humidity (0-100%)
    float pressure_hpa;           // Atmospheric pressure in hPa (300-1100 hPa)
    float gas_resistance_ohms;    // Gas sensor resistance in Ohms (> 0)
    uint16_t co2_ppm;            // CO2 approximation in ppm (400-5000 ppm)
    uint8_t iaq_index;           // Indoor Air Quality index (0-500)
    bool valid;                  // Data validity flag
} bme680_data_t;

/**
 * @brief Vibration sensor data from piezo sensor
 * 
 * Measured via ADC sampling at 1kHz with zero-crossing frequency detection.
 */
typedef struct {
    float dominant_frequency_hz;  // Measured vibration frequency (10-500 Hz)
    float amplitude;              // Vibration intensity (0.0-1.0 normalized)
    uint32_t sample_count;        // Number of ADC samples analyzed
    bool vibration_detected;      // True if vibration detected above threshold
    uint64_t last_vibration_time; // Timestamp of last detection (Unix time)
} vibration_data_t;

/**
 * @brief RTC time structure
 * 
 * Used for DS3231 Real-Time Clock time representation.
 */
typedef struct {
    uint16_t year;    // Year (e.g., 2024)
    uint8_t month;    // Month (1-12)
    uint8_t day;      // Day of month (1-31)
    uint8_t hour;     // Hour (0-23)
    uint8_t minute;   // Minute (0-59)
    uint8_t second;   // Second (0-59)
} rtc_time_t;

/**
 * @brief Complete sensor reading with timestamp
 * 
 * Aggregates all sensor data for a single wake cycle.
 */
typedef struct {
    uint64_t timestamp;           // Unix timestamp from RTC
    bme680_data_t bme680;        // BME680 environmental data
    vibration_data_t vibration;  // Vibration sensor data
    uint8_t battery_level;       // Battery level percentage (0-100, future)
    bool valid;                  // Overall reading validity
} sensor_reading_t;

// ============================================================================
// CONFIGURATION TYPES
// ============================================================================

/**
 * @brief WiFi configuration
 */
typedef struct {
    char ssid[32];              // WiFi SSID
    char password[64];          // WiFi password
    char server_url[128];       // Server URL (localhost for testing)
    uint16_t timeout_ms;        // Connection timeout in milliseconds
} wifi_config_t;

/**
 * @brief Sleep configuration
 */
typedef struct {
    uint32_t sleep_duration_minutes;  // Sleep duration (default: 15 minutes)
    bool use_rtc_wakeup;             // Use RTC timer for wake-up
    bool use_ext_wakeup;             // Use external GPIO for wake-up
    uint8_t ext_wakeup_pin;          // GPIO pin for external wake-up
} sleep_config_t;

/**
 * @brief System configuration
 * 
 * Complete system configuration including WiFi, sleep, and operational parameters.
 */
typedef struct {
    wifi_config_t wifi;                    // WiFi configuration
    sleep_config_t sleep;                  // Sleep configuration
    uint16_t reading_interval_minutes;     // Sensor reading interval (15)
    uint16_t transmission_interval_count;  // Readings before transmission (4)
    uint16_t max_stored_readings;          // Maximum readings in buffer (1000)
    char device_id[32];                    // Unique device identifier
} system_config_t;

// ============================================================================
// SENSOR DRIVER INTERFACE
// ============================================================================

/**
 * @brief Generic sensor driver interface
 * 
 * Provides uniform interface for all sensor drivers.
 */
typedef struct {
    esp_err_t (*init)(void);      // Initialize sensor
    esp_err_t (*read)(void* data); // Read sensor data
    esp_err_t (*deinit)(void);    // Deinitialize sensor
    const char* name;             // Sensor name for logging
} sensor_driver_t;

#endif // DATATYPES_H
