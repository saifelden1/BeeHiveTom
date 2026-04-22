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
 * Contains all sensor data for storage and transmission:
 * - timestamp: Unix time from RTC
 * - temperature_c: Temperature in Celsius from BME680
 * - humidity_percent: Relative humidity from BME680
 * - co2_ppm: CO2 concentration from BME680 gas sensor
 * - vibration_amplitude: Piezo sensor vibration intensity (normalized 0.0-1.0)
 * - battery_level: Battery percentage (0-100)
 */
typedef struct {
    uint64_t timestamp;           // Unix timestamp from RTC
    float temperature_c;          // Temperature in Celsius (-40 to 85°C)
    float humidity_percent;       // Relative humidity (0-100%)
    uint16_t co2_ppm;            // CO2 in ppm (400-5000)
    float vibration_amplitude;    // Vibration intensity (0.0-1.0 normalized)
    uint8_t battery_level;       // Battery level percentage (0-100)
    bool valid;                  // Overall reading validity
} sensor_reading_t;

#endif // DATATYPES_H
