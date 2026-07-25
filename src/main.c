#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"
#include <time.h>

#include "i2c_bus.h"
#include "htu21d.h"
#include "max17043.h"
#include "bh_1750.h"
#include "mpu6050.h"
#include "driver_motor.h"
#include "ble_manager.h"
#include "aws_iot.h"
#include "tcp_client.h"
#include "udp_client.h"
#include "net_config.h"
#include "ota_manager.h"

static const char *TAG = "MAIN";

#define SENSOR_SAMPLE_INTERVAL_MS 2000

static htu21d_t   m_htu21d;
static max17043_t m_max17043;
static bh1750_t   m_bh1750;
static mpu6050_t  m_mpu6050;

/* Bảo vệ truy cập I2C bus dùng chung giữa sensor_task và các nơi khác */
static SemaphoreHandle_t i2c_mutex;

/* Giá trị sensor mới nhất, cập nhật định kỳ bởi sensor_task */

typedef struct
{
    float s_temp;
    float s_hum;
    float s_bat_volt;
    float s_bat_soc;
    float s_lux;

    float s_accel_x;
    float s_accel_y;
    float s_accel_z;

    float s_gyro_x;
    float s_gyro_y;
    float s_gyro_z;

    float s_heading;

} sensor_data_t;

static QueueHandle_t sensor_queue;


/* Hướng la bàn (độ, 0-360), tích phân gyro_z theo thời gian.
 * Đây là góc yaw tương đối (không phải hướng từ trường thật) và sẽ trôi dần
 * theo thời gian do sai số tích lũy của gyroscope. */


/* Theo dõi lỗi liên tiếp của từng cảm biến I2C. Khi một cảm biến bị NACK
 * (không có trên bus / mất kết nối) nhiều lần liên tiếp, tạm ngưng đọc
 * cảm biến đó một số chu kỳ để tránh dồn lỗi/timeout trên bus mỗi 2s,
 * điều này có thể chiếm nhiều thời gian giữ i2c_mutex và làm các task
 * khác (WiFi/BLE) bị trễ -> rớt kết nối hoặc watchdog reset. */
#define SENSOR_FAIL_THRESHOLD 3
#define SENSOR_BACKOFF_CYCLES 10  /* ~20s ở chu kỳ 2s */

typedef struct {
    uint8_t fail_count;
    uint8_t backoff;
} sensor_health_t;

static sensor_health_t hc_htu21d, hc_max17043, hc_bh1750;

static bool sensor_should_skip(sensor_health_t *h)
{
    if (h->backoff > 0) {
        h->backoff--;
        return true;
    }
    return false;
}

static void sensor_mark_result(sensor_health_t *h, bool ok)
{
    if (ok) {
        h->fail_count = 0;
        h->backoff = 0;
    } else if (h->fail_count < SENSOR_FAIL_THRESHOLD) {
        h->fail_count++;
        if (h->fail_count >= SENSOR_FAIL_THRESHOLD) {
            h->backoff = SENSOR_BACKOFF_CYCLES;
        }
    }
}

/* Hướng la bàn (độ, 0-360), tích phân gyro_z theo thời gian.
 * Đây là góc yaw tương đối (không phải hướng từ trường thật) và sẽ trôi dần
 * theo thời gian do sai số tích lũy của gyroscope. */
static volatile float s_heading  = 0.0f;

/* Theo dõi lỗi liên tiếp của từng cảm biến I2C. Khi một cảm biến bị NACK
 * (không có trên bus / mất kết nối) nhiều lần liên tiếp, tạm ngưng đọc
 * cảm biến đó một số chu kỳ để tránh dồn lỗi/timeout trên bus mỗi 2s,
 * điều này có thể chiếm nhiều thời gian giữ i2c_mutex và làm các task
 * khác (WiFi/BLE) bị trễ -> rớt kết nối hoặc watchdog reset. */
