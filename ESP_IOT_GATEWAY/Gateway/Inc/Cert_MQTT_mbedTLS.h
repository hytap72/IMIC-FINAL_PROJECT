#ifndef CERT_MQTT_MBEDTLS_H
#define CERT_MQTT_MBEDTLS_H

#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"


static const char aws_root_ca[] = 
"-----BEGIN CERTIFICATE-----\n"

"-----END CERTIFICATE-----\n";

static const char client_cert[] = 
"-----BEGIN CERTIFICATE-----\n"

"-----END CERTIFICATE-----\n";

static const char client_key[] = 
"-----BEGIN RSA PRIVATE KEY-----\n"

"-----END RSA PRIVATE KEY-----\n";


int32_t esp32_mbedtls_transport_send(NetworkContext_t* pNetworkContext, const void* pBuffer, size_t bytesToSend);

int32_t esp32_mbedtls_transport_recv(NetworkContext_t* pNetworkContext, void* pBuffer, size_t bytesToRecv);

void mqtt_mbedtls_client_task(void *pvParameters);

#endif