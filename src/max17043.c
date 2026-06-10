#include "max17043.h"
#include "i2c_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


esp_err_t max17043_init(i2c_master_bus_handle_t bus_handle, max17043_t *sensor){
    sensor->soc = 0.0f;
    sensor->voltage = 0.0f;
    sensor->base.address = MAX17043_ADDR;

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = sensor->base.address,
        .scl_speed_hz = I2C_MASTER_FREQ,
    };
    return i2c_master_bus_add_device(bus_handle, &dev_config, &sensor->base.dev_handle);
}


float max17043_get_battery_voltage(max17043_t *sensor){
    uint8_t reg = REG_VCELL;
    uint8_t data[2] = {0, 0};

    esp_err_t err = i2c_master_transmit_receive(sensor -> base.dev_handle, &reg, 1, data, 2, pdMS_TO_TICKS(100));

    if(err == ESP_OK){
        uint16_t vcell = (data[0] << 8) | data[1];
        float voltage = (vcell >> 4) * 1.25f / 1000.0f;
        return voltage;
    }
    else{
        return -999.0f;
    }
}


float max17043_get_soc(max17043_t *sensor){
    uint8_t reg = REG_SOC;
    uint8_t data[2] = {0, 0};

    esp_err_t err = i2c_master_transmit_receive(sensor -> base.dev_handle, &reg, 1, data, 2, pdMS_TO_TICKS(100));

    if(err == ESP_OK){
        float soc = data[0] + (data[1] / 256.0f);
        return soc;
    }
    else{
        return -999.0f;
    }
}