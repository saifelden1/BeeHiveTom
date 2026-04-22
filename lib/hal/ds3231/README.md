# DS3231 Real-Time Clock Driver

Simple RTC driver for reading and setting time on DS3231 chip.

## Features
- Read current date/time (structured or Unix timestamp)
- Set time from Unix timestamp
- I2C communication using esp-idf-lib component
- Deep sleep compatible (RTC keeps time with battery backup)

## Hardware
- **Chip**: DS3231 RTC
- **Interface**: I2C
- **Address**: 0x68 (fixed)
- **Pins**: SDA=GPIO21, SCL=GPIO22
- **Battery**: CR2032 (keeps time during power off)

## Usage

### Basic Usage (Every 15-min Wake Cycle)
```c
#include "ds3231.h"

void app_main(void)
{
    // Initialize RTC
    rtc_init();
    
    // Get timestamp for sensor data
    uint64_t timestamp;
    rtc_get_timestamp(&timestamp);
    
    // Use timestamp for sensor readings
    sensor_reading_t reading;
    reading.timestamp = timestamp;
    bme680_read(&reading.bme680);
    vibration_read(&reading.vibration);
    
    // Try to send data
    if (wifi_connect_and_send() == ESP_OK) {
        // Update RTC from server response
        uint64_t server_timestamp = get_server_timestamp();
        rtc_set_time(server_timestamp);
    }
    
    // Cleanup and sleep
    rtc_deinit();
    esp_deep_sleep(15 * 60 * 1000000ULL);
}
```

### Get Structured Time
```c
// If you need year/month/day/hour/min/sec separately
rtc_time_t time;
rtc_get_time(&time);

ESP_LOGI("APP", "Time: %04d-%02d-%02d %02d:%02d:%02d",
         time.year, time.month, time.day,
         time.hour, time.minute, time.second);
```

### Set Time from Server
```c
// When WiFi connects and server responds with current time
uint64_t server_timestamp = 1737820800;  // Example: 2025-01-25 12:00:00 UTC
rtc_set_time(server_timestamp);
```

## API Reference

### `rtc_init()`
Initialize DS3231 RTC and I2C communication.

**Returns**: `ESP_OK` on success

### `rtc_get_time(rtc_time_t* time)`
Read current time as structured data.

**Output**:
```c
typedef struct {
    uint16_t year;    // Year (e.g., 2025)
    uint8_t month;    // Month (1-12)
    uint8_t day;      // Day (1-31)
    uint8_t hour;     // Hour (0-23)
    uint8_t minute;   // Minute (0-59)
    uint8_t second;   // Second (0-59)
} rtc_time_t;
```

### `rtc_get_timestamp(uint64_t* timestamp)`
Read current time as Unix timestamp (seconds since 1970).

**Use**: For sensor data timestamping

### `rtc_set_time(uint64_t timestamp)`
Set RTC time from Unix timestamp.

**Input**: Unix timestamp (seconds since 1970-01-01 00:00:00 UTC)

**Use**: Update RTC when server time is received

### `rtc_deinit()`
Release I2C resources before deep sleep.

## Configuration
```c
// config.h
#define DS3231_I2C_ADDR         0x68    // Fixed I2C address
#define I2C_MASTER_FREQ_HZ      400000  // 400kHz
#define I2C_MASTER_TIMEOUT_MS   1000    // 1 second timeout
```

## Notes
- DS3231 has fixed I2C address (0x68)
- Battery backup keeps time during deep sleep
- Uses 24-hour time format
- No calibration needed (syncs from server when WiFi connects)
- Temperature sensor and calibration features removed (not needed for this use case)
