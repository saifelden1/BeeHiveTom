/**
 * @file config.h
 * @brief System-wide configuration constants for Sensor Data Collection System
 * 
 * This file contains ALL configuration constants including I2C addresses,
 * sensor parameters, system intervals, and device settings.
 * 
 * CRITICAL: All I2C addresses MUST be configurable here.
 * DO NOT hardcode I2C addresses in driver implementations.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// ============================================================================
// I2C CONFIGURATION
// ============================================================================

/**
 * @brief BME680 I2C address
 * 
 * BME680 supports two I2C addresses based on SDO pin:
 * - 0x76: SDO connected to GND (default)
 * - 0x77: SDO connected to VCC
 */
#define BME680_I2C_ADDR         0x76

/**
 * @brief DS3231 RTC I2C address
 * 
 * DS3231 has a fixed I2C address (typically 0x68)
 */
#define DS3231_I2C_ADDR         0x68

/**
 * @brief I2C bus configuration
 */
#define I2C_MASTER_FREQ_HZ      400000  // 400kHz fast mode
#define I2C_MASTER_TIMEOUT_MS   1000    // 1 second timeout

// ============================================================================
// BME680 SENSOR CONFIGURATION
// ============================================================================

/**
 * @brief BME680 gas sensor heater configuration
 * 
 * Gas sensor requires heater activation for VOC/gas measurements.
 * Heater temperature: 320°C
 * Heater duration: 150ms
 */
#define BME680_HEATER_TEMP      320     // Heater temperature in Celsius
#define BME680_HEATER_DURATION  150     // Heater duration in milliseconds

/**
 * @brief BME680 gas sensor calibration
 */
#define BME680_CALIBRATION_TIME_MS  300000  // 5 minutes calibration time

/**
 * @brief BME680 CO2 approximation parameters
 * 
 * CO2 estimation from gas resistance using linear model:
 * CO2_ppm = BASELINE_CO2 + (REFERENCE_RESISTANCE - gas_resistance) / SENSITIVITY
 * 
 * Note: This is an approximation. BME680 measures VOCs, not CO2 directly.
 * For accurate CO2 measurement, use dedicated NDIR CO2 sensor.
 */
#define BME680_CO2_BASELINE_PPM     450     // Baseline CO2 level (typical outdoor)
#define BME680_CO2_REFERENCE_OHM    100000  // Reference gas resistance (clean air)
#define BME680_CO2_SENSITIVITY      1000    // Sensitivity factor (Ohms per ppm)

/**
 * @brief BME680 oversampling settings
 */
#define BME680_TEMP_OVERSAMPLE  2       // Temperature oversampling (x2)
#define BME680_HUM_OVERSAMPLE   2       // Humidity oversampling (x2)
#define BME680_PRES_OVERSAMPLE  2       // Pressure oversampling (x2)

// ============================================================================
// VIBRATION SENSOR CONFIGURATION
// ============================================================================

/**
 * @brief Vibration sensor ADC configuration
 * 
 * Piezo vibration sensor connected to ADC1 channel.
 * ADC configuration: 12-bit resolution, 11dB attenuation (0-3.3V range)
 */
#define VIBRATION_GPIO_PIN          GPIO_NUM_36     // GPIO36 (ADC1_CHANNEL_0)
#define VIBRATION_ADC_CHANNEL       ADC1_CHANNEL_0  // ADC1 Channel 0
#define VIBRATION_ADC_ATTEN         ADC_ATTEN_DB_11 // 0-3.3V range
#define VIBRATION_ADC_WIDTH         ADC_WIDTH_BIT_12 // 12-bit resolution (0-4095)

// ============================================================================
// BATTERY LEVEL CONFIGURATION
// ============================================================================

/**
 * @brief Battery level ADC configuration
 * 
 * Battery voltage monitoring via ADC with voltage divider.
 * ADC configuration: 12-bit resolution, 11dB attenuation (0-3.3V range)
 */
#define BATTERY_GPIO_PIN            GPIO_NUM_35     // GPIO35 (ADC1_CHANNEL_7)
#define BATTERY_ADC_CHANNEL         ADC1_CHANNEL_7  // ADC1 Channel 7
#define BATTERY_ADC_ATTEN           ADC_ATTEN_DB_11 // 0-3.3V range
#define BATTERY_ADC_WIDTH           ADC_WIDTH_BIT_12 // 12-bit resolution (0-4095)

/**
 * @brief Battery voltage specifications
 * 
 * Voltage divider: 2:1 ratio (R1=100kΩ, R2=100kΩ)
 * Max battery voltage: 4.2V → 2.1V at ADC
 * Min battery voltage: 3.0V → 1.5V at ADC
 */
