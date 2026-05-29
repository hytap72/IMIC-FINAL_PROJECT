#ifndef HTU21D_H
#define HTU21D_H

#include "esp_err.h"

// Khởi tạo I2C master
esp_err_t i2c_master_init(void);

// Đọc nhiệt độ (trả về độ C)
float htu21d_get_temperature(void);

// Đọc độ ẩm (trả về % RH)
float htu21d_get_humidity(void);

#endif