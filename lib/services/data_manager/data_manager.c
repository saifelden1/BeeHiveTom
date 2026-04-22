/**
 * @file data_manager.c
 * @brief Data Manager Service Layer Implementation
 * 
 * Manages persistent storage of sensor readings in NVS.
 * Implements unlimited storage with sequential keys.
 */

#include "data_manager.h"
#include "config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// PRIVATE DEFINITIONS
// ============================================================================

static const char* TAG = LOG_TAG_DATA_MGR;

/**
 * @brief NVS handle for sensor data namespace
 */
static nvs_handle_t nvs_handle = 0;

/**
 * @brief Metadata structure stored in NVS
 */
typedef struct {
    uint32_t count;  // Number of stored readings (also serves as next write index)
} nvs_metadata_t;

/**
 * @brief Current metadata (cached in RAM)
 */
static nvs_metadata_t metadata = {0};

/**
 * @brief Initialization flag
 */
static bool initialized = false;

// ============================================================================
// PRIVATE FUNCTION PROTOTYPES
// ============================================================================

static esp_err_t load_metadata(void);
static esp_err_t save_metadata(void);
static esp_err_t write_reading_with_retry(uint32_t index, const sensor_reading_t* reading);
static void format_reading_json(char* buffer, size_t* offset, size_t size, 
                                const sensor_reading_t* reading, bool is_last);

// ============================================================================
// PUBLIC FUNCTIONS
// ============================================================================

esp_err_t data_manager_init(void)
{
    if (initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace");
        return ESP_FAIL;
    }

    if (load_metadata() == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No existing metadata, initializing fresh");
        metadata.count = 0;
        if (save_metadata() != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save initial metadata");
            nvs_close(nvs_handle);
            return ESP_FAIL;
        }
    }

    initialized = true;
    ESP_LOGI(TAG, "Initialized with %lu stored readings", (unsigned long)metadata.count);
    return ESP_OK;
}

