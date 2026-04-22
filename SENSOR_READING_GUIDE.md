# Sensor Reading Guide

## Complete Example: Reading All 3 Sensors

### 1. Include Headers

```c
#include "datatypes.h"
#include "bme680.h"
#include "vibration.h"
#include "ds3231.h"
#include "esp_log.h"
```

### 2. Initialize All Sensors (Once at Startup)

```c
void app_main(void)
{
    // Initialize NVS (required for BME680 calibration)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize all sensors
    ESP_ERROR_CHECK(bme680_init());      // BME680 environmental sensor
    ESP_ERROR_CHECK(vibration_init());   // Piezo vibration sensor
    ESP_ERROR_CHECK(rtc_init());         // DS3231 RTC
    
    ESP_LOGI("MAIN", "All sensors initialized");
    
    // Now read sensors...
}
```

### 3. Create Data Structure to Store Readings

```c
// This structure holds ALL sensor data for one reading cycle
sensor_reading_t reading;
```

**Structure breakdown:**
```c
typedef struct {
    uint64_t timestamp;           // From RTC (Unix timestamp)
    env_data_t env;              // From BME680 (temp, humidity, pressure, CO2, IAQ)
    vibration_data_t vibration;  // From piezo sensor (ADC average)
    uint8_t battery_level;       // Future use
    bool valid;                  // Overall validity
} sensor_reading_t;
```

### 4. Read All Sensors Sequentially

```c
void read_all_sensors(sensor_reading_t* reading)
{
    esp_err_t ret;
    
    // Clear the structure
    memset(reading, 0, sizeof(sensor_reading_t));
    
    // ========================================
    // 1. Get timestamp from RTC
    // ========================================
    ret = rtc_get_timestamp(&reading->timestamp);
    if (ret != ESP_OK) {
        ESP_LOGE("MAIN", "Failed to read RTC: %s", esp_err_to_name(ret));
        reading->timestamp = 0;  // Mark as invalid
    }
    
    // ========================================
    // 2. Read BME680 environmental sensor
    // ========================================
    ret = bme680_measure_and_read(&reading->env);
    if (ret != ESP_OK) {
        ESP_LOGE("MAIN", "Failed to read BME680: %s", esp_err_to_name(ret));
        reading->env.valid = false;
    }
    
    // ========================================
    // 3. Read vibration sensor
    // ========================================
    ret = vibration_read(&reading->vibration);
    if (ret != ESP_OK) {
        ESP_LOGE("MAIN", "Failed to read vibration: %s", esp_err_to_name(ret));
        // vibration_data_t doesn't have valid flag, so just log error
    }
    
    // ========================================
    // 4. Mark overall reading as valid
    // ========================================
    reading->valid = (reading->timestamp > 0 && reading->env.valid);
}
```

### 5. Access the Data

```c
void print_sensor_data(const sensor_reading_t* reading)
{
    if (!reading->valid) {
        ESP_LOGW("MAIN", "Invalid sensor reading");
        return;
    }
    
    // ========================================
    // RTC Timestamp
    // ========================================
    ESP_LOGI("MAIN", "Timestamp: %llu (Unix time)", reading->timestamp);
    
    // ========================================
    // BME680 Environmental Data
    // ========================================
    ESP_LOGI("MAIN", "=== BME680 Environmental Sensor ===");
    ESP_LOGI("MAIN", "  Temperature:    %.2f °C", reading->env.temperature_c);
    ESP_LOGI("MAIN", "  Humidity:       %.2f %%", reading->env.humidity_percent);
    ESP_LOGI("MAIN", "  Pressure:       %.2f hPa", reading->env.pressure_hpa);
    ESP_LOGI("MAIN", "  Gas Resistance: %.0f Ohms", reading->env.gas_resistance_ohms);
    ESP_LOGI("MAIN", "  CO2 (approx):   %d ppm", reading->env.co2_ppm);
    ESP_LOGI("MAIN", "  IAQ Index:      %d (0=excellent, 500=bad)", reading->env.iaq_index);
    
    // ========================================
    // Vibration Data
    // ========================================
    ESP_LOGI("MAIN", "=== Vibration Sensor ===");
    ESP_LOGI("MAIN", "  ADC Average:    %.1f (0-4095)", reading->vibration.dominant_frequency_hz);
    ESP_LOGI("MAIN", "  Sample Count:   %lu", reading->vibration.sample_count);
}
```

