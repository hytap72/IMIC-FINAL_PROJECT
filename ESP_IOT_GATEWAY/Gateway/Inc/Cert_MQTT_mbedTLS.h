#ifndef CERT_MQTT_MBEDTLS_H
#define CERT_MQTT_MBEDTLS_H

#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"


static const char aws_root_ca[] = 
"-----BEGIN CERTIFICATE-----\n"
"MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF\n"
"ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n"
"b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL\n"
"MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\n"
"b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj\n"
"ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM\n"
"9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw\n"
"IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6\n"
"VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L\n"
"93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm\n"
"jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\n"
"AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA\n"
"A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI\n"
"U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs\n"
"N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv\n"
"o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU\n"
"5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy\n"
"rqXRfboQnoZsG4q5WTP468SQvvG5\n"
"-----END CERTIFICATE-----\n";

static const char client_cert[] = 
"-----BEGIN CERTIFICATE-----\n"
"MIIDWTCCAkGgAwIBAgIURVk8z9JMMntoqzFqHDmJc2WfblwwDQYJKoZIhvcNAQEL\n"
"BQAwTTFLMEkGA1UECwxCQW1hem9uIFdlYiBTZXJ2aWNlcyBPPUFtYXpvbi5jb20g\n"
"SW5jLiBMPVNlYXR0bGUgU1Q9V2FzaGluZ3RvbiBDPVVTMB4XDTI2MDYwNjEzNTUy\n"
"MVoXDTQ5MTIzMTIzNTk1OVowHjEcMBoGA1UEAwwTQVdTIElvVCBDZXJ0aWZpY2F0\n"
"ZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAKxhHbycjGh7Fd9BQvNK\n"
"eW7VR+TPk0fAtW5HO6+CFClLuY4xK7eNsYb+ai9zKHyENyCow9RCZ8Rw7S17Qwqc\n"
"mbKfCRoChMchX8oyID8qBNZZSn1x28k9f4HAlJsMeyhxYvQ1ECfzjqtkjilI/Sd4\n"
"AZB7MVAY3Cpam6gK588LV3zahER1XvCrxShYMvxA54QZoa2GjpaUZlq//lwZyA1l\n"
"vN/oNyk1yAGljo3p0i4KV4x2svCrjeMMU2JryCPuE33DnME764LllFDdko2zgwoU\n"
"ZXJs1yow1pQ4k8VchYzBLZQEcQhFfSzCTggObO58mknT91NovAJkP4JnkaDp2MRh\n"
"lrsCAwEAAaNgMF4wHwYDVR0jBBgwFoAUdySEFklyiArHLZ1iPtTYIH0oyEUwHQYD\n"
"VR0OBBYEFNiQe7Ie7oUl1UEPBLrlOry0LN7MMAwGA1UdEwEB/wQCMAAwDgYDVR0P\n"
"AQH/BAQDAgeAMA0GCSqGSIb3DQEBCwUAA4IBAQBz3+UBB241FBVOv/hqH9IPxbZj\n"
"2EwyeQ4hhlmqAXgKlf3bUGnfCsKU/1tniWfjMyU7fLLaEXzWBDwFezysKSNOHEmF\n"
"FHQyKBeSgXoOsqBGeXk7yNRXljB94Cq9e9NzWZr3X06oVUhpF6H2f42kBKtFGf/K\n"
"T//5XymHwEtbJhFkx9MHvLhkWWsUSr3FAEkONA0OWh4/tL//xKSYGeI+gywsBxYr\n"
"ZAdxmxVbJq7Fb0I6j2Lrw8p1xFnfb6QHz/uDDWIt3+IrddE/kw5Y9KoTS8xzxXbk\n"
"bd1bWJGttEU6F2Dzb/grbcfRN+aWs9/Oxm3f4ZZ4TWayAiOBu4mqffFF1+Cz\n"
"-----END CERTIFICATE-----\n";

