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

    ble_manager_init("IMIC_Robot");

    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
}
