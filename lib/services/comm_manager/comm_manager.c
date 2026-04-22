/**
 * @file comm_manager.c
 * @brief Communication Manager Service Implementation (Placeholder)
 */

#include "comm_manager.h"
#include "config.h"
#include "esp_log.h"

static const char* TAG = LOG_TAG_COMM_MGR;

esp_err_t comm_manager_init(void)
{
    ESP_LOGI(TAG, "Initialized (placeholder)");
    return ESP_OK;
}

esp_err_t comm_manager_send_json(const char* json_data, size_t length)
{
    ESP_LOGI(TAG, "Send JSON (placeholder): %zu bytes", length);
    
    // TODO: Implement WiFi connection
    // TODO: Implement HTTP POST to SERVER_URL
    // TODO: Implement WiFi disconnection
    
    return ESP_OK;
}

esp_err_t comm_manager_deinit(void)
{
    ESP_LOGI(TAG, "Deinitialized (placeholder)");
    return ESP_OK;
}
