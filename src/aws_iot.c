#include "aws_iot.h"
#include "mqtt_manager.h"
#include "net_config.h"
#include "aws_certs.h"
#include "driver_motor.h"
#include "ota_manager.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "AWS_IOT";

/* ─── Xử lý message nhận từ topic lệnh (AWS_IOT_TOPIC_CMD) ───
 * Payload JSON kỳ vọng: {"cmd": 1}  (giá trị theo motor_cmd_t) */
static void aws_iot_on_data(const char *topic, const char *data, int data_len)
{
    if (strcmp(topic, AWS_IOT_TOPIC_CMD) != 0) return;

    cJSON *root = cJSON_ParseWithLength(data, data_len);
    if (!root) {
        ESP_LOGW(TAG, "Invalid JSON command: %.*s", data_len, data);
        return;
    }

    cJSON *cmd_item = cJSON_GetObjectItem(root, "cmd");
    if (cJSON_IsNumber(cmd_item)) {
        motor_cmd_t cmd = (motor_cmd_t)cmd_item->valueint;
        ESP_LOGI(TAG, "AWS IoT command: 0x%02X", cmd);
        driver_motor_handle_cmd(cmd);
    }

    /* Lệnh OTA: {"ota_url": "http://.../firmware.bin"} */
    cJSON *ota_item = cJSON_GetObjectItem(root, "ota_url");
    if (cJSON_IsString(ota_item) && ota_item->valuestring[0] != '\0') {
        ESP_LOGI(TAG, "AWS IoT OTA command: %s", ota_item->valuestring);
        ota_manager_start(ota_item->valuestring);
    }

    cJSON_Delete(root);
}

/* Gửi ngay thông tin thiết bị (IP, phiên bản firmware...) mỗi khi MQTT
 * (re)connect, để dashboard biết IP thiết bị mà không phải đợi vòng
 * telemetry định kỳ đầu tiên */
static void aws_iot_on_connected(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "thing", AWS_IOT_THING_NAME);
    cJSON_AddStringToObject(root, "fw_version", ota_manager_get_version());
    cJSON_AddBoolToObject(root, "ota_in_progress", ota_manager_is_in_progress());
    cJSON_AddStringToObject(root, "device_ip", ota_manager_get_device_ip());

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) return;

    mqtt_manager_publish(AWS_IOT_TOPIC_DATA, payload, 0, 0, 0);
    cJSON_free(payload);
}

esp_err_t aws_iot_start(void)
{
    static char uri[96];
    snprintf(uri, sizeof(uri), "mqtts://%s:%d", AWS_IOT_ENDPOINT, AWS_IOT_PORT);

    mqtt_manager_config_t cfg = {
        .uri          = uri,
        .client_id    = AWS_IOT_THING_NAME,
        .ca_cert      = AWS_ROOT_CA,
        .client_cert  = AWS_DEVICE_CERT,
        .client_key   = AWS_PRIVATE_KEY,
        .on_data      = aws_iot_on_data,
        .on_connected = aws_iot_on_connected,
    };

    /* Cho ota_manager publish trạng thái OTA ngay trước khi nó tạm dừng
     * MQTT để tải firmware -> dashboard thấy "Đang cập nhật..." */
    ota_manager_set_status_cb(aws_iot_on_connected);

    esp_err_t ret = mqtt_manager_start(&cfg);
    if (ret != ESP_OK) return ret;

    return mqtt_manager_subscribe(AWS_IOT_TOPIC_CMD, 1);
}

esp_err_t aws_iot_publish_telemetry(float temperature, float humidity,
                                     float battery_voltage, float battery_soc,
                                     float heading, uint8_t motor_state)
{
    if (!mqtt_manager_is_connected()) return ESP_ERR_INVALID_STATE;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "thing", AWS_IOT_THING_NAME);
    cJSON_AddNumberToObject(root, "temperature", temperature);
    cJSON_AddNumberToObject(root, "humidity", humidity);
    cJSON_AddNumberToObject(root, "battery_voltage", battery_voltage);
    cJSON_AddNumberToObject(root, "battery_soc", battery_soc);
    cJSON_AddNumberToObject(root, "heading", heading);
    cJSON_AddNumberToObject(root, "motor_state", motor_state);
    cJSON_AddStringToObject(root, "fw_version", ota_manager_get_version());
    cJSON_AddBoolToObject(root, "ota_in_progress", ota_manager_is_in_progress());
    cJSON_AddStringToObject(root, "device_ip", ota_manager_get_device_ip());

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) return ESP_ERR_NO_MEM;

    int msg_id = mqtt_manager_publish(AWS_IOT_TOPIC_DATA, payload, 0, 0, 0);
    cJSON_free(payload);

    return msg_id >= 0 ? ESP_OK : ESP_FAIL;
}

bool aws_iot_is_connected(void)
{
    return mqtt_manager_is_connected();
}
