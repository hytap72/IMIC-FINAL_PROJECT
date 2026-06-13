#ifndef I2C_BUS_H
#define I2C_BUS_H

#include "driver/i2c_master.h"
#define I2C_MASTER_FREQ 100000
#define I2C_MASTER_SCL 22
#define I2C_MASTER_SDA 21
#define I2C_MASTER_PORT 0

typedef struct 
{
    i2c_master_dev_handle_t dev_handle;
    uint8_t address;
} i2c_dev_t;

//Truyền 1byte I2C
esp_err_t i2c_byte_send_to_sensor(i2c_master_dev_handle_t dev_handle, uint8_t data);

//Nhận data từ I2C
esp_err_t i2c_byte_receive_from_sensor(i2c_master_dev_handle_t dev_handle, uint8_t* buff);

// Khởi tạo Bus I2C, trả về handle của bus
esp_err_t i2c_bus_init(i2c_master_bus_handle_t *ret_bus_handle);


#endif