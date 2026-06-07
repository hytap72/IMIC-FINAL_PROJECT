#include "max17043.h"

static i2c_master_dev_handle_t i2c_handle;

static const char *TAG = "MAX17043";

void max17043_init(i2c_master_bus_handle_t bus){
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MAX17043_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_config, &i2c_handle));
    ESP_LOGI(TAG, "MAX17043 initialized");
}


float read_battery_voltage(void){
    uint8_t reg = REG_VCELL;
    uint8_t data[2] = {0, 0};

    esp_err_t err = i2c_master_transmit_receive(i2c_handle, &reg, 1, data, 2, -1);

    if(err == ESP_OK){
        uint16_t vcell = (data[0] << 8) | data[1];
        float voltage = (vcell >> 4) * 1.25f / 1000.0f;
        return voltage;
    }
    else{
        ESP_LOGE(TAG, "ERROR: %s", esp_err_to_name(err));
        return -1.0;
    }
}


float read_soc(void){
    uint8_t reg = REG_SOC;
    uint8_t data[2] = {0, 0};

    esp_err_t err = i2c_master_transmit_receive(i2c_handle, &reg, 1, data, 2, -1);

    if(err == ESP_OK){
        float soc = data[0] + (data[1] / 256.0f);
        return soc;
    }
    else{
        ESP_LOGE(TAG, "ERROR: %s", esp_err_to_name(err));
        return -1.0;
    }
}