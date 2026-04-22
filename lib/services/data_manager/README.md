# Data Manager Service

## Overview

The Data Manager service provides persistent storage of sensor readings in NVS (Non-Volatile Storage) with JSON formatting for transmission.

## Simple Explanation: How It Works

### The Big Picture
Think of the Data Manager as a **notebook that survives power loss**:
1. **Store**: Write sensor readings to flash memory (NVS)
2. **Track**: Keep a counter of how many readings exist
3. **Format**: Convert all readings to JSON text when needed
4. **Send**: Pass JSON to WiFi manager for transmission
5. **Clear**: Erase all readings after successful send

### The JSON Buffer Flow

```
┌─────────────────────────────────────────────────────────────┐
│ main.c (app_main function)                                  │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  1. Read sensors → sensor_reading_t                         │
│  2. data_manager_store_reading(&reading)                    │
│     └─> Writes to NVS flash: "reading_0", "reading_1"...   │
│                                                              │
│  3. Allocate JSON buffer (2.8KB)                            │
│     char* json_buffer = malloc(JSON_BUFFER_SIZE);           │
│                                                              │
│  4. Format JSON from NVS                                    │
│     data_manager_format_json(json_buffer, size)             │
│     ├─> Reads "reading_0" from NVS                          │
│     ├─> Reads "reading_1" from NVS                          │
│     ├─> Reads "reading_2" from NVS                          │
│     └─> Builds: {"id":"esp32_001","r":[{...},{...},{...}]} │
│                                                              │
│  5. Send JSON via WiFi                                      │
│     comm_manager_send_json(json_buffer, strlen(json_buffer))│
│                                                              │
│  6. If send succeeds:                                       │
│     data_manager_clear()                                    │
│     └─> Deletes all "reading_X" from NVS                    │
│                                                              │
│  7. Free buffer                                             │
│     free(json_buffer);                                      │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### Where Data Lives

**NVS Flash (Survives Power Loss)**:
```
NVS["metadata"]   = {count: 3}
NVS["reading_0"]  = {ts:1704067200, temp:22.5, hum:55.2, ...}
NVS["reading_1"]  = {ts:1704068100, temp:22.7, hum:54.8, ...}
NVS["reading_2"]  = {ts:1704069000, temp:23.0, hum:54.5, ...}
```

**RAM (Temporary, Lost on Power Loss)**:
```c
// In main.c during transmission:
char* json_buffer = malloc(2816);  // Allocated on heap
// Contains: {"id":"esp32_001","r":[{...},{...},{...}]}
// After transmission: free(json_buffer);  // Released
```

### How NVS → JSON Works

**Step 1: Read from NVS**
```c
// Inside data_manager_format_json():
for (uint32_t i = 0; i < count; i++) {
    sensor_reading_t reading;
    
    // Fetch from NVS: "reading_0", "reading_1", etc.
    data_manager_get_reading(i, &reading);
    
    // Convert to JSON and append to buffer
    format_reading_json(buffer, &offset, size, &reading, is_last);
}
```

**Step 2: Build JSON String**
```c
// data_manager_get_reading() implementation:
esp_err_t data_manager_get_reading(uint32_t index, sensor_reading_t* reading)
{
    // Build key: "reading_0", "reading_1", etc.
    char key[32];
    snprintf(key, sizeof(key), "reading_%lu", index);
    
    // Read from NVS flash into 'reading' struct
    size_t size = sizeof(sensor_reading_t);
    return nvs_get_blob(nvs_handle, key, reading, &size);
}
```

### Accessing JSON for WiFi

**In main.c (lines 120-145)**:
```c
// Allocate buffer
char* json_buffer = malloc(JSON_BUFFER_SIZE);

// Fill buffer with JSON from NVS
data_manager_format_json(json_buffer, JSON_BUFFER_SIZE);

// Now json_buffer contains:
// {"id":"esp32_001","r":[{"ts":1704067200,"temp":22.50,...},{...}]}

// Send it
comm_manager_send_json(json_buffer, strlen(json_buffer));

