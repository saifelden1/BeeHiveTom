# Battery Level HAL

Simple ADC-based battery voltage monitoring.

## Features
- Single ADC read (no averaging)
- Voltage divider compensation
- Percentage output (0-100%)
- Configurable via `config.h`

## Configuration
All settings in `config.h`:
- GPIO pin and ADC channel
- Battery voltage range (min/max)
- Voltage divider ratio
- ADC attenuation and resolution

## Usage
```c
battery_init();
uint8_t level = 0;
battery_read_level(&level);
battery_deinit();
```
