# BME680 Sensor Driver

Environmental sensor driver for temperature, humidity, pressure, gas/VOC, CO2 approximation, and IAQ index.

## Deep Sleep Compatible
✅ Designed for 15-minute wake/sleep cycles  
✅ Calibration baseline persists in NVS across deep sleep  
✅ Fast initialization on wake-up  

## Quick Start

```c
#include "bme680.h"

// 1. Initialize (call on every wake-up)
bme680_init();

// 2. Trigger gas measurement
bme680_trigger_gas_measurement();

// 3. Wait for measurement (150ms + margin)
vTaskDelay(pdMS_TO_TICKS(200));

// 4. Read sensor data
bme680_data_t data;
if (bme680_read(&data) == ESP_OK && data.valid) {
    printf("Temp: %.1f°C, Humidity: %.1f%%, Pressure: %.1f hPa\n",
           data.temperature_c, data.humidity_percent, data.pressure_hpa);
    printf("Gas: %.0f Ohms, CO2: %d ppm, IAQ: %d\n",
           data.gas_resistance_ohms, data.co2_ppm, data.iaq_index);
}

// 5. (OPTIONAL) Deinitialize before deep sleep
// Note: Deep sleep resets everything, so deinit is optional
// Only needed for testing or non-deep-sleep scenarios
bme680_deinit();
```

## Calibration

- **First 10 readings**: Establishes gas baseline for IAQ/CO2 calculation
- **Baseline saved to NVS**: Persists across deep sleep and power cycles
- **Auto-restore**: Calibration loaded automatically on subsequent wake-ups

## Configuration (config.h)

```c
#define BME680_I2C_ADDR         0x76    // or 0x77
#define BME680_HEATER_TEMP      320     // °C
#define BME680_HEATER_DURATION  150     // ms
#define I2C_MASTER_FREQ_HZ      400000  // 400kHz
```

## Data Structure

```c
typedef struct {
    float temperature_c;          // -40 to 85°C
    float humidity_percent;       // 0-100%
    float pressure_hpa;           // 300-1100 hPa
    float gas_resistance_ohms;    // > 0
    uint16_t co2_ppm;            // 400-5000 ppm (approximation)
    uint8_t iaq_index;           // 0-500 (0=excellent, 500=extremely polluted)
    bool valid;                  // Data validity flag
} bme680_data_t;
```

## IAQ Index Scale

| Range   | Air Quality          |
|---------|---------------------|
| 0-50    | Excellent           |
| 51-100  | Good                |
| 101-150 | Lightly polluted    |
| 151-200 | Moderately polluted |
| 201-250 | Heavily polluted    |
| 251-350 | Severely polluted   |
| 351-500 | Extremely polluted  |

## Notes

- **CO2 values are approximations** from gas resistance (not direct measurement)
- Gas sensor requires **warm-up time** (~150ms) before reading
- Calibration completes after **10 readings** (~2.5 hours at 15-min intervals)
- I2C pins: **GPIO21 (SDA), GPIO22 (SCL)**
- Power consumption: **<30s active time per 15-min cycle**
- **Deep sleep resets everything**: `bme680_deinit()` is optional before deep sleep (system resets anyway)
- **Always call `bme680_init()`** after wake-up (I2C driver not installed after reset)
