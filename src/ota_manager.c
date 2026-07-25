#include "ota_manager.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_app_desc.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"
#include "mqtt_manager.h"

static const char *TAG = "OTA_MANAGER";

static volatile bool s_ota_in_progress = false;
static char s_device_ip[16] = "0.0.0.0";
static ota_status_cb_t s_status_cb = NULL;

static void ota_task(void *pvParameter)
{
    char *url = (char *)pvParameter;
    ESP_LOGI(TAG, "Bat dau OTA tu: %s", url);

    esp_http_client_config_t http_config = {
        .url               = url,
        .timeout_ms        = 30000,
        .keep_alive_enable = true,
        .crt_bundle_attach = esp_crt_bundle_attach,
        /* URL presigned S3 (kèm x-amz-security-token) dài hơn buffer
         * header mặc định (512 bytes) -> "HTTP_CLIENT: Out of buffer" */
        .buffer_size       = 2048,
        .buffer_size_tx    = 2048,
    };
    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    /* Báo trạng thái "Đang cập nhật..." lên dashboard TRƯỚC khi dừng MQTT,
     * vì mqtt_manager_stop() ngay sau đây sẽ làm mất kết nối trong lúc tải */
    if (s_status_cb) s_status_cb();

    /* Giải phóng buffer/TLS context của MQTT (mTLS) để có đủ heap liên tục
     * cho kết nối TLS của HTTPS OTA -> tránh mbedtls_ssl_setup ALLOC_FAILED */
    mqtt_manager_stop();

    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA thanh cong, khoi dong lai sau 1s...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA loi: %s", esp_err_to_name(ret));
        mqtt_manager_reconnect();
    }

    free(url);
    s_ota_in_progress = false;
    if (s_status_cb) s_status_cb();
    vTaskDelete(NULL);
}

esp_err_t ota_manager_start(const char *url)
{
    if (s_ota_in_progress) {
        ESP_LOGW(TAG, "OTA dang chay, bo qua yeu cau moi");
        return ESP_ERR_INVALID_STATE;
    }

    char *url_copy = strdup(url);
    if (!url_copy) return ESP_ERR_NO_MEM;

    s_ota_in_progress = true;
    if (xTaskCreate(ota_task, "ota_task", 8192, url_copy, 5, NULL) != pdPASS) {
        free(url_copy);
        s_ota_in_progress = false;
        return ESP_FAIL;
    }

    return ESP_OK;
}

bool ota_manager_is_in_progress(void)
{
    return s_ota_in_progress;
}

const char *ota_manager_get_version(void)
{
    return esp_app_get_description()->version;
}

void ota_manager_set_device_ip(const char *ip)
{
    strncpy(s_device_ip, ip, sizeof(s_device_ip) - 1);
    s_device_ip[sizeof(s_device_ip) - 1] = '\0';
}

const char *ota_manager_get_device_ip(void)
{
    return s_device_ip;
}

void ota_manager_set_status_cb(ota_status_cb_t cb)
{
    s_status_cb = cb;
}
