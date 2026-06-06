#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "htu21d.h"
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
    /* NVS và event loop cần cho WiFi + BLE */
    nvs_flash_init();
    esp_event_loop_create_default();

    /* Khởi tạo I2C và các driver */
    i2c_master_init();
    driver_motor_init();

    /* Khởi tạo BLE — advertise tên "IMIC_Robot" */
    ble_manager_init("IMIC_Robot");

    /* Task đọc cảm biến định kỳ */
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
}