#define SENSOR_FAIL_THRESHOLD 3
#define SENSOR_BACKOFF_CYCLES 10  /* ~20s ở chu kỳ 2s */

typedef struct {
    uint8_t fail_count;
    uint8_t backoff;
} sensor_health_t;

static sensor_health_t hc_htu21d, hc_max17043, hc_bh1750;

static bool sensor_should_skip(sensor_health_t *h)
{
    if (h->backoff > 0) {
        h->backoff--;
        return true;
    }
    return false;
}

static void sensor_mark_result(sensor_health_t *h, bool ok)
{
    if (ok) {
        h->fail_count = 0;
        h->backoff = 0;
    } else if (h->fail_count < SENSOR_FAIL_THRESHOLD) {
        h->fail_count++;
        if (h->fail_count >= SENSOR_FAIL_THRESHOLD) {
            h->backoff = SENSOR_BACKOFF_CYCLES;
        }
    }
}

/* Đọc các cảm biến I2C (HTU21D, MAX17043, BH1750) định kỳ */
static void sensor_task(void *pvParameters)
{
    sensor_data_t sensor_data = {
        .s_temp      = -999.0f,
        .s_hum       = -999.0f,
        .s_bat_volt  = -999.0f,
        .s_bat_soc   = -999.0f,
        .s_lux       = -999.0f,

        .s_accel_x   = 0.0f,
        .s_accel_y   = 0.0f,
        .s_accel_z   = 0.0f,

        .s_gyro_x    = 0.0f,
        .s_gyro_y    = 0.0f,
        .s_gyro_z    = 0.0f,

        .s_heading   = 0.0f
    };
    while (1) {
        if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (!sensor_should_skip(&hc_htu21d)) {
                float temp = htu21d_get_temperature(&m_htu21d);
                float hum  = htu21d_get_humidity(&m_htu21d);
                sensor_mark_result(&hc_htu21d, temp > -900.0f && hum > -900.0f);
                s_temp = temp;
                s_hum  = hum;
            }

            if (!sensor_should_skip(&hc_max17043)) {
                float bat_volt = max17043_get_battery_voltage(&m_max17043);
                float bat_soc  = max17043_get_soc(&m_max17043);
                sensor_mark_result(&hc_max17043, bat_volt > -900.0f && bat_soc > -900.0f);
                s_bat_volt = bat_volt;
                s_bat_soc  = bat_soc;
            }

            if (!sensor_should_skip(&hc_bh1750)) {
                float lux = bh1750_read_lux(&m_bh1750);
                sensor_mark_result(&hc_bh1750, lux > -900.0f);
                s_lux = lux;
            }

            if (mpu6050_read(&m_mpu6050) == ESP_OK) {
                s_accel_x = m_mpu6050.accel_x;
                s_accel_y = m_mpu6050.accel_y;
                s_accel_z = m_mpu6050.accel_z;
                s_gyro_x  = m_mpu6050.gyro_x;
                s_gyro_y  = m_mpu6050.gyro_y;
                s_gyro_z  = m_mpu6050.gyro_z;

                /* Tích phân gyro_z (độ/giây) theo chu kỳ lấy mẫu để ra góc heading */
                float heading = s_heading + s_gyro_z * (SENSOR_SAMPLE_INTERVAL_MS / 1000.0f);
                heading = fmodf(heading, 360.0f);
                if (heading < 0.0f) heading += 360.0f;
                s_heading = heading;
            }

            xSemaphoreGive(i2c_mutex);
            xQueueOverwrite(sensor_queue, &sensor_data);
        }

        ESP_LOGI(TAG, "Temp: %.2f C, Hum: %.2f %%, Batt: %.2f V (%.1f %%), Lux: %.2f",
                 s_temp, s_hum, s_bat_volt, s_bat_soc, s_lux);
        ESP_LOGI(TAG, "Accel: %.2f, %.2f, %.2f g | Gyro: %.2f, %.2f, %.2f deg/s | Heading: %.1f deg",
                 s_accel_x, s_accel_y, s_accel_z, s_gyro_x, s_gyro_y, s_gyro_z, s_heading);

        vTaskDelay(pdMS_TO_TICKS(SENSOR_SAMPLE_INTERVAL_MS));
    }
}

