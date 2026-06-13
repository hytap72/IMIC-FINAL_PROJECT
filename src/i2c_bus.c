#include "i2c_bus.h"
#include "freertos/FreeRTOS.h"


esp_err_t i2c_bus_init(i2c_master_bus_handle_t *ret_bus_handle) {
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_PORT,
        .sda_io_num = I2C_MASTER_SDA,
        .scl_io_num = I2C_MASTER_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    return i2c_new_master_bus(&bus_config, ret_bus_handle);
}


esp_err_t i2c_byte_send_to_sensor(i2c_master_dev_handle_t dev_handle, uint8_t data){
    return i2c_master_transmit(dev_handle, &data, 1, pdMS_TO_TICKS(100));
}

esp_err_t i2c_byte_receive_from_sensor(i2c_master_dev_handle_t dev_handle, uint8_t* buff){
    return i2c_master_receive(dev_handle, buff, sizeof(buff), pdMS_TO_TICKS(100));
}

