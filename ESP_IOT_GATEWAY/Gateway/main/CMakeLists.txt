idf_component_register(
    SRCS "main.c"
         "../Src/BLE_WIFI.c"
         "../Src/GPIO.c"
         "../Src/MQTT_TLS.c"
         "../Src/OTA.c"
         
    INCLUDE_DIRS "." 
                 "../Inc"

    REQUIRES nvs_flash mbedtls esp_http_client esp_https_ota json coreMQTT coreHTTP esp_netif esp_wifi esp_event lwip bt driver
)