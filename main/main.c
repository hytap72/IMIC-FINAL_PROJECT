#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "BH_1750.h"

static const char *TAG = "MAIN";

#define I2C_MASTER_SDA   GPIO_NUM_21
#define I2C_MASTER_SCL   GPIO_NUM_22
#define I2C_MASTER_FREQ  400000

void app_main(void)
{
    /* 1. Khởi tạo I2C bus */
    i2c_master_bus_handle_t bus_handle;
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port              = I2C_NUM_0,
        .sda_io_num            = I2C_MASTER_SDA,
        .scl_io_num            = I2C_MASTER_SCL,
        .clk_source            = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt     = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus_handle));

    /* 2. Khởi tạo BH1750 */
    bh1750_handle_t bh1750;
    ESP_ERROR_CHECK(bh1750_init(bus_handle, BH1750_I2C_ADDR_LOW, &bh1750));

    /* 3. Đọc liên tục */
    while (1) {
        float lux = 0;
        esp_err_t err = bh1750_read_lux(&bh1750, &lux);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Light: %.2f lx", lux);
        } else {
            ESP_LOGE(TAG, "Read error: %s", esp_err_to_name(err));
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}