#define BATTERY_MAX_VOLTAGE_MV      4200    // Maximum battery voltage (mV)
#define BATTERY_MIN_VOLTAGE_MV      3000    // Minimum battery voltage (mV)
#define BATTERY_VOLTAGE_DIVIDER     2       // Voltage divider ratio (2:1)

/**
 * @brief Battery ADC reference
 */
#define BATTERY_ADC_MAX_VALUE       4095    // 12-bit ADC maximum value
#define BATTERY_VREF_MV             3300    // Reference voltage in millivolts

/**
 * @brief Vibration sensor sampling configuration
 * 
 * Simple ADC sampling with averaging for piezo sensor:
 * - Sample duration: 500ms (configurable)
 * - Sample rate: 100 Hz (10ms interval between readings)
 * - Returns normalized amplitude (0.0-1.0)
 * 
 * Piezo sensor generates AC voltage proportional to vibration intensity.
 * Higher vibration = higher voltage = higher ADC reading.
 */
#define VIBRATION_SAMPLE_DURATION_MS    500     // Sampling duration in milliseconds
#define VIBRATION_SAMPLE_RATE_HZ        100     // Sampling rate in Hz (100Hz = 10ms interval)
#define VIBRATION_SAMPLE_INTERVAL_MS    10      // 10ms between readings
#define VIBRATION_BUFFER_SIZE           (VIBRATION_SAMPLE_RATE_HZ * VIBRATION_SAMPLE_DURATION_MS / 1000) // 50 samples

/**
 * @brief Vibration amplitude normalization
 */
#define VIBRATION_ADC_MAX_VALUE     4095    // 12-bit ADC maximum value (for normalization)

// ============================================================================
// SYSTEM TIMING CONFIGURATION
// ============================================================================

/**
 * @brief Wake and sleep intervals
 */
#define READING_INTERVAL_MIN        15      // Sensor reading interval (minutes)
#define TRANSMISSION_INTERVAL       4       // Readings before transmission
#define SLEEP_DURATION_SEC          (READING_INTERVAL_MIN * 60)  // 900 seconds

/**
 * @brief Operation timeouts
 */
#define SENSOR_READ_TIMEOUT_MS      5000    // 5 seconds for all sensors
#define WIFI_CONNECT_TIMEOUT_MS     10000   // 10 seconds WiFi connection
#define HTTP_TRANSMIT_TIMEOUT_MS    10000   // 10 seconds HTTP transmission
#define MAX_ACTIVE_TIME_MS          30000   // 30 seconds max active time

// ============================================================================
// DATA STORAGE CONFIGURATION
// ============================================================================

/**
 * @brief NVS (Non-Volatile Storage) configuration
 */
#define NVS_NAMESPACE               "sensor_data"   // NVS namespace
#define NVS_PARTITION_SIZE          16384           // 16KB NVS partition

/**
 * @brief NVS key names
 */
#define NVS_KEY_METADATA            "metadata"      // Metadata key (stores count)
#define NVS_KEY_READING_PREFIX      "reading_"      // Reading key prefix (e.g., "reading_0")

/**
 * @brief NVS retry configuration
 */
#define NVS_WRITE_MAX_RETRIES       3               // Max retries for NVS write
#define NVS_WRITE_RETRY_DELAY_MS    50              // Delay between retries

// ============================================================================
// JSON FORMATTING CONFIGURATION
// ============================================================================

/**
 * @brief JSON buffer sizing
 * 
 * Static buffer allocation to avoid heap fragmentation.
 * Single reading: ~120 bytes with minimal keys
 * Buffer sized for batch transmission of accumulated readings.
 */
#define JSON_SINGLE_READING_SIZE    128             // Bytes per reading in JSON (reduced with minimal keys)
#define JSON_MAX_READINGS_PER_BATCH 20              // Max readings per transmission (reduced for memory efficiency)
#define JSON_BUFFER_SIZE            (JSON_SINGLE_READING_SIZE * JSON_MAX_READINGS_PER_BATCH + 256) // ~2.8KB

/**
 * @brief JSON field names (ultra-minimal for bandwidth efficiency)
 * 
 * Minimal keys save bandwidth and reduce transmission time.
 * Configure these to match your server's expected format.
 * 
 * Current mapping:
 * - "id"   → Device identifier
 * - "r"    → Readings array
 * - "ts"   → Timestamp (Unix time)
 * - "temp" → Temperature (°C)
 * - "hum"  → Humidity (%)
 * - "co2"  → CO2 (ppm)
 * - "vib"  → Vibration amplitude (0.0-1.0)
 * - "batt" → Battery level (%)
 */