esp_err_t data_manager_store_reading(const sensor_reading_t* reading)
{
    if (!initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (reading == NULL || !reading->valid) {
        ESP_LOGW(TAG, "Invalid reading");
        return ESP_ERR_INVALID_ARG;
    }

    if (write_reading_with_retry(metadata.count, reading) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to store reading after retries, skipping");
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Stored reading %lu", (unsigned long)metadata.count);
    
    metadata.count++;
    save_metadata();  // Non-critical if fails
    
    return ESP_OK;
}

uint32_t data_manager_get_count(void)
{
    if (!initialized) {
        ESP_LOGW(TAG, "Not initialized");
        return 0;
    }
    
    return metadata.count;
}

esp_err_t data_manager_format_json(char* buffer, size_t size)
{
    if (!initialized || buffer == NULL || size == 0) {
        ESP_LOGE(TAG, "Invalid state or arguments");
        return ESP_ERR_INVALID_ARG;
    }

    if (metadata.count == 0) {
        ESP_LOGW(TAG, "No readings to format");
        return ESP_ERR_NOT_FOUND;
    }

    size_t offset = 0;
    
    // Start JSON object
    offset += snprintf(buffer, size, "{\"%s\":\"%s\",\"%s\":[",
                      JSON_KEY_DEVICE_ID, DEVICE_ID, JSON_KEY_READINGS);
    
    if (offset >= size - 10) {
        ESP_LOGE(TAG, "Buffer too small");
        return ESP_ERR_INVALID_SIZE;
    }

    // Format each reading (from 0 to count-1)
    uint32_t readings_to_format = (metadata.count > JSON_MAX_READINGS_PER_BATCH) 
                                  ? JSON_MAX_READINGS_PER_BATCH : metadata.count;
    
    for (uint32_t i = 0; i < readings_to_format; i++) {
        sensor_reading_t reading;
        
        if (data_manager_get_reading(i, &reading) != ESP_OK) {
            continue;
        }

        format_reading_json(buffer, &offset, size, &reading, (i == readings_to_format - 1));
        
        if (offset >= size - 10) {
            ESP_LOGE(TAG, "Buffer overflow");
            return ESP_ERR_INVALID_SIZE;
        }
    }

    // Close JSON object
    offset += snprintf(buffer + offset, size - offset, "]}");

    ESP_LOGI(TAG, "Formatted %lu readings (%zu bytes)", 
             (unsigned long)readings_to_format, offset);
    
    return ESP_OK;
}

esp_err_t data_manager_clear(void)
{
    if (!initialized || metadata.count == 0) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Clearing %lu readings", (unsigned long)metadata.count);

    char key[32];
    for (uint32_t i = 0; i < metadata.count; i++) {
        snprintf(key, sizeof(key), "%s%lu", NVS_KEY_READING_PREFIX, (unsigned long)i);
        nvs_erase_key(nvs_handle, key);  // Ignore errors
    }

    metadata.count = 0;
    
    if (save_metadata() != ESP_OK || nvs_commit(nvs_handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit clear");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Cleared all readings");
    return ESP_OK;
}

bool data_manager_is_full(void)
{
    if (!initialized) {
        return true;
    }

    nvs_stats_t nvs_stats;
    if (nvs_get_stats(NVS_NAMESPACE, &nvs_stats) != ESP_OK || nvs_stats.total_entries == 0) {
        return false;
    }

    return (nvs_stats.free_entries * 100 / nvs_stats.total_entries) < 10;
}

esp_err_t data_manager_get_reading(uint32_t index, sensor_reading_t* reading)
{
    if (!initialized || reading == NULL || index >= metadata.count) {
        return ESP_ERR_INVALID_ARG;
    }

    char key[32];
    snprintf(key, sizeof(key), "%s%lu", NVS_KEY_READING_PREFIX, (unsigned long)index);
    
    size_t required_size = sizeof(sensor_reading_t);
    return nvs_get_blob(nvs_handle, key, reading, &required_size);
}

esp_err_t data_manager_deinit(void)
{
    if (!initialized) {
        return ESP_OK;
    }

    nvs_close(nvs_handle);
    initialized = false;
    
    ESP_LOGI(TAG, "Deinitialized");
    return ESP_OK;
}

// ============================================================================
// PRIVATE FUNCTIONS
// ============================================================================

static esp_err_t load_metadata(void)
{
    size_t required_size = sizeof(nvs_metadata_t);
    return nvs_get_blob(nvs_handle, NVS_KEY_METADATA, &metadata, &required_size);
}

static esp_err_t save_metadata(void)
{
    if (nvs_set_blob(nvs_handle, NVS_KEY_METADATA, &metadata, sizeof(nvs_metadata_t)) != ESP_OK) {
        return ESP_FAIL;
    }
    return nvs_commit(nvs_handle);
}

static esp_err_t write_reading_with_retry(uint32_t index, const sensor_reading_t* reading)
{
    char key[32];
    snprintf(key, sizeof(key), "%s%lu", NVS_KEY_READING_PREFIX, (unsigned long)index);
    
    for (uint8_t retry = 0; retry < NVS_WRITE_MAX_RETRIES; retry++) {
        if (nvs_set_blob(nvs_handle, key, reading, sizeof(sensor_reading_t)) == ESP_OK &&
            nvs_commit(nvs_handle) == ESP_OK) {
            return ESP_OK;
        }
        
        if (retry < NVS_WRITE_MAX_RETRIES - 1) {
            vTaskDelay(pdMS_TO_TICKS(NVS_WRITE_RETRY_DELAY_MS));
        }
    }

    ESP_LOGE(TAG, "Failed to write after %d retries", NVS_WRITE_MAX_RETRIES);
    return ESP_FAIL;
}

static void format_reading_json(char* buffer, size_t* offset, size_t size,
                                const sensor_reading_t* reading, bool is_last)
{
    *offset += snprintf(buffer + *offset, size - *offset,
        "{"
        "\"%s\":%llu,"
        "\"%s\":%.2f,"
        "\"%s\":%.2f,"
        "\"%s\":%u,"
        "\"%s\":%.3f,"
        "\"%s\":%u"
        "}%s",
        JSON_KEY_TIMESTAMP, (unsigned long long)reading->timestamp,
        JSON_KEY_TEMP, reading->temperature_c,
        JSON_KEY_HUM, reading->humidity_percent,
        JSON_KEY_CO2, reading->co2_ppm,
        JSON_KEY_VIB, reading->vibration_amplitude,
        JSON_KEY_BATTERY, reading->battery_level,
        is_last ? "" : ","
    );
}
