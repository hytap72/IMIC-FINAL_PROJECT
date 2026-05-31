#include "max17043.h"

static const char *TAG = "APP_MAIN";

void app_main(void){
    i2c_init();
    vTaskDelay(pdMS_TO_TICKS(500));
    while(1){
        float voltage = read_battery_voltage();
        float soc = read_soc();
        if (voltage >= 0.0 && soc >= 0.0) {
            ESP_LOGI(TAG, "Voltage: %.2f V | Capacity: %.2f %%", voltage, soc);
            if (soc <= 20.0) {
                ESP_LOGW(TAG, "Warning: Low battery level!");
            }
        } 
        else{
            ESP_LOGE(TAG, "Communication error: Cannot read data from MAX17043!");
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}