/* Gửi telemetry (sensor + battery + trạng thái motor) định kỳ
 * lên AWS IoT (MQTT) và UDP server */
static void telemetry_task(void *pvParameters)
{
    sensor_data_t sensor_data;
    motor_cmd_t motor_state;
    while (1) {
        float temp     = s_temp;
        float hum      = s_hum;
        float bat_volt = s_bat_volt;
        float bat_soc  = s_bat_soc;
        float heading  = s_heading;
        motor_cmd_t motor_state = driver_motor_get_state();

        if (aws_iot_is_connected()) {
            aws_iot_publish_telemetry(temp, hum, bat_volt, bat_soc, heading, (uint8_t)motor_state);
        }


        if (udp_client_is_ready()) {
            char payload[112];
            int len = snprintf(payload, sizeof(payload),
                                "{\"temp\":%.2f,\"hum\":%.2f,\"batt_v\":%.2f,\"soc\":%.1f,\"heading\":%.1f,\"motor\":%d}",
                                temp, hum, bat_volt, bat_soc, heading, (int)motor_state);
            udp_client_send(payload, len);
        }

        vTaskDelay(pdMS_TO_TICKS(TELEMETRY_INTERVAL_MS));
    }
}

/* Đồng bộ giờ qua SNTP — bắt buộc để TLS xác thực chứng chỉ AWS IoT
 * (mặc định ESP32 khởi động với đồng hồ ở năm 1970, khiến chứng chỉ
 * server bị coi là "chưa có hiệu lực") */
static void sync_time(void)
{
    ESP_LOGI(TAG, "Dang dong bo gio qua SNTP...");
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    config.server_from_dhcp = false;
    esp_netif_sntp_init(&config);

    if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(20000)) != ESP_OK) {
        ESP_LOGW(TAG, "Dong bo gio that bai (timeout)");
    } else {
        time_t now = time(NULL);
        ESP_LOGI(TAG, "Da dong bo gio: %s", ctime(&now));
    }
}

/* Gọi khi WiFi STA kết nối thành công (có IP) — khởi động các kết nối mạng */
static void on_wifi_connected(void)
{
    /* WiFi/mạng hoạt động tốt -> xác nhận firmware hiện tại ổn định,
     * tránh bị bootloader tự rollback về firmware cũ sau OTA */
    esp_ota_mark_app_valid_cancel_rollback();

    sync_time();
    aws_iot_start();
    tcp_client_start();
    udp_client_start();
    xTaskCreate(telemetry_task, "telemetry_task", 4096, NULL, 5, NULL);
}

void app_main(void)
{
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();

    /* Tạo I2C bus dùng chung cho tất cả sensor */
    i2c_master_bus_handle_t i2c_bus;
    ESP_ERROR_CHECK(i2c_bus_init(&i2c_bus));

    /* Thêm các sensor vào bus */
    htu21d_init(i2c_bus, &m_htu21d);
    max17043_init(i2c_bus, &m_max17043);
    bh1750_init(i2c_bus, &m_bh1750);
    mpu6050_init(i2c_bus, &m_mpu6050);
    driver_motor_init();

    i2c_mutex = xSemaphoreCreateMutex();
    sensor_queue = xQueueCreate(1, sizeof(sensor_data_t));
    if(sensor_queue == NULL) 
    {
        ESP_LOGE(TAG, "Create sensor queue failed!");
        abort();
    }

    ble_manager_set_wifi_connected_cb(on_wifi_connected);
    ble_manager_init("IMIC_Robot");

    /* Thử kết nối WiFi mặc định ngay khi khởi động (không cần BLE) */
    ble_manager_connect_wifi(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASSWORD);

    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
}
