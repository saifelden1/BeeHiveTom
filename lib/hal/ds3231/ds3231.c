/**
 * @file ds3231.c
 * @brief DS3231 Real-Time Clock Driver Implementation
 * 
 * Wraps esp-idf-lib DS3231 component with project-specific interface.
 * Provides simple time read/write operations.
 * 
 * DEEP SLEEP COMPATIBLE:
 * - Designed for 15-minute wake/sleep cycles
 * - Quick initialization for fast wake cycles
 * - RTC maintains time during deep sleep (battery backed)
 */

#include "ds3231.h"
#include "config.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "ds3231.h"  // esp-idf-lib ds3231 component
#include <string.h>
#include <time.h>

// Log tag
static const char* TAG = LOG_TAG_RTC;

// I2C port for DS3231
#define DS3231_I2C_PORT I2C_NUM_0

// Driver state
static bool s_initialized = false;
static i2c_dev_t s_ds3231_dev;

/**
 * @brief Initialize I2C for DS3231
 */
static esp_err_t ds3231_i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GPIO_NUM_21,  // Default SDA pin
        .scl_io_num = GPIO_NUM_22,  // Default SCL pin
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    esp_err_t ret = i2c_param_config(DS3231_I2C_PORT, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = i2c_driver_install(DS3231_I2C_PORT, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "I2C driver already installed");
            return ESP_OK;
        }
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "I2C initialized on port %d (SDA: GPIO%d, SCL: GPIO%d)",
             DS3231_I2C_PORT, GPIO_NUM_21, GPIO_NUM_22);
    
    return ESP_OK;
}

esp_err_t rtc_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "RTC already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing DS3231 RTC...");
    ESP_LOGI(TAG, "I2C Address: 0x%02X", DS3231_I2C_ADDR);
    
    // Initialize I2C
    esp_err_t ret = ds3231_i2c_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C initialization failed");
        return ret;
    }
    
    // Initialize DS3231 device descriptor
    memset(&s_ds3231_dev, 0, sizeof(i2c_dev_t));
    ret = ds3231_init_desc(&s_ds3231_dev, DS3231_I2C_PORT, GPIO_NUM_21, GPIO_NUM_22);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize DS3231 descriptor: %s", esp_err_to_name(ret));
        return ret;
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "DS3231 RTC initialized successfully");
    
    return ESP_OK;
}

esp_err_t rtc_get_time(rtc_time_t* time)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "RTC not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (time == NULL) {
        ESP_LOGE(TAG, "Invalid time pointer");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Read time from DS3231 using esp-idf-lib
    struct tm timeinfo;
    esp_err_t ret = ds3231_get_time(&s_ds3231_dev, &timeinfo);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read time from RTC: %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }
    
    // Convert tm structure to rtc_time_t
    time->year = timeinfo.tm_year + 1900;  // tm_year is years since 1900
    time->month = timeinfo.tm_mon + 1;     // tm_mon is 0-11
    time->day = timeinfo.tm_mday;
    time->hour = timeinfo.tm_hour;
    time->minute = timeinfo.tm_min;
    time->second = timeinfo.tm_sec;
    
    ESP_LOGI(TAG, "RTC time: %04d-%02d-%02d %02d:%02d:%02d",
             time->year, time->month, time->day,
             time->hour, time->minute, time->second);
    
    return ESP_OK;
}

esp_err_t rtc_get_timestamp(uint64_t* timestamp)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "RTC not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (timestamp == NULL) {
        ESP_LOGE(TAG, "Invalid timestamp pointer");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Read time from RTC
    rtc_time_t time;
    esp_err_t ret = rtc_get_time(&time);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Convert to Unix timestamp using standard library
    struct tm timeinfo = {
        .tm_year = time.year - 1900,  // tm_year is years since 1900
        .tm_mon = time.month - 1,     // tm_mon is 0-11
        .tm_mday = time.day,
        .tm_hour = time.hour,
        .tm_min = time.minute,
        .tm_sec = time.second,
        .tm_isdst = -1  // Let mktime determine DST
    };
    
    time_t unix_time = mktime(&timeinfo);
    if (unix_time == -1) {
        ESP_LOGE(TAG, "Failed to convert time to timestamp");
        return ESP_FAIL;
    }
    
    *timestamp = (uint64_t)unix_time;
    
    ESP_LOGD(TAG, "Unix timestamp: %llu", *timestamp);
    
    return ESP_OK;
}

esp_err_t rtc_set_time(uint64_t timestamp)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "RTC not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Validate timestamp (must be after 2000-01-01)
    if (timestamp < 946684800) {  // 2000-01-01 00:00:00 UTC
        ESP_LOGE(TAG, "Invalid timestamp: %llu (too old)", timestamp);
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Setting RTC time from timestamp: %llu", timestamp);
    
    // Convert Unix timestamp to tm structure
    time_t unix_time = (time_t)timestamp;
    struct tm* timeinfo = gmtime(&unix_time);
    
    if (timeinfo == NULL) {
        ESP_LOGE(TAG, "Failed to convert timestamp to time");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Setting RTC time: %04d-%02d-%02d %02d:%02d:%02d",
             timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
             timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    
    // Set time on DS3231 using esp-idf-lib
    esp_err_t ret = ds3231_set_time(&s_ds3231_dev, timeinfo);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set time on RTC: %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "RTC time set successfully");
    
    return ESP_OK;
}

esp_err_t rtc_deinit(void)
{
    if (!s_initialized) {
        ESP_LOGW(TAG, "RTC not initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Deinitializing DS3231 RTC...");
    
    // Free DS3231 device descriptor
    esp_err_t ret = ds3231_free_desc(&s_ds3231_dev);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to free DS3231 descriptor: %s", esp_err_to_name(ret));
    }
    
    // Delete I2C driver
    ret = i2c_driver_delete(DS3231_I2C_PORT);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "I2C driver delete failed: %s", esp_err_to_name(ret));
    }
    
    s_initialized = false;
    
    ESP_LOGI(TAG, "DS3231 RTC deinitialized");
    
    return ESP_OK;
}
