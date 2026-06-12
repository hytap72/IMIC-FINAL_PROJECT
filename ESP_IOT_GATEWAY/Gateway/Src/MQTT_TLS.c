#include "global.h"

static const char *TAG = "MQTT_CLIENT";
extern bool is_ota_running;

struct NetworkContext
{
    mbedtls_ssl_context *ssl;
};

// TLS Send Function
int32_t esp32_mbedtls_transport_send(NetworkContext_t* pNetworkContext, const void* pBuffer, size_t bytesToSend){
    int ret = mbedtls_ssl_write(pNetworkContext->ssl, (const unsigned char *)pBuffer, bytesToSend);
    return (ret > 0) ? ret : -1;
}

// TLS Receive Function
int32_t esp32_mbedtls_transport_recv(NetworkContext_t* pNetworkContext, void* pBuffer, size_t bytesToRecv){
    int ret = mbedtls_ssl_read(pNetworkContext->ssl, (unsigned char *)pBuffer, bytesToRecv);
    
    if (ret > 0) {
        return ret; 
    } else if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE || ret == MBEDTLS_ERR_SSL_TIMEOUT) {
        // Equivalent to EAGAIN in POSIX -> Timeout, no data, but network is still OK
        return 0; 
    } else {
        return -1; // Network disconnection error
    }
}


// Must return bool and take exactly 6 parameters
bool mqttEventCallback(MQTTContext_t * pContext,
                       MQTTPacketInfo_t * pPacketInfo,
                       MQTTDeserializedInfo_t * pDeserializedInfo,
                       MQTTSuccessFailReasonCode_t * pReasonCode,
                       MQTTPropBuilder_t * pAckPropertyBuilder,
                       MQTTPropBuilder_t * pPropertyBuilder) 
{
    // 1. If data is received (PUBLISH from web)
    if ((pPacketInfo->type & 0xF0) == MQTT_PACKET_TYPE_PUBLISH) {
        MQTTPublishInfo_t * pPublishInfo = pDeserializedInfo->pPublishInfo;
        // Allocate memory to read the string safely
        char *payload = malloc(pPublishInfo->payloadLength + 1);
        if (payload != NULL) {
            memcpy(payload, pPublishInfo->pPayload, pPublishInfo->payloadLength);
            payload[pPublishInfo->payloadLength] = '\0'; // Null-terminate the string
            
            ESP_LOGI(TAG, "Topic: %.*s", pPublishInfo->topicNameLength, pPublishInfo->pTopicName);
            ESP_LOGI(TAG, "Payload: %s", payload);

            // Parse JSON to make decisions
            cJSON *root = cJSON_Parse(payload);
            if (root != NULL) {
                // A. Process OTA command
                cJSON *ota_cmd = cJSON_GetObjectItem(root, "ota_url");
                if (cJSON_IsString(ota_cmd) && (ota_cmd->valuestring != NULL)) {
                    ESP_LOGW(TAG, ">>> STARTING OTA DOWNLOAD FROM URL: %s", ota_cmd->valuestring);
                    start_ota_process(ota_cmd->valuestring); // Trigger OTA.c process
                }

                // B. Process Device Shadow (Toggle Relay/LED)
                cJSON *state = cJSON_GetObjectItem(root, "state");
                if (state != NULL) {
                    cJSON *led_cmd = cJSON_GetObjectItem(state, "led");
                    if (cJSON_IsString(led_cmd)) {
                        ESP_LOGW(TAG, ">>> SHADOW COMMAND: LED 1 -> %s", led_cmd->valuestring);
                        if (strcmp(led_cmd->valuestring, "ON") == 0) {
                            gpio_set_level(LED1_PIN, 1);
                        } else if (strcmp(led_cmd->valuestring, "OFF") == 0) {
                            gpio_set_level(LED1_PIN, 0);
                        }
                    }

                    // 2. Control Relay
                    cJSON *relay_cmd = cJSON_GetObjectItem(state, "relay");
                    if (cJSON_IsString(relay_cmd)) {
                        ESP_LOGW(TAG, ">>> SHADOW COMMAND: Relay 1 -> %s", relay_cmd->valuestring);
                        if (strcmp(relay_cmd->valuestring, "ON") == 0) {
                            gpio_set_level(RELAY_PIN, 1);
                        } else if (strcmp(relay_cmd->valuestring, "OFF") == 0) {
                            gpio_set_level(RELAY_PIN, 0);
                        }
                    }

                    // 3. Control LED 2
                    cJSON *led2_cmd = cJSON_GetObjectItem(state, "led2");
                    if (cJSON_IsString(led2_cmd)) {
                        ESP_LOGW(TAG, ">>> SHADOW COMMAND: LED 2 -> %s", led2_cmd->valuestring);
                        if (strcmp(led2_cmd->valuestring, "ON") == 0) {
                            gpio_set_level(LED2_PIN, 1);
                        } else if (strcmp(led2_cmd->valuestring, "OFF") == 0) {
                            gpio_set_level(LED2_PIN, 0);
                        }
                    }
                }
                cJSON_Delete(root);
            }
            free(payload);
        }
    } 
    // 2. If Connection Acknowledgment (CONNACK) is received
    else if (pPacketInfo->type == 0x20) { 
        if (pReasonCode != NULL) {
            ESP_LOGI(TAG, ">>> CONNACK packet received! Reason code: %d", *pReasonCode);
        } else {
            ESP_LOGI(TAG, ">>> CONNACK packet received!");
        }
    }
    // 3. If Subscribe Acknowledgment (SUBACK) is received
    else if (pPacketInfo->type == 0x90) { 
        ESP_LOGI(TAG, ">>> SUBACK received! Topic subscription successful.");
    }
    // 4. If Publish Acknowledgment (PUBACK) is received
    else if (pPacketInfo->type == 0x40) { 
        ESP_LOGI(TAG, ">>> PUBACK received! Data published successfully.");
    }
    // 5. Other system packets (PingReq, PingResp...)
    else {
        ESP_LOGI(TAG, "System packet received (Type: %X)", pPacketInfo->type);
    }
    
    // Must return true to instruct coreMQTT not to disconnect
    return true; 
}

