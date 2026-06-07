#ifndef HTU21D_H
#define HTU21D_H

#include "esp_err.h"
#include "driver/i2c_master.h"

// Khởi tạo I2C bus (new driver), trả về bus_handle để các sensor khác dùng chung
esp_err_t i2c_master_init(i2c_master_bus_handle_t *out_bus);

float htu21d_get_temperature(void);
float htu21d_get_humidity(void);

#endif
