# Vibration Sensor Driver (Piezo)

## Overview

Analog piezo vibration sensor driver for continuous vibration monitoring in beehive applications. Measures vibration frequency and amplitude using ADC sampling with median filtering for stable readings.

## Features

- **Blocking ADC Sampling**: Configurable sampling rate (default: 1kHz) and duration (default: 500ms)
- **Frequency Detection**: Zero-crossing detection with median filtering (10-500 Hz range)
- **Amplitude Measurement**: Peak-to-peak amplitude normalized to 0.0-1.0
- **Continuous Vibration**: Optimized for continuous vibration monitoring (not impact detection)
- **Deep Sleep Compatible**: Quick init/deinit for 15-minute wake/sleep cycles

## Hardware Configuration

### GPIO Pin
- **Default**: GPIO36 (ADC1_CHANNEL_0)
- **Configurable**: Set `VIBRATION_GPIO_PIN` in `config.h`

### ADC Settings
- **Resolution**: 12-bit (0-4095)
- **Attenuation**: 11dB (0-3.3V range)
- **Sample Rate**: 1000 Hz (configurable via `VIBRATION_SAMPLE_RATE_HZ`)
- **Sample Duration**: 500 ms (configurable via `VIBRATION_SAMPLE_DURATION_MS`)

### Piezo Sensor Connection
```
Piezo Sensor (+) ──→ GPIO36 (ADC1_CH0)
Piezo Sensor (-) ──→ GND
```

**Note**: For high-amplitude signals, add a voltage divider or clamping circuit to protect the ESP32 ADC (max 3.3V).

## Configuration (config.h)

```c
// GPIO and ADC channel
#define VIBRATION_GPIO_PIN          GPIO_NUM_36
#define VIBRATION_ADC_CHANNEL       ADC1_CHANNEL_0

// Sampling parameters
#define VIBRATION_SAMPLE_DURATION_MS    500     // 500ms sampling window
#define VIBRATION_SAMPLE_RATE_HZ        1000    // 1kHz sampling rate
#define VIBRATION_THRESHOLD             0.1f    // Detection threshold (0.0-1.0)

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

### Frequency Detection
1. **DC Offset Removal**: Calculate mean ADC value as baseline
2. **Zero-Crossing Detection**: Count sign changes relative to baseline
3. **Frequency Calculation**: Measure time between crossings
4. **Median Filtering**: Use median of detected frequencies for stability

### Amplitude Calculation
1. **Peak-to-Peak**: Find min/max ADC values in sample window
2. **Normalization**: Divide by ADC max value (4095) to get 0.0-1.0 range

### Vibration Detection
- Vibration is detected when amplitude exceeds threshold (default: 0.1)
- Frequency must be within valid range (10-500 Hz)

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

- **Static Buffer**: `VIBRATION_BUFFER_SIZE * sizeof(int32_t)` = 500 * 4 = 2KB
- **Frequency Array**: ~500 bytes (temporary, stack allocated)
- **Total**: ~2.5KB RAM

## Performance

- **Sampling Time**: 500ms (configurable)
- **Processing Time**: <50ms (frequency analysis)
- **Total Read Time**: ~550ms
- **Power Consumption**: ~80mA during active sampling

## Troubleshooting

### No Vibration Detected
- Check piezo sensor connection
- Verify GPIO pin configuration
- Lower threshold value
- Check ADC voltage range (must be 0-3.3V)

### Unstable Frequency Readings
- Increase sample duration for more data
- Check for electrical noise on ADC line
- Add capacitor (0.1µF) across piezo terminals

### ADC Read Failures
- Verify ADC1 channel is not used by WiFi
- Check GPIO pin is ADC-capable
- Ensure ADC is not in use by other components