static const char client_key[] = 
"-----BEGIN RSA PRIVATE KEY-----\n"
"MIIEpAIBAAKCAQEArGEdvJyMaHsV30FC80p5btVH5M+TR8C1bkc7r4IUKUu5jjEr\n"
"t42xhv5qL3MofIQ3IKjD1EJnxHDtLXtDCpyZsp8JGgKExyFfyjIgPyoE1llKfXHb\n"
"yT1/gcCUmwx7KHFi9DUQJ/OOq2SOKUj9J3gBkHsxUBjcKlqbqArnzwtXfNqERHVe\n"
"8KvFKFgy/EDnhBmhrYaOlpRmWr/+XBnIDWW83+g3KTXIAaWOjenSLgpXjHay8KuN\n"
"4wxTYmvII+4TfcOcwTvrguWUUN2SjbODChRlcmzXKjDWlDiTxVyFjMEtlARxCEV9\n"
"LMJOCA5s7nyaSdP3U2i8AmQ/gmeRoOnYxGGWuwIDAQABAoIBAGd+Ky9ZQgdclsYi\n"
"VYLiHMQJMw/R/FfhAEzEYbY+v8IHX9FZ/ihG3uXwi2oPlqgjbUGjcWdXsxtCvXNI\n"
"BPkzCiguyXUfO/6eL/eiXq/tl1fl5g4otM8+p/YL3Gy2cp7WbJW34gpKdUa7qYpp\n"
"8uumZjILdJDlTBH1smySl5g78/vV7UvVGZayXYtPPiHf/BJQ56DzXpk/R9OHqvYA\n"
"yUgyWXxIv7ZMJ41bEQdk7MsGmpwGh4ehebAPK9Gyz97gfvu8kw71maDP/lwKWTNc\n"
"UnPqLeaYJDfiOPgZhfXPFMindQa877ud3LpkTvcIe/0puiE4dRmghRn9zKn8hpLW\n"
"BcVQk3kCgYEA3zI4Zv12H5lACf7DIxsZf2zp73mKlyZzEqKxmVLPc7HXVhrINqgr\n"
"R4nnfh2rEgh/iZC7jqiMmWCqFGRKxhw/Dh5ecmkQGIynBtL0vuDqGvJ8/HowRVuT\n"
"klSdDofCRXEV3k24kX/BmjMNmd4SqSKpOqysFphlH94TuX3deIojXJcCgYEAxbbo\n"
"aTsmKQ05qGGQBiWg+xRUnRsCWKY4l5nd0cz7hk08VYsWglRxvskaJXrTAabOKzpd\n"
"M6OblJp+D1GuNSTOACqI8MRblPinsZ32/Ndlwk6AoA9SZ0Rg6USGYCONyQq/ol9g\n"
"AYdMiweDuQxNwv3jQEV/XABIxQmqxSmVTT18x30CgYEAn2Qr51DGlJZKp1iqXl2x\n"
"/c/32C8CDCNHNl29WvNKdyLf1vvVU0MRdUtEaEojqwMqoUEc0CoKDlZ734gn5gax\n"
"+REy/Z2OAxofBb6NTestBAV7wIo6Aq39WxjV+FR3JkQ2C8WRM0b7KXWgFuwCg4rf\n"
"M6mwU4qNXQ6pI2SBTt3Hlw0CgYEAw358xXg/3j1+clitkPZp+3l5xLItGryYnoX8\n"
"h/MYDQ6Xhrn1cr7OjWfJW5/bvflSR/n4qBOwxToRRVJtX4zZx95G81Ikf+Nx5LUd\n"
"v5m3u23lzt33i7ZX4K7p82uUSWJAY+vMQRan/5xZiYJ57UEfD7DoIm8BhRpu6W6Z\n"
"evs+Qv0CgYBzJCztNUMwP+9IGYf/pk/eCz6XEmKbO3fQlkfwJIu8zCVz6lBWR45p\n"
"zvu0G+2tz8GU7kbzV5UvyWr3AaeJUTxD5KkvWuNq/VbfKMzU3z2/CcVBhRkkM/Jt\n"
"fhkaxG1j50KTjC5ASP/0/pi4mXBs14+rfIhg1zPHHa1/LVOOW3GMug==\n"
"-----END RSA PRIVATE KEY-----\n";


int32_t esp32_mbedtls_transport_send(NetworkContext_t* pNetworkContext, const void* pBuffer, size_t bytesToSend);

int32_t esp32_mbedtls_transport_recv(NetworkContext_t* pNetworkContext, void* pBuffer, size_t bytesToRecv);

void mqtt_mbedtls_client_task(void *pvParameters);

#endif