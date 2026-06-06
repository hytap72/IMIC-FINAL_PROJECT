#include "htu21d.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"

#define I2C_MASTER_SCL              22
#define I2C_MASTER_SDA              21
#define I2C_MASTER_CHANNEL          I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000
#define I2C_MASTER_TIMEOUT_MS       1000

#define HTU21D_ADDR                 0x40
#define HTU21D_TRIG_TEMP_MEASURE    0xF3
#define HTU21D_TRIG_HUMI_MEASURE    0xF5

static const char *TAG = "HTU21D";

// Private Functions

static esp_err_t htu21d_read_raw(uint8_t cmd, uint16_t *raw_data)
{
    esp_err_t ret;

    ret = i2c_master_write_to_device(
        I2C_MASTER_CHANNEL,
        HTU21D_ADDR,
        &cmd,
        1,
        pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS)
    );
    if (ret != ESP_OK)
        return ret;

    vTaskDelay(pdMS_TO_TICKS(50));   // chờ sensor xử lý

    uint8_t data[3];
    ret = i2c_master_read_from_device(
        I2C_MASTER_CHANNEL,
        HTU21D_ADDR,
        data,
        3,
        pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS)
    );
    if (ret != ESP_OK)
        return ret;

    *raw_data = (data[0] << 8) | data[1];
    *raw_data &= 0xFFFC;   // bỏ 2 bit trạng thái cuối
    return ESP_OK;
}

// Public Functions

// Khởi tạo I2C Channel 0   
esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA,
        .scl_io_num = I2C_MASTER_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_CHANNEL, &conf));
    return i2c_driver_install(I2C_MASTER_CHANNEL, conf.mode, 0, 0, 0);
}

// Đọc nhiệt độ 
float htu21d_get_temperature(void)
{
    uint16_t raw;
    if (htu21d_read_raw(HTU21D_TRIG_TEMP_MEASURE, &raw) != ESP_OK)
    {
        ESP_LOGE(TAG, "Temperature read failed");
        return -999.0f;
    }
    return -46.85f + (175.72f * (float)raw / 65536.0f);
}

// Đọc độ ẩm 
float htu21d_get_humidity(void)
{
    uint16_t raw;
    if (htu21d_read_raw(HTU21D_TRIG_HUMI_MEASURE, &raw) != ESP_OK)
    {
        ESP_LOGE(TAG, "Humidity read failed");
        return -999.0f;
    }
    return -6.0f + (125.0f * (float)raw / 65536.0f);
}