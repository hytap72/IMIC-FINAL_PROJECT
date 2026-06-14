#include "mqtt_manager.h"

#include <string.h>
#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "MQTT_MANAGER";

static esp_mqtt_client_handle_t s_client = NULL;
static mqtt_data_cb_t s_on_data = NULL;
static mqtt_connected_cb_t s_on_connected = NULL;
static volatile bool s_connected = false;
static esp_mqtt_client_config_t s_mqtt_cfg;

/* Lưu lại các topic đã subscribe để tự động subscribe lại mỗi khi
 * (re)connect tới broker — esp_mqtt_client_subscribe() chỉ có tác dụng
 * khi client đã ở trạng thái connected. */
#define MQTT_MAX_SUBSCRIPTIONS 4
typedef struct {
    char topic[128];
    int qos;
} mqtt_subscription_t;

static mqtt_subscription_t s_subscriptions[MQTT_MAX_SUBSCRIPTIONS];
static int s_subscription_count = 0;

/* Buffer tạm để null-terminate topic/data trước khi đưa vào callback */
#define MQTT_TOPIC_BUF_LEN  128
#define MQTT_DATA_BUF_LEN   2048

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected to broker");
        s_connected = true;
        /* Subscribe lại tất cả topic đã đăng ký mỗi khi (re)connect */
        for (int i = 0; i < s_subscription_count; i++) {
            int msg_id = esp_mqtt_client_subscribe(s_client, s_subscriptions[i].topic, s_subscriptions[i].qos);
            ESP_LOGI(TAG, "Subscribing to %s, msg_id=%d", s_subscriptions[i].topic, msg_id);
        }
        if (s_on_connected) {
            s_on_connected();
        }
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Disconnected from broker");
        s_connected = false;
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "Subscribed, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGW(TAG, "Unsubscribed, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "Published, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA: {
        if (!s_on_data) break;

        char topic[MQTT_TOPIC_BUF_LEN];
        char data[MQTT_DATA_BUF_LEN];

        int topic_len = event->topic_len < (int)sizeof(topic) - 1 ? event->topic_len : (int)sizeof(topic) - 1;
        int data_len  = event->data_len  < (int)sizeof(data)  - 1 ? event->data_len  : (int)sizeof(data)  - 1;

        memcpy(topic, event->topic, topic_len);
        topic[topic_len] = '\0';
        memcpy(data, event->data, data_len);
        data[data_len] = '\0';

        ESP_LOGI(TAG, "Data received, topic=%s, data=%s", topic, data);
        s_on_data(topic, data, data_len);
        break;
    }

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error, type=%d", event->error_handle->error_type);
        break;

    default:
        break;
    }
}

esp_err_t mqtt_manager_start(const mqtt_manager_config_t *config)
{
    if (s_client) {
        ESP_LOGW(TAG, "MQTT client already started");
        return ESP_ERR_INVALID_STATE;
    }

    s_on_data = config->on_data;
    s_on_connected = config->on_connected;

    s_mqtt_cfg = (esp_mqtt_client_config_t){
        .broker.address.uri = config->uri,
        .broker.verification.certificate = config->ca_cert,
        .credentials.client_id = config->client_id,
        .credentials.authentication.certificate = config->client_cert,
        .credentials.authentication.key = config->client_key,
        /* Mặc định 10s không đủ cho mTLS handshake (RSA) trên ESP32 */
        .network.timeout_ms = 30000,
        .network.reconnect_timeout_ms = 10000,
        /* Mặc định 1024 bytes không đủ chứa URL OTA presigned S3 (kèm
         * x-amz-security-token rất dài) -> message bị chia thành nhiều
         * MQTT_EVENT_DATA (chunk sau có topic rỗng) làm JSON bị cắt giữa. */
        .buffer.size = 2048,
    };

    s_client = esp_mqtt_client_init(&s_mqtt_cfg);
    if (!s_client) {
        ESP_LOGE(TAG, "Failed to init MQTT client");
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    esp_err_t ret = esp_mqtt_client_start(s_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "MQTT client started, connecting to %s", config->uri);
    return ESP_OK;
}

esp_err_t mqtt_manager_stop(void)
{
    if (!s_client) return ESP_ERR_INVALID_STATE;

    esp_mqtt_client_stop(s_client);
    esp_mqtt_client_destroy(s_client);
    s_client = NULL;
    s_connected = false;
    return ESP_OK;
}

esp_err_t mqtt_manager_reconnect(void)
{
    if (s_client) return ESP_ERR_INVALID_STATE;

    s_client = esp_mqtt_client_init(&s_mqtt_cfg);
    if (!s_client) {
        ESP_LOGE(TAG, "Failed to init MQTT client");
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    esp_err_t ret = esp_mqtt_client_start(s_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(ret));
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "MQTT client restarted");
    return ESP_OK;
}

int mqtt_manager_publish(const char *topic, const char *data, int len, int qos, int retain)
{
    if (!s_client || !s_connected) return -1;
    return esp_mqtt_client_publish(s_client, topic, data, len, qos, retain);
}

esp_err_t mqtt_manager_subscribe(const char *topic, int qos)
{
    if (!s_client) return ESP_ERR_INVALID_STATE;

    /* Lưu lại để tự động subscribe lại khi (re)connect */
    if (s_subscription_count < MQTT_MAX_SUBSCRIPTIONS) {
        strncpy(s_subscriptions[s_subscription_count].topic, topic, sizeof(s_subscriptions[0].topic) - 1);
        s_subscriptions[s_subscription_count].topic[sizeof(s_subscriptions[0].topic) - 1] = '\0';
        s_subscriptions[s_subscription_count].qos = qos;
        s_subscription_count++;
    }

    if (!s_connected) {
        /* Sẽ được subscribe khi MQTT_EVENT_CONNECTED bắn ra */
        return ESP_OK;
    }

    int msg_id = esp_mqtt_client_subscribe(s_client, topic, qos);
    return msg_id >= 0 ? ESP_OK : ESP_FAIL;
}

bool mqtt_manager_is_connected(void)
{
    return s_connected;
}
