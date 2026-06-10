#include "htu21d.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"



esp_err_t htu21d_init(i2c_master_bus_handle_t bus_handle, htu21d_t *sensor) {
    
    sensor->temp = 0.0f;
    sensor->humidity = 0.0f;
    sensor->raw_data = 0;
    sensor->base.address = HTU21D_ADDR;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_7,
        .device_address = sensor->base.address,
        .scl_speed_hz = I2C_MASTER_FREQ,
    };
    return i2c_master_bus_add_device(bus_handle, &dev_cfg, &sensor->base.dev_handle);
}

static esp_err_t htu21d_get_raw_data(htu21d_t *sensor, uint8_t *cmd){
    uint8_t data[3];
    esp_err_t ret;
    ret = i2c_byte_send_to_sensor(sensor->base.dev_handle, *cmd);
    if(ret != ESP_OK) {
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(50));

    ret = i2c_byte_receive_from_sensor(sensor -> base.dev_handle, data);
    if (ret != ESP_OK) {
        return ret;
    }  
    sensor->raw_data = (data[0] << 8) | data[1];
    sensor->raw_data &= 0xFFFC; // Loại bỏ 2 bit cuối - bit trạng thái
    return ESP_OK;
}

float htu21d_get_temperature(htu21d_t *sensor) {

    uint8_t cmd = HTU21D_TRIG_TEMP_MEASURE;
    if(htu21d_get_raw_data(sensor, &cmd) != ESP_OK){
        return -999.0f;
    }
    return sensor->temp= -46.85f + (175.72f * (float)sensor->raw_data / 65536.0f);
}


// Đọc độ ẩm 
float htu21d_get_humidity(htu21d_t *sensor)
{
    uint8_t cmd = HTU21D_TRIG_HUMI_MEASURE;
    if(htu21d_get_raw_data(sensor, &cmd) != ESP_OK){
        return -999.0f;
    }
    return sensor->humidity= -6.0f + (125.0f * (float)sensor->raw_data / 65536.0f);
}