### 6. Complete Working Example

```c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "datatypes.h"
#include "bme680.h"
#include "vibration.h"
#include "ds3231.h"

static const char* TAG = "MAIN";

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize all sensors
    ESP_LOGI(TAG, "Initializing sensors...");
    ESP_ERROR_CHECK(bme680_init());
    ESP_ERROR_CHECK(vibration_init());
    ESP_ERROR_CHECK(rtc_init());
    ESP_LOGI(TAG, "All sensors initialized successfully");
    
    // Main loop: read sensors every 5 seconds
    while (1) {
        sensor_reading_t reading;
        
        ESP_LOGI(TAG, "\n========== Reading Sensors ==========");
        
        // Get timestamp
        rtc_get_timestamp(&reading.timestamp);
        ESP_LOGI(TAG, "Timestamp: %llu", reading.timestamp);
        
        // Read BME680
        if (bme680_measure_and_read(&reading.env) == ESP_OK) {
            ESP_LOGI(TAG, "BME680: T=%.1f°C H=%.1f%% P=%.1fhPa CO2=%dppm IAQ=%d",
                     reading.env.temperature_c,
                     reading.env.humidity_percent,
                     reading.env.pressure_hpa,
                     reading.env.co2_ppm,
                     reading.env.iaq_index);
        } else {
            ESP_LOGE(TAG, "BME680 read failed");
        }
        
        // Read vibration
        if (vibration_read(&reading.vibration) == ESP_OK) {
            ESP_LOGI(TAG, "Vibration: ADC=%.1f (samples=%lu)",
                     reading.vibration.dominant_frequency_hz,
                     reading.vibration.sample_count);
        } else {
            ESP_LOGE(TAG, "Vibration read failed");
        }
        
        ESP_LOGI(TAG, "=====================================\n");
        
        // Wait 5 seconds before next reading
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
```

---

## 📦 Data Type Reference

### **env_data_t** (BME680 Environmental Sensor)

```c
typedef struct {
    float temperature_c;          // Temperature in Celsius (-40 to 85°C)
    float humidity_percent;       // Relative humidity (0-100%)
    float pressure_hpa;           // Atmospheric pressure (300-1100 hPa)
    float gas_resistance_ohms;    // Gas sensor resistance (> 0 Ohms)
    uint16_t co2_ppm;            // CO2 approximation (400-5000 ppm)
    uint8_t iaq_index;           // Indoor Air Quality (0-500)
                                 //   0-50: Excellent
                                 //   51-100: Good
                                 //   101-150: Lightly polluted
                                 //   151-200: Moderately polluted
                                 //   201-250: Heavily polluted
                                 //   251-350: Severely polluted
                                 //   351-500: Extremely polluted
    bool valid;                  // Data validity flag
} env_data_t;
```

**How to access:**
```c
sensor_reading_t reading;
bme680_measure_and_read(&reading.env);

float temp = reading.env.temperature_c;
float humidity = reading.env.humidity_percent;
uint16_t co2 = reading.env.co2_ppm;
uint8_t air_quality = reading.env.iaq_index;
```

### **vibration_data_t** (Piezo Vibration Sensor)

```c
typedef struct {
    float dominant_frequency_hz;  // ADC average value (0-4095)
                                 // NOTE: Currently stores ADC average, not frequency
    float amplitude;              // Not used (reserved for future)
    uint32_t sample_count;        // Number of ADC samples collected
    bool vibration_detected;      // Not used (reserved for future)
    uint64_t last_vibration_time; // Timestamp of reading (seconds)
} vibration_data_t;
```

**How to access:**
```c
sensor_reading_t reading;
vibration_read(&reading.vibration);

float adc_avg = reading.vibration.dominant_frequency_hz;  // 0-4095
uint32_t samples = reading.vibration.sample_count;        // Should be ~50
```

