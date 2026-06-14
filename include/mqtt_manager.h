#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

/* Callback nhận message từ topic đã subscribe.
 * topic/data đã được null-terminate, chỉ hợp lệ trong phạm vi callback. */
typedef void (*mqtt_data_cb_t)(const char *topic, const char *data, int data_len);

/* Callback gọi mỗi khi MQTT client (re)connect tới broker thành công */
typedef void (*mqtt_connected_cb_t)(void);

typedef struct {
    const char *uri;          /* "mqtts://host:port" hoặc "mqtt://host:port" */
    const char *client_id;    /* MQTT client id, NULL để auto-generate */
    const char *ca_cert;      /* PEM root CA, NULL nếu không xác thực broker */
    const char *client_cert;  /* PEM device cert, NULL nếu không dùng mTLS */
    const char *client_key;   /* PEM private key, NULL nếu không dùng mTLS */
    mqtt_data_cb_t on_data;   /* gọi khi có message trên topic đã subscribe */
    mqtt_connected_cb_t on_connected; /* gọi khi (re)connect thành công, NULL nếu không cần */
} mqtt_manager_config_t;

/* Khởi tạo và kết nối MQTT client (chạy nền, tự reconnect) */
esp_err_t mqtt_manager_start(const mqtt_manager_config_t *config);

/* Publish message. Trả về message_id (>=0) hoặc -1 nếu lỗi/chưa kết nối */
int mqtt_manager_publish(const char *topic, const char *data, int len, int qos, int retain);

/* Subscribe topic */
esp_err_t mqtt_manager_subscribe(const char *topic, int qos);

bool mqtt_manager_is_connected(void);

#endif