uint32_t getTimeMs(void){
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

void mqtt_mbedtls_client_task(void *pvParameters) {
    const char *host_name = "a2ycbxa14yqj0d-ats.iot.ap-southeast-1.amazonaws.com";
    const char* port = "8883";

    mbedtls_net_context server_fd;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cacert, clicert;
    mbedtls_pk_context pkey;

    while(1) {
        // Initialize memory for mbedTLS
        mbedtls_net_init(&server_fd);
        mbedtls_ssl_init(&ssl);
        mbedtls_ssl_config_init(&conf);
        mbedtls_x509_crt_init(&cacert);
        mbedtls_x509_crt_init(&clicert);
        mbedtls_pk_init(&pkey);
        mbedtls_ctr_drbg_init(&ctr_drbg);
        mbedtls_entropy_init(&entropy);

        ESP_LOGI(TAG, "Seeding random number generator (Entropy)...");
        int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);
        if (ret != 0) { ESP_LOGE(TAG, "RNG Seed Error: -0x%x", -ret); goto exit_tls; }

        ESP_LOGI(TAG, "Loading certificates...");
        ret = mbedtls_x509_crt_parse(&cacert, (const unsigned char *)aws_root_ca, sizeof(aws_root_ca));
        ret |= mbedtls_x509_crt_parse(&clicert, (const unsigned char *)client_cert, sizeof(client_cert));
        // Standard call (7 parameters - Requires random function and drbg context at the end):
        ret |= mbedtls_pk_parse_key(&pkey, (const unsigned char *)client_key, sizeof(client_key), NULL, 0, mbedtls_ctr_drbg_random, &ctr_drbg);
        if (ret != 0) { ESP_LOGE(TAG, "Certificate Parsing Error: -0x%x", -ret); goto exit_tls; }

        ESP_LOGI(TAG, "Opening TCP connection to AWS...");
        ret = mbedtls_net_connect(&server_fd, host_name, port, MBEDTLS_NET_PROTO_TCP);
        if (ret != 0) { ESP_LOGE(TAG, "TCP connection error: -0x%x", -ret); goto exit_tls; }

        ESP_LOGI(TAG, "Configuring TLS...");
        mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
        mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
        mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
        mbedtls_ssl_conf_own_cert(&conf, &clicert, &pkey);
        mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);

        mbedtls_ssl_conf_read_timeout(&conf, 500); 

        mbedtls_ssl_setup(&ssl, &conf);
        if ((ret = mbedtls_ssl_set_hostname(&ssl, host_name)) != 0) {
            ESP_LOGE(TAG, "SNI Hostname setup error: -0x%x", -ret);
            goto exit_tls;
        }
        mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout);

        ESP_LOGI(TAG, "Performing TLS Handshake...");
        while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                ESP_LOGE(TAG, "Handshake error: -0x%x", -ret);
                goto exit_tls;
            }
        }
        ESP_LOGI(TAG, ">>> TLS CONNECTION SUCCESSFUL! Handing over to MQTT...");


        NetworkContext_t networkContext = {.ssl = &ssl};
        TransportInterface_t transport = {
            .pNetworkContext = &networkContext,
            .send = esp32_mbedtls_transport_send,
            .recv = esp32_mbedtls_transport_recv
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
        
        // INITIALIZE STATEFUL QOS WITH REQUIRED PARAMETERS:
        MQTT_InitStatefulQoS(&mqttContext, 
                             outgoingPublishes, 
                             5, 
                             incomingPublishes, 
                             5,
                             NULL, 
                             0);

        MQTTConnectInfo_t connectInfo = {0};
        connectInfo.cleanSession = true;
        connectInfo.pClientIdentifier = "ESP32_IMIC_FINAL_PROJECT";
        connectInfo.clientIdentifierLength = strlen(connectInfo.pClientIdentifier);
        connectInfo.keepAliveSeconds = 60;

        bool sessionPresent = false;
        ESP_LOGI(TAG, "Sending MQTT CONNECT packet...");
        MQTTStatus_t status = MQTT_Connect(&mqttContext, &connectInfo, NULL, 5000, &sessionPresent, NULL, NULL);
        
        if(status == MQTTSuccess){
            ESP_LOGI(TAG, "MQTT Connected successfully! Session Present: %d", sessionPresent); 
            
            // ==========================================================
            // A. PERFORM SUBSCRIBE (RAM cleared using memset)
            // ==========================================================
            MQTTSubscribeInfo_t subInfo[2];
            memset(subInfo, 0, sizeof(subInfo)); // <--- MOST IMPORTANT COMMAND
            
            subInfo[0].qos = MQTTQoS1;                      
            subInfo[0].pTopicFilter = "$aws/things/ESP32_IMIC_FINAL_PROJECT/shadow/update/delta";    
            subInfo[0].topicFilterLength = strlen(subInfo[0].pTopicFilter);

            subInfo[1].qos = MQTTQoS1;                      
            subInfo[1].pTopicFilter = "ota";    
            subInfo[1].topicFilterLength = strlen(subInfo[1].pTopicFilter);

            uint16_t packetId = MQTT_GetPacketId(&mqttContext);
            if(packetId == 0) packetId = 1; // <--- FORCE ID TO BE NON-ZERO
            
            ESP_LOGI(TAG, "Packaging SUBSCRIBE command...");
            status = MQTT_Subscribe(&mqttContext, subInfo, 2, packetId, NULL);
            if (status != MQTTSuccess) {
                ESP_LOGE(TAG, "Immediate Subscribe failure! Error code: %d", status);
            } else {
                ESP_LOGI(TAG, "SUBSCRIBE command pushed to TCP Socket!");
            }

            // ==========================================================
            // B. MAIN LOOP (PUBLISH and RECEIVE HANDLING)
            // ==========================================================
            uint32_t lastPublishTime = getTimeMs();
            while(1){
                status = MQTT_ProcessLoop(&mqttContext);
                if(status != MQTTSuccess){
                    ESP_LOGE(TAG, "MQTT_ProcessLoop failed: %d", status);
                    break;
                }
                
                if ((getTimeMs() - lastPublishTime) > 5000) {
                    
                    // IF OTA IS RUNNING -> SKIP PUBLISHING TO YIELD RAM
                    if (is_ota_running == true) {
                        ESP_LOGW(TAG, "OTA in progress, pausing MQTT publish to yield RAM & Bandwidth!");
                        lastPublishTime = getTimeMs(); // Reset timer to avoid log spam
                    } 
                    // IF OTA IS NOT RUNNING -> PUBLISH DATA AS NORMAL
                    else {
                        MQTTPublishInfo_t pubInfo;
                        memset(&pubInfo, 0, sizeof(MQTTPublishInfo_t)); // <--- CLEAR RAM
                        
                        pubInfo.qos = MQTTQoS1;
                        pubInfo.pTopicName = "telemetry"; // Your current topic
                        pubInfo.topicNameLength = strlen(pubInfo.pTopicName);
                        
                        char payload_buffer[150];
                        sprintf(payload_buffer, 
                            "{\"device_id\": \"ESP32_IMIC_FINAL_PROJECT\", \"temperature\": %d, \"humidity\": %d, \"light\": %d, \"battery\": %d}", 
                            (int)(esp_random() % 50),
                            (int)(esp_random() % 100),
                            (int)(esp_random() % 1000),
                            (int)(esp_random() % 100)
                        );
                        pubInfo.pPayload = payload_buffer;
                        pubInfo.payloadLength = strlen(payload_buffer);

                        uint16_t packetId = MQTT_GetPacketId(&mqttContext);
                        if(packetId == 0) packetId = 2; // <--- FORCE ID TO BE NON-ZERO
                        
                        ESP_LOGI(TAG, "Packaging PUBLISH QoS 1...");
                        
                        MQTTStatus_t pubStatus = MQTT_Publish(&mqttContext, &pubInfo, packetId, NULL);
                        if (pubStatus != MQTTSuccess) {
                            ESP_LOGE(TAG, "Immediate Publish failure! Error code: %d", pubStatus);
                        }
                        
                        lastPublishTime = getTimeMs();
                    }
                }
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }
        }
        exit_tls:
            ESP_LOGW(TAG, "Closing connection, cleaning RAM and waiting to Reconnect...");
            mbedtls_ssl_close_notify(&ssl);
            mbedtls_net_free(&server_fd);
            mbedtls_x509_crt_free(&cacert);
            mbedtls_x509_crt_free(&clicert);
            mbedtls_pk_free(&pkey);
            mbedtls_ssl_free(&ssl);
            mbedtls_ssl_config_free(&conf);
            mbedtls_ctr_drbg_free(&ctr_drbg);
            mbedtls_entropy_free(&entropy);
        
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}