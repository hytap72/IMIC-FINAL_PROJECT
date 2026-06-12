#include "global.h"

static const char *TAG = "MQTT_CLIENT";

struct NetworkContext
{
    int tcp_socket;
};

int32_t esp32_transport_send(NetworkContext_t* pNetworkContext, const void* pBuffer, size_t bytesToSend){
    int bytes_send = send(pNetworkContext->tcp_socket, pBuffer, bytesToSend, 0);
    return (bytes_send >= 0) ? bytes_send : -1;
}

int32_t esp32_transport_recv(NetworkContext_t* pNetworkContext, void* pBuffer, size_t bytesToRecv){
    int bytes_recv = recv(pNetworkContext->tcp_socket, pBuffer, bytesToRecv, 0);
    
    if (bytes_recv > 0) {
        return bytes_recv; // Đọc thành công
    } else if (bytes_recv == 0) {
        return -1; // Server chủ động đóng kết nối
    } else {
        // Nếu bytes_recv < 0 (Lỗi)
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS) {
            // Đây chỉ là timeout 500ms, mạng vẫn bình thường.
            // Trả về 0 để coreMQTT biết là không có data và tiếp tục đếm Keep-Alive
            return 0; 
        }
        return -1; // Lỗi đứt mạng thực sự
    }
}

