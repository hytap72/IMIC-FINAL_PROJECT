#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "i2c_bus.h"
#include "htu21d.h"
#include "max17043.h"
#include "bh_1750.h"

#define HTU21D_SAMPLE_TIME 2000
#define MAX17043_SAMPLE_TIME 2000
#define BH1750_SAMPLE_TIME 2000

typedef enum {
    TAG_MAIN = 0,
    TAG_HTU21D,
    TAG_MAX17043,
    TAG_BH1750
} tag_index_t;

static const char *TAG[] = {
    [TAG_MAIN] = "MAIN", 
    [TAG_HTU21D] = "HTU21D",
    [TAG_MAX17043] = "MAX17043",
    [TAG_BH1750] = "BH1750"
};



//Tạo mutex toàn cục
static SemaphoreHandle_t i2c_mutex = NULL;





// // Task đọc cảm biến htu21d - 2s
// static void htu21d_reading_task(void *pvParameters)
// {
//     htu21d_t *sensor = (htu21d_t *)pvParameters;
//     while (1)
//     {
//         if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE){
//             htu21d_get_temperature(sensor);
//             htu21d_get_humidity(sensor);
//             xSemaphoreGive(i2c_mutex);
//         }
        
//         ESP_LOGE(TAG[TAG_HTU21D], "Temp: %.2f C, Hum: %.2f %%", sensor->temp, sensor->humidity);
//         vTaskDelay(pdMS_TO_TICKS(HTU21D_SAMPLE_TIME));
//     }
// }

// //Task đọc cảm biến max17043 - 10s
// static void max17043_reading_task(void *pvParameters){
//     max17043_t *sensor = (max17043_t*)pvParameters;
    
//     while (1)
//     {
//         if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE){
//             max17043_get_battery_voltage(sensor);
//             max17043_get_soc(sensor);

//             xSemaphoreGive(i2c_mutex);
//         }

//         ESP_LOGE(TAG[TAG_MAX17043], "BAT_Vol: %.2f V, BAT_SoC: %.2f %%", sensor->voltage, sensor->soc);
//         vTaskDelay(pdMS_TO_TICKS(MAX17043_SAMPLE_TIME));
//     }
    
// }

// //Task đọc cảm biến bh1750 - 10s
// static void bh1750_reading_task(void *pvParameters){
//     bh1750_t *sensor = (bh1750_t*)pvParameters;

//     while (1)
//     {
//         if(xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE){

//             bh1750_read_lux(sensor);
//             xSemaphoreGive(i2c_mutex);
//         }
        
//         ESP_LOGE(TAG[TAG_BH1750], "Lux_value: %.2f", sensor->lux_value);
//         vTaskDelay(pdMS_TO_TICKS(BH1750_SAMPLE_TIME));
//     }
    
// }




//---------------------------Hàm gọi giả lập task đọc cảm biển----------------


//Task đọc cảm biến htu21d

static void test_htu21d_reading_task(void *pvParameters){
    float *data = (float*)pvParameters;

    while(1){
        if(xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE){
            *data = 30.0f;
            xSemaphoreGive(i2c_mutex);
        }
        ESP_LOGE(TAG[TAG_HTU21D], "Temp: %.2f Humi: %.2f %", *data, *data);
        vTaskDelay(pdMS_TO_TICKS(HTU21D_SAMPLE_TIME));
    }
}

//Task đọc cảm biến max17043 
static void test_max17043_reading_task(void *pvParameters){
    float *data = (float*)pvParameters;
    
    while (1)
    {
        if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE){
            *data = 100.0f;

            xSemaphoreGive(i2c_mutex);
        }

        ESP_LOGE(TAG[TAG_MAX17043], "BAT_Vol: %.2f V, BAT_SoC: %.2f %%", *data, *data);
        vTaskDelay(pdMS_TO_TICKS(MAX17043_SAMPLE_TIME));
    }
    
}

//Task đọc cảm biến bh1750 - 20s
static void test_bh1750_reading_task(void *pvParameters){
    float* data = (float*)pvParameters;

    while (1)
    {
        if(xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE){

            *data = 101.0f;
            xSemaphoreGive(i2c_mutex);
        }
        
        ESP_LOGE(TAG[TAG_BH1750], "Lux_value: %.2f", *data);
        vTaskDelay(pdMS_TO_TICKS(BH1750_SAMPLE_TIME));
    }
    
}



htu21d_t m_htu21d;
max17043_t m_max17043;
bh1750_t m_bh17043;


void app_main(void)
{
    //Khởi tạo I2C
    i2c_master_bus_handle_t bus_handle;
    esp_err_t ret = i2c_bus_init(&bus_handle);
    if(ret != ESP_OK){
        ESP_LOGE(TAG[TAG_MAIN], "Initialize I2C fail: %s", esp_err_to_name(ret));
    }
    else{
        ESP_LOGE(TAG[TAG_MAIN], "Initialize I2C successfully");
    }

    /*Khởi tạo cảm biến*/
    htu21d_init(bus_handle, &m_htu21d);
    // max17043_init(bus_handle, &m_max17043);
    // bh1750_init(bus_handle, &m_bh17043);

    /*Khởi tạo mutex*/
    i2c_mutex = xSemaphoreCreateMutex();
    if (i2c_mutex == NULL) {
        ESP_LOGE(TAG[TAG_MAIN], "Failed to create I2C mutex");
        return;
    }


    /*Khởi tạo task FreeRTOS*/
    // xTaskCreate(htu21d_reading_task, "htu21d_task", 4096, &m_htu21d, 5, NULL);
    // xTaskCreate(max17043_reading_task, "max17043", 4096, &m_max17043, 5, NULL);
    // xTaskCreate(bh1750_reading_task, "bh1750", 4096, &m_bh17043, 5, NULL);



//-------------------Khởi tạo biến và task giả lập đọc cảm biến------------------
    float test_max;
    float test_bh;
    float test_htu;
    xTaskCreate(test_htu21d_reading_task, "htu21d", 4096, &test_htu, 5, NULL);
    xTaskCreate(test_max17043_reading_task, "max17043", 4096, &test_max, 5, NULL);
    xTaskCreate(test_bh1750_reading_task, "bh1750", 4096, &test_bh, 5, NULL);

}