#include "global.h"

#define HOST_IP_ADDR "192.168.1.67"
#define PORT 3333

static const char *TAG = "UDP_CLIENT";
static const char *payload = "Hello ASUS\n";

void udp_client(void *pvParameters){
    char rx_buffer[128];
    char host_ip[] = HOST_IP_ADDR;
    int addr_family = 0;
    int ip_protocol = 0;

    while(1){
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr(HOST_IP_ADDR);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if(sock < 0){
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }

        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        ESP_LOGI(TAG, "Socket created, sending to %s:%d", host_ip, PORT);

        while(1){
            int err = sendto(sock, payload, strlen(payload), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            if (err >= 0) {
                struct sockaddr_in temp_addr;
                socklen_t temp_len = sizeof(temp_addr);
                getsockname(sock, (struct sockaddr *)&temp_addr, &temp_len);
                ESP_LOGW("UDP", "PORT CUA ESP32 LA: %d", ntohs(temp_addr.sin_port));
            }
            if(err < 0){
                ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                break;
            }
            ESP_LOGI(TAG, "Message sent!");

            struct sockaddr_storage source_addr;
            socklen_t socklen = sizeof(source_addr);
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer)-1, 0, (struct sockaddr *)&source_addr, &socklen);
            if(len < 0){
                ESP_LOGE(TAG, "Recvfrom failed: errno %d", errno);
                break;
            }
            else{
                rx_buffer[len] = 0;
                ESP_LOGI(TAG, "Received %d bytes form %s: ", len, host_ip);
                ESP_LOGI(TAG, "%s", rx_buffer);
            }
            vTaskDelay(2000 / portTICK_PERIOD_MS); 
        }
        if(sock != -1){
            ESP_LOGE(TAG, "Shutting dowm socket and restarting!");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}