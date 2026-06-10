#include "udp_client.h"
#include "net_config.h"
#include "driver_motor.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

static const char *TAG = "UDP_CLIENT";

static int s_sock = -1;
static struct sockaddr_in s_dest_addr;

static void udp_client_task(void *pvParameters)
{
    (void)pvParameters;
    uint8_t rx_buf[64];

    while (1) {
        struct sockaddr_in source_addr;
        socklen_t socklen = sizeof(source_addr);

        int len = recvfrom(s_sock, rx_buf, sizeof(rx_buf), 0,
                            (struct sockaddr *)&source_addr, &socklen);
        if (len < 0) {
            ESP_LOGW(TAG, "recvfrom failed: errno %d", errno);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        for (int i = 0; i < len; i++) {
            driver_motor_handle_cmd((motor_cmd_t)rx_buf[i]);
        }
    }
}

esp_err_t udp_client_start(void)
{
    if (s_sock >= 0) {
        ESP_LOGW(TAG, "UDP client already started");
        return ESP_ERR_INVALID_STATE;
    }

    s_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (s_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return ESP_FAIL;
    }

    memset(&s_dest_addr, 0, sizeof(s_dest_addr));
    s_dest_addr.sin_family = AF_INET;
    s_dest_addr.sin_port   = htons(UDP_SERVER_PORT);
    inet_pton(AF_INET, UDP_SERVER_IP, &s_dest_addr.sin_addr);

    /* Bind cùng port để vừa gửi telemetry vừa nhận lệnh */
    struct sockaddr_in bind_addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(UDP_SERVER_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(s_sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) != 0) {
        ESP_LOGW(TAG, "bind failed: errno %d (vẫn dùng được để gửi)", errno);
    }

    BaseType_t ret = xTaskCreate(udp_client_task, "udp_client", 4096, NULL, 5, NULL);
    if (ret != pdPASS) {
        close(s_sock);
        s_sock = -1;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "UDP client ready, target %s:%d", UDP_SERVER_IP, UDP_SERVER_PORT);
    return ESP_OK;
}

int udp_client_send(const void *data, size_t len)
{
    if (s_sock < 0) return -1;
    return sendto(s_sock, data, len, 0,
                   (struct sockaddr *)&s_dest_addr, sizeof(s_dest_addr));
}

bool udp_client_is_ready(void)
{
    return s_sock >= 0;
}
