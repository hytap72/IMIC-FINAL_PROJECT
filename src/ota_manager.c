#include "ota_manager.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_app_desc.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "esp_http_server.h"

static const char *TAG = "OTA_MANAGER";

static volatile bool s_ota_in_progress = false;
static char s_device_ip[16] = "0.0.0.0";

static void ota_task(void *pvParameter)
{
    char *url = (char *)pvParameter;
    ESP_LOGI(TAG, "Bat dau OTA tu: %s", url);

    esp_http_client_config_t http_config = {
        .url            = url,
        .timeout_ms     = 30000,
        .keep_alive_enable = true,
    };
    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA thanh cong, khoi dong lai sau 1s...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA loi: %s", esp_err_to_name(ret));
    }

    free(url);
    s_ota_in_progress = false;
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

/* Trả về CORS headers để dashboard (chạy ở origin khác) có thể gọi /update */
static void add_cors_headers(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
}

static esp_err_t update_options_handler(httpd_req_t *req)
{
    add_cors_headers(req);
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/* Nhận file firmware (.bin) ghi trực tiếp từ POST body vào partition OTA
 * kế tiếp, sau đó đặt làm partition boot và khởi động lại */
static esp_err_t update_post_handler(httpd_req_t *req)
{
    add_cors_headers(req);

    if (s_ota_in_progress) {
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_send(req, "OTA dang chay, vui long doi", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    s_ota_in_progress = true;

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        s_ota_in_progress = false;
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    esp_ota_handle_t ota_handle;
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin loi: %s", esp_err_to_name(err));
        s_ota_in_progress = false;
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char buf[1024];
    int remaining = req->content_len;
    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buf, remaining < (int)sizeof(buf) ? remaining : (int)sizeof(buf));
        if (recv_len <= 0) {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) continue;
            ESP_LOGE(TAG, "Loi nhan du lieu OTA");
            esp_ota_abort(ota_handle);
            s_ota_in_progress = false;
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        err = esp_ota_write(ota_handle, buf, recv_len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write loi: %s", esp_err_to_name(err));
            esp_ota_abort(ota_handle);
            s_ota_in_progress = false;
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        remaining -= recv_len;
    }

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end loi: %s", esp_err_to_name(err));
        s_ota_in_progress = false;
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition loi: %s", esp_err_to_name(err));
        s_ota_in_progress = false;
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_send(req, "OK, dang khoi dong lai...", HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "OTA qua HTTP thanh cong, khoi dong lai sau 1s...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}

esp_err_t ota_manager_start_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;

    httpd_handle_t server = NULL;
    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Khong khoi dong duoc HTTP server: %s", esp_err_to_name(err));
        return err;
    }

    httpd_uri_t update_post = {
        .uri      = "/update",
        .method   = HTTP_POST,
        .handler  = update_post_handler,
    };
    httpd_register_uri_handler(server, &update_post);

    httpd_uri_t update_options = {
        .uri      = "/update",
        .method   = HTTP_OPTIONS,
        .handler  = update_options_handler,
    };
    httpd_register_uri_handler(server, &update_options);

    ESP_LOGI(TAG, "OTA HTTP server san sang tai http://%s/update", s_device_ip);
    return ESP_OK;
}