#define JSON_KEY_DEVICE_ID      "id"
#define JSON_KEY_READINGS       "r"
#define JSON_KEY_TIMESTAMP      "ts"
#define JSON_KEY_TEMP           "temp"
#define JSON_KEY_HUM            "hum"
#define JSON_KEY_CO2            "co2"
#define JSON_KEY_VIB            "vib"
#define JSON_KEY_BATTERY        "batt"

// ============================================================================
// WIFI CONFIGURATION
// ============================================================================

/**
 * @brief WiFi credentials (placeholder - should be configured at runtime)
 */
#define WIFI_SSID                   "YOUR_WIFI_SSID"
#define WIFI_PASSWORD               "YOUR_WIFI_PASSWORD"

/**
 * @brief Server configuration (localhost for testing)
 */
#define SERVER_URL                  "http://localhost:8080/api/sensor-data"

/**
 * @brief WiFi connection parameters
 */
#define WIFI_MAX_RETRY              3       // Maximum connection retries
#define WIFI_RETRY_DELAY_MS         1000    // Delay between retries

// ============================================================================
// DEVICE CONFIGURATION
// ============================================================================

/**
 * @brief Device identification
 */
#define DEVICE_ID                   "esp32_sensor_001"
#define FIRMWARE_VERSION            "1.0.0"

/**
 * @brief Hardware configuration
 */
#define BOARD_NAME                  "Freenove ESP32 WROVER"
#define POWER_SUPPLY_VOLTAGE        3.3f    // 3.3V regulated power

// ============================================================================
// POWER MANAGEMENT CONFIGURATION
// ============================================================================

/**
 * @brief Deep sleep configuration
 */
#define DEEP_SLEEP_WAKEUP_TIME_US   (SLEEP_DURATION_SEC * 1000000ULL)
#define DEEP_SLEEP_CURRENT_UA       10      // Target deep sleep current (µA)
#define ACTIVE_CURRENT_MA           200     // Active phase current (mA)

/**
 * @brief Power consumption targets
 */
#define TARGET_AVG_CURRENT_MA       10      // 24-hour average current target

// ============================================================================
// ERROR HANDLING CONFIGURATION
// ============================================================================

/**
 * @brief Sensor error handling
 */
#define SENSOR_MAX_RETRIES          3       // Max consecutive failures before disable
#define SENSOR_RETRY_DELAY_MS       100     // Delay between sensor retries

/**
 * @brief Transmission error handling
 */
#define TRANSMISSION_MAX_RETRIES    3       // Max transmission retries
#define TRANSMISSION_BACKOFF_MS     1000    // Exponential backoff base delay

// ============================================================================
// VALIDATION RANGES
// ============================================================================

/**
 * @brief BME680 sensor value ranges
 */
#define BME680_TEMP_MIN_C           -40.0f  // Minimum temperature
#define BME680_TEMP_MAX_C           85.0f   // Maximum temperature
#define BME680_HUM_MIN_PCT          0.0f    // Minimum humidity
#define BME680_HUM_MAX_PCT          100.0f  // Maximum humidity
#define BME680_PRES_MIN_HPA         300.0f  // Minimum pressure
#define BME680_PRES_MAX_HPA         1100.0f // Maximum pressure
#define BME680_CO2_MIN_PPM          400     // Minimum CO2 (typical indoor)
#define BME680_CO2_MAX_PPM          5000    // Maximum CO2 (typical indoor)
#define BME680_IAQ_MIN              0       // Minimum IAQ index
#define BME680_IAQ_MAX              500     // Maximum IAQ index

/**
 * @brief Vibration sensor value ranges
 */
#define VIBRATION_FREQ_MIN_HZ       0.0f    // Minimum frequency (no vibration)
#define VIBRATION_FREQ_MAX_HZ       500.0f  // Maximum frequency
#define VIBRATION_AMP_MIN           0.0f    // Minimum amplitude
#define VIBRATION_AMP_MAX           1.0f    // Maximum amplitude (normalized)

/**
 * @brief Timestamp validation
 */
#define TIMESTAMP_MIN_YEAR          2020    // Minimum valid year

// ============================================================================
// LOGGING CONFIGURATION
// ============================================================================

/**
 * @brief Log tags for different components
 */
#define LOG_TAG_MAIN                "MAIN"
#define LOG_TAG_BME680              "BME680"
#define LOG_TAG_VIBRATION           "VIBRATION"
#define LOG_TAG_RTC                 "RTC"
#define LOG_TAG_DATA_MGR            "DATA_MGR"
#define LOG_TAG_COMM_MGR            "COMM_MGR"
#define LOG_TAG_POWER_MGR           "POWER_MGR"

#endif // CONFIG_H
