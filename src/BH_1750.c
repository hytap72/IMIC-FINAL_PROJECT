#include "bh_1750.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"

static const char *TAG = "BH1750";


esp_err_t bh1750_init(i2c_master_bus_handle_t bus_handle, bh1750_t *sensor)
{
    sensor->base.address = BH1750_I2C_ADDR_LOW;
    sensor->lux_value = 0.0f;
    sensor->mode = BH1750_MODE_CONT_H;
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = sensor->base.address,
        .scl_speed_hz = I2C_MASTER_FREQ,
    };
    ESP_RETURN_ON_ERROR(
        i2c_master_bus_add_device(bus_handle, &dev_cfg, &sensor->base.dev_handle),
        TAG, "Failed to add BH1750 to I2C bus");

    /* Power ON */
    ESP_RETURN_ON_ERROR(
        i2c_byte_send_to_sensor(sensor->base.dev_handle, BH1750_CMD_POWER_ON),
        TAG, "Power ON failed");
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Reset data register */
    ESP_RETURN_ON_ERROR(
        i2c_byte_send_to_sensor(sensor->base.dev_handle, BH1750_CMD_RESET),
        TAG, "Reset failed");

    /* Mặc định: Continuous High Resolution */
    ESP_RETURN_ON_ERROR(
        bh1750_set_mode(sensor, sensor->mode),
        TAG, "Set mode failed");

    ESP_LOGI(TAG, "BH1750 initialized at addr=0x%02X", sensor->base.address);
    return ESP_OK;
}

esp_err_t bh1750_set_mode(bh1750_t *sensor,bh1750_mode_t mode)
{
    ESP_RETURN_ON_ERROR(
        i2c_byte_send_to_sensor(sensor->base.dev_handle, (uint8_t)mode),
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

float bh1750_read_lux(bh1750_t *sensor)
{
    uint8_t raw[2] = {0};

    if(i2c_master_receive(sensor->base.dev_handle, raw, sizeof(raw), pdMS_TO_TICKS(100)) != ESP_OK){
        return -999.0f;
    }

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
    sensor->lux_value = (float)raw_val / divisor;

    return sensor->lux_value;
}
