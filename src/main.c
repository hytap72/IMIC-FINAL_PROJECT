#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "htu21d.h"

static const char *TAG = "MAIN";

// Task đọc cảm biến
static void sensor_reading_task(void *pvParameters)
{
    while (1)
    {
        float temp = htu21d_get_temperature();
        float hum = htu21d_get_humidity();
        ESP_LOGI(TAG, "Temp: %.2f C, Hum: %.2f %%", temp, hum);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    i2c_master_init();

    xTaskCreate(sensor_reading_task, "sensor_task", 4096, NULL, 5, NULL);

}