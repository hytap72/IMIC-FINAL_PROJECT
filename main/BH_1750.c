#include "BH_1750.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
static const char *TAG = "BH1750";
#define I2C_TIMEOUT_MS 200

/* Gửi 1 byte lệnh xuống sensor */
static esp_err_t bh1750_send_cmd(bh1750_handle_t *sensor, uint8_t cmd)
{
    return i2c_master_transmit(sensor->i2c_handle, &cmd, 1,
                               I2C_TIMEOUT_MS / portTICK_PERIOD_MS);
}

esp_err_t bh1750_init(i2c_master_bus_handle_t bus_handle,
                      uint8_t dev_addr,
                      bh1750_handle_t *out_handle)
{
    /* Thêm device vào bus */
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev_addr,
        .scl_speed_hz = 400000,
    };
    ESP_RETURN_ON_ERROR(
        i2c_master_bus_add_device(bus_handle, &dev_cfg, &out_handle->i2c_handle),
        TAG, "Failed to add BH1750 to I2C bus");

    /* Power ON */
    ESP_RETURN_ON_ERROR(
        bh1750_send_cmd(out_handle, BH1750_CMD_POWER_ON),
        TAG, "Power ON failed");
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Reset data register */
    ESP_RETURN_ON_ERROR(
        bh1750_send_cmd(out_handle, BH1750_CMD_RESET),
        TAG, "Reset failed");

    /* Mặc định: Continuous High Resolution */
    ESP_RETURN_ON_ERROR(
        bh1750_set_mode(out_handle, BH1750_MODE_CONT_H),
        TAG, "Set mode failed");

    ESP_LOGI(TAG, "BH1750 initialized at addr=0x%02X", dev_addr);
    return ESP_OK;
}

esp_err_t bh1750_set_mode(bh1750_handle_t *sensor, bh1750_mode_t mode)
{
    ESP_RETURN_ON_ERROR(
        bh1750_send_cmd(sensor, (uint8_t)mode),
        TAG, "Set mode failed");
    sensor->mode = mode;

    /* Chờ measurement time theo datasheet */
    uint32_t wait_ms;

    if (mode == BH1750_MODE_CONT_L || mode == BH1750_MODE_ONE_L)
    {
        wait_ms = 24;
    }
    else
    {
        wait_ms = 180;
    }
    vTaskDelay(pdMS_TO_TICKS(wait_ms));
    return ESP_OK;
}

esp_err_t bh1750_read_lux(bh1750_handle_t *sensor, float *lux)
{
    uint8_t raw[2] = {0};

    ESP_RETURN_ON_ERROR(
        i2c_master_receive(sensor->i2c_handle, raw, sizeof(raw),
                           I2C_TIMEOUT_MS / portTICK_PERIOD_MS),
        TAG, "Read failed");

    /* Công thức: lux = raw_value / 1.2
     * Nếu dùng H2 mode (0.5 lx resolution): lux = raw_value / 2.4 */
    uint16_t raw_val = ((uint16_t)raw[0] << 8) | raw[1];
    float divisor;
    if(sensor->mode == BH1750_MODE_CONT_H2 || sensor->mode == BH1750_MODE_ONE_H2){
        divisor = 2.4f;
    }
    else{
        divisor = 1.2f;
    }
    *lux = (float)raw_val / divisor;

    return ESP_OK;
}