// Clean up
free(json_buffer);
```

**Key Points**:
- Buffer is **allocated in main.c** (not in data_manager)
- Data Manager **fills** the buffer with JSON text
- Communication Manager **sends** the buffer via WiFi
- Buffer is **freed** after transmission

## Architecture

- **Storage**: Unlimited NVS storage with sequential keys
- **Format**: Configurable JSON format via `config.h`
- **Memory**: Static buffers to avoid heap fragmentation
- **Reliability**: Automatic retry on write failures

## Key Features

### 1. Unlimited Storage
- No circular buffer or fixed limit
- Stores readings indefinitely until transmitted
- Sequential key naming: `reading_0`, `reading_1`, etc.

### 2. Batch Operations
- Store readings when WiFi unavailable
- Format all readings as JSON when WiFi available
- Clear all readings after successful transmission

### 3. Configurable JSON Format
Three format types in `config.h`:
- **Type 0 (Compact)**: Minimal keys, no whitespace
- **Type 1 (Standard)**: Readable with descriptive keys
- **Type 2 (Verbose)**: Full descriptive keys with units

### 4. Error Handling
- Automatic retry on NVS write failures (3 attempts)
- Skip and log failed writes (don't block system)
- Graceful degradation on NVS full

## API Functions

### Initialization
```c
esp_err_t data_manager_init(void);
```
Initialize NVS namespace and load metadata.

### Store Reading
```c
esp_err_t data_manager_store_reading(const sensor_reading_t* reading);
```
Store sensor reading with automatic retry. Skips invalid readings.

### Get Count
```c
uint32_t data_manager_get_count(void);
```
Returns number of stored readings.

### Format JSON
```c
esp_err_t data_manager_format_json(char* buffer, size_t size);
```
Format all stored readings as JSON. Buffer should be `JSON_BUFFER_SIZE` bytes.

### Clear Readings
```c
esp_err_t data_manager_clear(void);
```
Delete all readings after successful transmission.

### Check Full
```c
bool data_manager_is_full(void);
```
Check if NVS partition is nearly full (<10% free).

### Get Reading
```c
esp_err_t data_manager_get_reading(uint32_t index, sensor_reading_t* reading);
```
Retrieve specific reading by index (for debugging).

### Cleanup
```c
esp_err_t data_manager_deinit(void);
```
Close NVS handle before deep sleep.

## Usage Example

```c
// Initialize
data_manager_init();

// Store readings (no WiFi)
sensor_reading_t reading;
// ... populate reading ...
data_manager_store_reading(&reading);

// Check count
uint32_t count = data_manager_get_count();
printf("Stored readings: %lu\n", count);

// Format JSON (WiFi available)
static char json_buffer[JSON_BUFFER_SIZE];
if (data_manager_format_json(json_buffer, sizeof(json_buffer)) == ESP_OK) {
    // Send via comm_manager
    comm_manager_send_data(json_buffer);
    
    // Clear after successful transmission
    data_manager_clear();
}

// Cleanup before sleep
data_manager_deinit();
```

## Configuration (config.h)

### JSON Format
```c
#define JSON_FORMAT_TYPE            0  // 0=compact, 1=standard, 2=verbose
```

### Buffer Sizing
```c
#define JSON_SINGLE_READING_SIZE    256
#define JSON_MAX_READINGS_PER_BATCH 100
#define JSON_BUFFER_SIZE            (JSON_SINGLE_READING_SIZE * JSON_MAX_READINGS_PER_BATCH + 512)
```

### Retry Configuration
```c
#define NVS_WRITE_MAX_RETRIES       3
#define NVS_WRITE_RETRY_DELAY_MS    50
```

## Memory Considerations

### Static Allocation
- No dynamic memory allocation (no `malloc`)
- Uses stack-based buffers for JSON formatting
- Caller provides buffer for `data_manager_format_json()`

### NVS Usage
- Each reading: ~100 bytes in NVS
- Metadata: ~8 bytes
- Total capacity depends on NVS partition size

### Recommended Buffer Size
```c
static char json_buffer[JSON_BUFFER_SIZE];  // ~26KB for 100 readings
```

## Error Handling

### Write Failures
- Retries 3 times with 50ms delay
- Logs error and skips reading if all retries fail
- System continues operation (reading is lost)

### NVS Full
- `data_manager_is_full()` checks free space
- Returns true if <10% free
- Application should trigger transmission

### Invalid Readings
- Skips readings with `valid = false`
- Logs warning and returns `ESP_ERR_INVALID_ARG`

## Workflow Integration

### State Machine Integration
```
STATE_STORE_DATA:
    data_manager_store_reading(&reading);
    
STATE_TRANSMIT_CHECK:
    if (data_manager_get_count() >= TRANSMISSION_INTERVAL) {
        goto STATE_TRANSMIT_DATA;
    }
    
STATE_TRANSMIT_DATA:
    data_manager_format_json(buffer, size);
    comm_manager_send_data(buffer);
    data_manager_clear();  // After successful send
```

## Testing Considerations

### Unit Tests
- Test NVS initialization
- Test storing/retrieving readings
- Test JSON formatting with all format types
- Test retry mechanism
- Test clear operation

### Integration Tests
- Test persistence across simulated reboot
- Test NVS full scenario
- Test batch transmission workflow
- Test error recovery

## Performance

### Write Performance
- Single write: ~5-10ms (including commit)
- Retry overhead: +50ms per retry
- Typical: <20ms per reading

### Read Performance
- Single read: ~2-5ms
- JSON formatting: ~50-100ms for 100 readings

### Memory Usage
- RAM: <1KB (metadata + buffers)
- NVS: ~100 bytes per reading
- Stack: ~26KB for JSON buffer (temporary)