**Note:** The field name `dominant_frequency_hz` is misleading - it currently stores the **ADC average value** (0-4095), not frequency. This is a simplified implementation.

### **rtc_time_t** (DS3231 RTC Time)

```c
typedef struct {
    uint16_t year;    // Year (e.g., 2024)
    uint8_t month;    // Month (1-12)
    uint8_t day;      // Day (1-31)
    uint8_t hour;     // Hour (0-23)
    uint8_t minute;   // Minute (0-59)
    uint8_t second;   // Second (0-59)
} rtc_time_t;
```

**How to access:**
```c
// Get structured time
rtc_time_t time;
rtc_get_time(&time);
ESP_LOGI(TAG, "Time: %04d-%02d-%02d %02d:%02d:%02d",
         time.year, time.month, time.day,
         time.hour, time.minute, time.second);

// Get Unix timestamp (recommended for sensor readings)
uint64_t timestamp;
rtc_get_timestamp(&timestamp);
ESP_LOGI(TAG, "Timestamp: %llu", timestamp);
```

### **sensor_reading_t** (Complete Reading)

```c
typedef struct {
    uint64_t timestamp;           // Unix timestamp from RTC
    env_data_t env;              // Environmental data (BME680)
    vibration_data_t vibration;  // Vibration data (piezo)
    uint8_t battery_level;       // Battery level (future use)
    bool valid;                  // Overall validity
} sensor_reading_t;
```

**This is your main data structure** - it holds everything from one sensor reading cycle.

---

## 🔧 API Quick Reference

### BME680 Functions
```c
esp_err_t bme680_init(void);                        // Initialize sensor
esp_err_t bme680_measure_and_read(env_data_t* data); // Measure + read (blocks ~200ms)
esp_err_t bme680_read(env_data_t* data);            // Read only (after trigger)
esp_err_t bme680_trigger_gas_measurement(void);     // Trigger measurement
bool bme680_gas_ready(void);                        // Check if ready
esp_err_t bme680_deinit(void);                      // Cleanup before sleep
```

### Vibration Functions
```c
esp_err_t vibration_init(void);                     // Initialize ADC
esp_err_t vibration_read(vibration_data_t* data);   // Read vibration (blocks ~500ms)
esp_err_t vibration_deinit(void);                   // Cleanup before sleep
```

### RTC Functions
```c
esp_err_t rtc_init(void);                           // Initialize RTC
esp_err_t rtc_get_time(rtc_time_t* time);          // Get structured time
esp_err_t rtc_get_timestamp(uint64_t* timestamp);  // Get Unix timestamp
esp_err_t rtc_set_time(uint64_t timestamp);        // Set time from timestamp
esp_err_t rtc_deinit(void);                        // Cleanup before sleep
```

---

## ⚠️ Important Notes

1. **BME680 Calibration**: First 10 readings are used for gas baseline calibration. IAQ values stabilize after ~10 readings.

2. **Blocking Operations**:
   - `bme680_measure_and_read()`: Blocks ~200-250ms
   - `vibration_read()`: Blocks ~500ms
   - Total reading time: ~700-750ms

3. **Deep Sleep Compatibility**: All drivers support deep sleep:
   - Call `sensor_deinit()` before sleep
   - Call `sensor_init()` after wake
   - BME680 calibration persists in NVS

4. **Error Handling**: Always check return values:
   ```c
   if (bme680_measure_and_read(&reading.env) != ESP_OK) {
       ESP_LOGE(TAG, "BME680 failed");
       reading.env.valid = false;
   }
   ```

5. **Memory**: All structures use static allocation - no malloc/free needed.

---

## 🚀 Next Steps

**To complete the system, you need to implement:**
- ⏳ Task 8: Data Manager (store readings in NVS)
- ⏳ Task 9: Communication Manager (WiFi + HTTP)
- ⏳ Task 10: Power Manager (deep sleep)
- ⏳ Task 12: State Machine (orchestrate everything)

**Current status:** HAL layer complete, ready for service layer implementation.
