#include "htu21d.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"

#define I2C_MASTER_SCL          22
#define I2C_MASTER_SDA          21
#define I2C_MASTER_FREQ_HZ      100000
#define I2C_MASTER_TIMEOUT_MS   1000

#define HTU21D_ADDR             0x40
#define HTU21D_TRIG_TEMP        0xF3
#define HTU21D_TRIG_HUMI        0xF5

static const char *TAG = "HTU21D";
static i2c_master_dev_handle_t s_dev = NULL;

esp_err_t i2c_master_init(i2c_master_bus_handle_t *out_bus)
{
    i2c_master_bus_config_t bus_cfg = {
        .clk_source             = I2C_CLK_SRC_DEFAULT,
        .i2c_port               = I2C_NUM_0,
        .scl_io_num             = I2C_MASTER_SCL,
        .sda_io_num             = I2C_MASTER_SDA,
        .glitch_ignore_cnt      = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, out_bus));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = HTU21D_ADDR,
        .scl_speed_hz    = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(*out_bus, &dev_cfg, &s_dev));

    ESP_LOGI(TAG, "I2C initialized, HTU21D added");
    return ESP_OK;
}

static esp_err_t htu21d_read_raw(uint8_t cmd, uint16_t *raw_data)
{
    uint8_t data[3] = {0};

    ESP_RETURN_ON_ERROR(
        i2c_master_transmit(s_dev, &cmd, 1, I2C_MASTER_TIMEOUT_MS),
        TAG, "transmit failed");

    vTaskDelay(pdMS_TO_TICKS(50));

    ESP_RETURN_ON_ERROR(
        i2c_master_receive(s_dev, data, 3, I2C_MASTER_TIMEOUT_MS),
        TAG, "receive failed");

    *raw_data = ((data[0] << 8) | data[1]) & 0xFFFC;
    return ESP_OK;
}

float htu21d_get_temperature(void)
{
    uint16_t raw;
    if (htu21d_read_raw(HTU21D_TRIG_TEMP, &raw) != ESP_OK) {
        ESP_LOGE(TAG, "Temperature read failed");
        return -999.0f;
    }
    return -46.85f + (175.72f * (float)raw / 65536.0f);
}

float htu21d_get_humidity(void)
{
    uint16_t raw;
    if (htu21d_read_raw(HTU21D_TRIG_HUMI, &raw) != ESP_OK) {
        ESP_LOGE(TAG, "Humidity read failed");
        return -999.0f;
    }
    return -6.0f + (125.0f * (float)raw / 65536.0f);
}
