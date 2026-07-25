#ifndef MQTT_H
#define MQTT_H

#include "sdkconfig.h"
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h> 
#include <arpa/inet.h>
#include "esp_netif.h"
#include "esp_log.h"
#include "core_mqtt.h"
#include "netdb.h"
#include <stdbool.h>

int32_t esp32_transport_send(NetworkContext_t* pNetworkContext, const void* pBuffer, size_t bytesToSend);

int32_t esp32_transport_recv(NetworkContext_t* pNetworkContext, void* pBuffer, size_t bytesToRecv);

uint32_t getTimeMs(void);

void mqtt_client_task(void *pvParameters);

#endif