#include "global.h"

// 1. Declare global flag variable
bool is_ota_running = false; 

static const char *TAG = "OTA_UPDATE";

// Independent task to handle downloading and flashing ROM
void ota_task(void *pvParameter) {
    char *firmware_url = (char *)pvParameter;
    ESP_LOGI(TAG, "Starting OTA process. URL: %s", firmware_url);

    // Configure HTTP Client to access AWS S3 via HTTPS
    esp_http_client_config_t config = {
        .url = firmware_url,
        .crt_bundle_attach = esp_crt_bundle_attach, 
        .keep_alive_enable = true,
        .timeout_ms = 15000 // 2. Increase Timeout to 15000ms (15 seconds) for stability
    };

    // Configure parameters for the OTA library
    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };

    ESP_LOGW(TAG, "Downloading Firmware from AWS S3...");
    ESP_LOGW(TAG, "WARNING: PLEASE DO NOT TURN OFF POWER OR DISCONNECT WIFI NOW!");
    
    // Call the ESP-IDF OTA execution function 
    esp_err_t ret = esp_https_ota(&ota_config);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "====================================");
        ESP_LOGI(TAG, "   OTA SUCCESSFUL! PREPARING TO REBOOT  ");
        ESP_LOGI(TAG, "====================================");
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        esp_restart(); // Restart with the new Firmware
    } else {
        ESP_LOGE(TAG, "OTA FAILED! Error code: %s", esp_err_to_name(ret));
        
        // 3. If OTA fails, clear the flag to allow MQTT task to resume
        is_ota_running = false; 
    }
    
    // CLEAN UP RAM 
    free(firmware_url);  
    vTaskDelete(NULL);   
}

// Interface function to be called from other modules
void start_ota_process(const char* url) {
    char *url_copy = strdup(url);
    
    if(url_copy != NULL) {
        // 4. Set the flag IMMEDIATELY to signal MQTT to pause
        is_ota_running = true; 
        vTaskDelay(500 / portTICK_PERIOD_MS); // Wait 0.5s for MQTT to stop gracefully
        
        xTaskCreate(ota_task, "ota_task", 8192, (void *)url_copy, 5, NULL);
    } else {
        ESP_LOGE(TAG, "RAM Overflow Error: Cannot allocate memory to copy URL!");
    }
}