uint32_t getTimeMs(void){
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

// Bắt buộc phải có kiểu trả về là bool và 6 tham số nhé
bool mqttEventCallback(MQTTContext_t * pContext,
                       MQTTPacketInfo_t * pPacketInfo,
                       MQTTDeserializedInfo_t * pDeserializedInfo,
                       MQTTSuccessFailReasonCode_t * pReasonCode,
                       MQTTPropBuilder_t * pAckPropertyBuilder,
                       MQTTPropBuilder_t * pPropertyBuilder) 
{
    // 1. Nếu nhận được dữ liệu (PUBLISH từ web gửi xuống)
    if ((pPacketInfo->type & 0xF0) == MQTT_PACKET_TYPE_PUBLISH) {
        MQTTPublishInfo_t * pPublishInfo = pDeserializedInfo->pPublishInfo;
        ESP_LOGI(TAG, "Nhận được tin nhắn từ Topic: %.*s", 
                 pPublishInfo->topicNameLength, pPublishInfo->pTopicName);
        ESP_LOGI(TAG, "Nội dung: %.*s", 
                 pPublishInfo->payloadLength, (const char *)pPublishInfo->pPayload);
    } 
    // 2. Nếu nhận được xác nhận Kết nối (CONNACK)
    else if (pPacketInfo->type == 0x20) { 
        if (pReasonCode != NULL) {
            ESP_LOGI(TAG, ">>> Đã nhận gói CONNACK! Mã xác nhận: %d", *pReasonCode);
        } else {
            ESP_LOGI(TAG, ">>> Đã nhận gói CONNACK!");
        }
    }
    // 3. Nếu nhận được xác nhận Đăng ký Topic (SUBACK)
    else if (pPacketInfo->type == 0x90) { 
        ESP_LOGI(TAG, ">>> Đã nhận SUBACK! Đăng ký Topic thành công.");
    }
    // 4. Nếu nhận được xác nhận Đã gửi dữ liệu (PUBACK)
    else if (pPacketInfo->type == 0x40) { 
        ESP_LOGI(TAG, ">>> Đã nhận PUBACK! Gửi dữ liệu nhiệt độ thành công.");
    }
    // 5. Các gói tin ẩn danh khác (PingReq, PingResp...)
    else {
        ESP_LOGI(TAG, "Nhận được gói tin hệ thống (Type: %X)", pPacketInfo->type);
    }
    
    // Bắt buộc trả về true để báo coreMQTT không ngắt kết nối
    return true; 
}

void mqtt_client_task(void *pvParameters) {
    const char *host_name = "broker.hivemq.com";
    const int port = 1883;

    while(1) {
        struct hostent *he = gethostbyname(host_name);
        if (he == NULL) {
            ESP_LOGE(TAG, "DNS lookup failed for %s", host_name);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            continue;
        }
        
        struct in_addr *addr = (struct in_addr *)he->h_addr;
        ESP_LOGI(TAG, "DNS Resolved %s to %s", host_name, inet_ntoa(*addr));

        struct sockaddr_in dest_addr;
        dest_addr.sin_addr = *addr;
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(port);

        int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket");
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            continue;
        }

        struct timeval tv = { .tv_sec = 0, .tv_usec = 500000 }; // 500ms
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0) {
            ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
            close(sock);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "TCP Connected to HiveMQ Public Broker!");

        NetworkContext_t networkContext = {.tcp_socket = sock};
        TransportInterface_t transport = {
            .pNetworkContext = &networkContext,
            .send = esp32_transport_send,
            .recv = esp32_transport_recv
        };

        uint8_t buffer[1024];
        MQTTFixedBuffer_t networkBuffer = {
            .pBuffer = buffer,
            .size = sizeof(buffer)
        };
        MQTTContext_t mqttContext;
        MQTT_Init(&mqttContext, &transport, getTimeMs, mqttEventCallback, &networkBuffer);

MQTTPubAckInfo_t outgoingPublishes[5];
        MQTTPubAckInfo_t incomingPublishes[5];
        memset(outgoingPublishes, 0, sizeof(outgoingPublishes));
        memset(incomingPublishes, 0, sizeof(incomingPublishes));
        
        // GỌI HÀM VỚI ĐẦY ĐỦ 5 THAM SỐ:
        MQTT_InitStatefulQoS(&mqttContext, 
                             outgoingPublishes, 
                             5, 
                             incomingPublishes, 
                             5,
                             NULL, 
                             0);

        MQTTConnectInfo_t connectInfo = {0};
        connectInfo.cleanSession = true;
        connectInfo.pClientIdentifier = "ESP32_Test_MQTT_IMIC";
        connectInfo.clientIdentifierLength = strlen(connectInfo.pClientIdentifier);
        connectInfo.keepAliveSeconds = 60;

        bool sessionPresent = false;
        ESP_LOGI(TAG, "Sending MQTT CONNECT packet...");
        MQTTStatus_t status = MQTT_Connect(&mqttContext, &connectInfo, NULL, 5000, &sessionPresent, NULL, NULL);
        
        if(status == MQTTSuccess){
            // Sửa luôn chữ Section thành Session cho chuẩn nhé
            ESP_LOGI(TAG, "MQTT Connected successfully! Session Present: %d", sessionPresent); 
            
            // ==========================================================
            // A. THỰC HIỆN SUBSCRIBE (Đã dọn sạch rác RAM bằng memset)
            // ==========================================================
            MQTTSubscribeInfo_t subInfo;
            memset(&subInfo, 0, sizeof(MQTTSubscribeInfo_t)); // <--- LỆNH QUAN TRỌNG NHẤT
            
            subInfo.qos = MQTTQoS1;                      
            subInfo.pTopicFilter = "esp32/control/IMIC/1401/#";    
            subInfo.topicFilterLength = strlen(subInfo.pTopicFilter);

            uint16_t packetId = MQTT_GetPacketId(&mqttContext);
            if(packetId == 0) packetId = 1; // <--- ÉP ID KHÁC 0
            
            ESP_LOGI(TAG, "Đang đóng gói SUBSCRIBE...");
            status = MQTT_Subscribe(&mqttContext, &subInfo, 1, packetId, NULL);
            if (status != MQTTSuccess) {
                ESP_LOGE(TAG, "Subscribe thất bại TẠI CHỖ! Mã lỗi: %d", status);
            } else {
                ESP_LOGI(TAG, "Đã đẩy lệnh SUBSCRIBE vào TCP Socket!");
            }

            // ==========================================================
            // B. VÒNG LẶP CHÍNH (PUBLISH và XỬ LÝ NHẬN)
            // ==========================================================
            uint32_t lastPublishTime = getTimeMs();
            while(1){
                status = MQTT_ProcessLoop(&mqttContext);
                if(status != MQTTSuccess){
                    ESP_LOGE(TAG, "MQTT_ProcessLoop failed: %d", status);
                    break;
                }
                
                if ((getTimeMs() - lastPublishTime) > 5000) {
                    MQTTPublishInfo_t pubInfo;
                    memset(&pubInfo, 0, sizeof(MQTTPublishInfo_t)); // <--- DỌN SẠCH RAM
                    
                    pubInfo.qos = MQTTQoS1;
                    pubInfo.pTopicName = "esp32/sensor/temp/IMIC/1401"; // Topic hiện tại của bạn
                    pubInfo.topicNameLength = strlen(pubInfo.pTopicName);
                    
                    char payload_buffer[50];
                    sprintf(payload_buffer, "{\"nhiet_do\": %d}", (int)(esp_random() % 100));
                    pubInfo.pPayload = payload_buffer;
                    pubInfo.payloadLength = strlen(payload_buffer);

                    packetId = MQTT_GetPacketId(&mqttContext);
                    if(packetId == 0) packetId = 2; // <--- ÉP ID KHÁC 0
                    
                    ESP_LOGI(TAG, "Đang đóng gói PUBLISH QoS 1...");
                    
                    // Ghi nhận trạng thái trả về của MQTT_Publish để biết có lỗi không
                    MQTTStatus_t pubStatus = MQTT_Publish(&mqttContext, &pubInfo, packetId, NULL);
                    if (pubStatus != MQTTSuccess) {
                        ESP_LOGE(TAG, "Publish thất bại TẠI CHỖ! Mã lỗi: %d", pubStatus);
                    }
                    
                    lastPublishTime = getTimeMs();
                }
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }
        }
        else{
            ESP_LOGE(TAG, "MQTT Connection failed! Error code: %d", status);
        }
        ESP_LOGW(TAG, "Shutting down socket and restarting!");
        shutdown(sock, 0);
        close(sock);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}


