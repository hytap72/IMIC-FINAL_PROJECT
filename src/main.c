#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/i2c_master.h"

#include "htu21d.h"
#include "max17043.h"
#include "driver_motor.h"
#include "ble_manager.h"
#include "aws_iot.h"
#include "tcp_client.h"
#include "udp_client.h"
#include "net_config.h"

static const char *TAG = "MAIN";

static void sensor_task(void *pvParameters)
{
    while (1) {
        float temp = htu21d_get_temperature();
        float hum  = htu21d_get_humidity();
        ESP_LOGI(TAG, "Temp: %.2f C, Hum: %.2f %%", temp, hum);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/* Gửi telemetry (sensor + battery + trạng thái motor) định kỳ
 * lên AWS IoT (MQTT) và UDP server */
static void telemetry_task(void *pvParameters)
{
    while (1) {
        float temp     = htu21d_get_temperature();
        float hum      = htu21d_get_humidity();
        float bat_volt = read_battery_voltage();
        float bat_soc  = read_soc();
        motor_cmd_t motor_state = driver_motor_get_state();

        if (aws_iot_is_connected()) {
            aws_iot_publish_telemetry(temp, hum, bat_volt, bat_soc, (uint8_t)motor_state);
        }

        if (udp_client_is_ready()) {
            char payload[96];
            int len = snprintf(payload, sizeof(payload),
                                "{\"temp\":%.2f,\"hum\":%.2f,\"batt_v\":%.2f,\"soc\":%.1f,\"motor\":%d}",
                                temp, hum, bat_volt, bat_soc, (int)motor_state);
            udp_client_send(payload, len);
        }

        vTaskDelay(pdMS_TO_TICKS(TELEMETRY_INTERVAL_MS));
    }
}

/* Gọi khi WiFi STA kết nối thành công (có IP) — khởi động các kết nối mạng */
static void on_wifi_connected(void)
{
    aws_iot_start();
    tcp_client_start();
    udp_client_start();
    xTaskCreate(telemetry_task, "telemetry_task", 4096, NULL, 5, NULL);
}

void app_main(void)
{
    nvs_flash_init();
    esp_event_loop_create_default();

    /* Tạo I2C bus dùng chung cho tất cả sensor */
    i2c_master_bus_handle_t i2c_bus;
    i2c_master_init(&i2c_bus);

    /* Thêm các sensor vào bus */
    max17043_init(i2c_bus);

    driver_motor_init();

    ble_manager_set_wifi_connected_cb(on_wifi_connected);
    ble_manager_init("IMIC_Robot");

    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
}
