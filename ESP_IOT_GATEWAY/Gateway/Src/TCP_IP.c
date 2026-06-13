#include "global.h"

#define HOST_IP_ADDR "192.168.1.67"
#define PORT 3333

static const char *TAG = "TCP_CLIENT";
static const char *payload = "Hello ASUS\n";

void tcp_client(void *pvParameters){
    char rx_buffer[128];
    char host_ip[] = HOST_IP_ADDR;
    int addr_family = 0;
    int protocol_ip = 0;

    while(1){
        struct sockaddr_in dest_addr;
        inet_pton(AF_INET, host_ip, &dest_addr.sin_addr);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PORT);
        addr_family = AF_INET;
        protocol_ip = IPPROTO_IP;

        int sock = socket(addr_family, SOCK_STREAM, protocol_ip);
        if(sock < 0){
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno); 
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "Socket created, connecting to %s:%d", host_ip, PORT);
        
        int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if(err != 0){
            ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
            close(sock);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "Successfully connected!!!");

        while(1){
            int err = send(sock, payload, strlen(payload), 0);
            if (err < 0){
                ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                break;
            }

            int len = recv(sock, rx_buffer, sizeof(rx_buffer)-1, 0);
            if(len < 0){
                ESP_LOGE(TAG, "Receive fail: errno %d", errno);
                break;
            }
            else if (len == 0) {
                ESP_LOGW(TAG, "Connection closed by server");
                break;
            }
            else{
                rx_buffer[len] = 0;
                ESP_LOGI(TAG, "Received %d bytes from %s", len, host_ip);
                ESP_LOGI(TAG, "%s", rx_buffer);
            }
            
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }
        
        if(sock != -1){
            ESP_LOGW(TAG, "Shutting down socket and restarting!");
            shutdown(sock, 0);
            close(sock);
        }
    }
}