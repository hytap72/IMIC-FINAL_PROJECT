#include "max17043.h"

static i2c_master_bus_handle_t bus_handle;
static i2c_master_dev_handle_t i2c_handle;

static const char *TAG = "MAX17043";

void i2c_init(void){
    i2c_master_bus_config_t i2c_master_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = -1,
        .scl_io_num = I2C_SCL_PIN,
        .sda_io_num = I2C_SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_master_config, &bus_handle));
    
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MAX17043_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &i2c_handle));
    ESP_LOGI(TAG, "MAX17043 I2C initialized successfully!");
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