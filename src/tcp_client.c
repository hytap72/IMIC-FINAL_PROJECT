#include "tcp_client.h"
#include "net_config.h"
#include "driver_motor.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

static const char *TAG = "TCP_CLIENT";

static int s_sock = -1;
static SemaphoreHandle_t s_sock_mutex;
static volatile bool s_connected = false;

static void tcp_client_task(void *pvParameters)
{
    (void)pvParameters;
    uint8_t rx_buf[64];

    while (1) {
        struct sockaddr_in dest = {
            .sin_family = AF_INET,
            .sin_port   = htons(TCP_SERVER_PORT),
        };
        inet_pton(AF_INET, TCP_SERVER_IP, &dest.sin_addr);

        int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        ESP_LOGI(TAG, "Connecting to %s:%d ...", TCP_SERVER_IP, TCP_SERVER_PORT);
        if (connect(sock, (struct sockaddr *)&dest, sizeof(dest)) != 0) {
            ESP_LOGW(TAG, "Connect failed: errno %d, retrying in 5s", errno);
            close(sock);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        ESP_LOGI(TAG, "Connected to TCP server");
        xSemaphoreTake(s_sock_mutex, portMAX_DELAY);
        s_sock = sock;
        s_connected = true;
        xSemaphoreGive(s_sock_mutex);

        while (1) {
            int len = recv(sock, rx_buf, sizeof(rx_buf), 0);
            if (len < 0) {
                ESP_LOGW(TAG, "recv failed: errno %d", errno);
                break;
            } else if (len == 0) {
                ESP_LOGW(TAG, "Connection closed by server");
                break;
            }

            for (int i = 0; i < len; i++) {
                driver_motor_handle_cmd((motor_cmd_t)rx_buf[i]);
            }
        }

        xSemaphoreTake(s_sock_mutex, portMAX_DELAY);
        s_connected = false;
        s_sock = -1;
        xSemaphoreGive(s_sock_mutex);
        close(sock);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

esp_err_t tcp_client_start(void)
{
    if (s_sock_mutex) {
        ESP_LOGW(TAG, "TCP client already started");
        return ESP_ERR_INVALID_STATE;
    }

    s_sock_mutex = xSemaphoreCreateMutex();
    if (!s_sock_mutex) return ESP_ERR_NO_MEM;

    BaseType_t ret = xTaskCreate(tcp_client_task, "tcp_client", 4096, NULL, 5, NULL);
    return ret == pdPASS ? ESP_OK : ESP_FAIL;
}

int tcp_client_send(const void *data, size_t len)
{
    if (!s_sock_mutex) return -1;

    int ret = -1;
    xSemaphoreTake(s_sock_mutex, portMAX_DELAY);
    if (s_connected && s_sock >= 0) {
        ret = send(s_sock, data, len, 0);
    }
    xSemaphoreGive(s_sock_mutex);

    return ret;
}

bool tcp_client_is_connected(void)
{
    return s_connected;
}
