# Vibration Sensor Driver (Piezo)

## Overview

Analog piezo vibration sensor driver for continuous vibration monitoring in beehive applications. Measures vibration frequency and amplitude using ADC sampling with median filtering for stable readings.

## Features

- **Blocking ADC Sampling**: 100 Hz sample rate (10ms intervals) for 500ms duration
- **Peak Detection**: Detects piezo voltage spikes with debouncing
- **Frequency Measurement**: Calculates actual Hz from peak intervals with median filtering (10-500 Hz range)
- **Amplitude Measurement**: Maximum deviation from baseline, normalized to 0.0-1.0
- **Continuous Vibration**: Optimized for continuous vibration monitoring (beehive wing beats)
- **Deep Sleep Compatible**: Quick init/deinit for 15-minute wake/sleep cycles

## Hardware Configuration

### GPIO Pin
- **Default**: GPIO36 (ADC1_CHANNEL_0)
- **Configurable**: Set `VIBRATION_GPIO_PIN` in `config.h`

### ADC Settings
- **Resolution**: 12-bit (0-4095)
- **Attenuation**: 11dB (0-3.3V range)
- **Sample Rate**: 100 Hz (10ms intervals)
- **Sample Duration**: 500 ms (50 samples total)
- **Required Hardware**: 1MΩ resistor from ADC pin to GND (voltage bleed/protection)

### Piezo Sensor Connection
```
Piezo Sensor (+) ──→ GPIO36 (ADC1_CH0)
                 │
                 └──→ 1MΩ Resistor ──→ GND
Piezo Sensor (-) ──→ GND
```

**Important**: The 1MΩ pulldown resistor is **required** to:
- Bleed off excess voltage from piezo element
- Protect ESP32 ADC input (max 3.3V)
- Provide stable baseline reference

## Configuration (config.h)

```c
// GPIO and ADC channel
#define VIBRATION_GPIO_PIN          GPIO_NUM_36
#define VIBRATION_ADC_CHANNEL       ADC1_CHANNEL_0

// Sampling parameters
#define VIBRATION_SAMPLE_DURATION_MS    500     // 500ms sampling window
#define VIBRATION_SAMPLE_RATE_HZ        100     // 100Hz (10ms intervals)
#define VIBRATION_SAMPLE_INTERVAL_MS    10      // 10ms between readings
#define VIBRATION_THRESHOLD             0.05f   // Detection threshold (0.0-1.0, 5%)
#define VIBRATION_MIN_PEAK_SPACING_MS   10      // Debounce time (10ms)

// Frequency range
#define VIBRATION_MIN_FREQ_HZ       10      // Minimum frequency
#define VIBRATION_MAX_FREQ_HZ       500     // Maximum frequency
```

## Usage

```c
#include "vibration.h"

// 1. Initialize sensor
esp_err_t ret = vibration_init();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Vibration init failed");
    return;
}

// 2. Read vibration data
vibration_data_t vib_data;
ret = vibration_read(&vib_data);
if (ret == ESP_OK) {
    if (vib_data.vibration_detected) {
        ESP_LOGI(TAG, "Vibration: %.1f Hz, Amplitude: %.3f",
                 vib_data.dominant_frequency_hz,
                 vib_data.amplitude);
    } else {
        ESP_LOGI(TAG, "No vibration detected");
    }
}

// 3. Deinitialize before deep sleep
vibration_deinit();
```

## API Reference

### `vibration_init()`
Initializes ADC1 for piezo sensor reading. Configures GPIO pin, ADC channel, resolution, and attenuation.

**Returns**: `ESP_OK` on success, error code otherwise

### `vibration_read(vibration_data_t* data)`
Performs blocking ADC sampling for configured duration. Analyzes samples to extract frequency and amplitude.

**Parameters**:
- `data`: Pointer to `vibration_data_t` structure to populate

**Returns**: `ESP_OK` on success, `ESP_ERR_INVALID_ARG` if data is NULL, `ESP_ERR_INVALID_STATE` if not initialized

**Output Structure**:
```c
typedef struct {
    float dominant_frequency_hz;  // Median frequency (Hz)
    float amplitude;              // Normalized amplitude (0.0-1.0)
    uint32_t sample_count;        // Number of samples analyzed
    bool vibration_detected;      // True if amplitude > threshold
    uint64_t last_vibration_time; // Timestamp (Unix seconds)
} vibration_data_t;
```

### `vibration_deinit()`
Releases ADC resources before deep sleep.

**Returns**: `ESP_OK` on success, error code otherwise

## Algorithm Details

### Peak Detection (Piezo-Specific)
1. **DC Offset Removal**: Calculate mean ADC value as baseline
2. **Peak Detection**: Find local maxima above threshold
   - Current value > previous value
   - Current value ≥ next value  
   - Current value > baseline + threshold
3. **Debouncing**: Minimum 10ms between valid peaks
4. **Frequency Calculation**: Measure time between consecutive peaks
5. **Median Filtering**: Use median of detected frequencies for stability

### Amplitude Calculation
1. **Maximum Deviation**: Find max absolute deviation from baseline
2. **Normalization**: Divide by half ADC range (piezo swings one direction)
3. **Result**: 0.0-1.0 normalized amplitude

### Vibration Detection
- Vibration detected when amplitude exceeds threshold (default: 0.05 = 5%)
- Frequency must be within valid range (10-500 Hz)
- Returns actual Hz frequency in `dominant_frequency_hz`

## Beehive Application Notes

### Typical Vibration Frequencies
- **Worker bee wing beat**: 200-250 Hz
- **Queen piping**: 300-500 Hz
- **Swarming activity**: 100-300 Hz

### Recommended Settings
- **Sample Duration**: 500ms (captures multiple vibration cycles)
- **Threshold**: 0.1 (filters out ambient noise)
- **Frequency Range**: 10-500 Hz (covers all bee activity)

## Memory Usage

- **Static Buffer**: `VIBRATION_BUFFER_SIZE * sizeof(int32_t)` = 50 * 4 = 200 bytes
- **Frequency Array**: ~200 bytes (temporary, stack allocated)
- **Total**: ~400 bytes RAM

## Performance

- **Sampling Time**: 500ms (50 samples at 10ms intervals)
- **Processing Time**: <20ms (peak detection + median)
- **Total Read Time**: ~520ms
- **Power Consumption**: ~80mA during active sampling

## Troubleshooting

### No Vibration Detected
- Check piezo sensor connection
- Verify GPIO pin configuration
- Lower threshold value
- Check ADC voltage range (must be 0-3.3V)

### Unstable Frequency Readings
- Increase sample duration for more peak intervals
- Check for electrical noise on ADC line
- Add capacitor (0.1µF) across piezo terminals
- Verify 1MΩ pulldown resistor is installed

### ADC Read Failures
- Verify ADC1 channel is not used by WiFi
- Check GPIO pin is ADC-capable (GPIO32-39 on ESP32)
- Ensure ADC is not in use by other components
- Verify 1MΩ resistor connection
