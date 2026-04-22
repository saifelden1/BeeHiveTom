# Power Manager Service

Manages deep sleep and peripheral power control.

## Features
- Timer-based wake (15 minutes default)
- Automatic sensor deinitialization
- Deep sleep entry

## Usage
```c
power_manager_init();
// ... do work ...
power_manager_sleep(); // Does not return
```

## Configuration
Sleep duration in `config.h`:
- `SLEEP_DURATION_SEC` (default: 900 seconds = 15 minutes)
