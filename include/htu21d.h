#ifndef HTU21D_H
#define HTU21D_H

#include <stdint.h>
#include "i2c_bus.h"

#define HTU21D_ADDR                 0x40
#define HTU21D_TRIG_TEMP_MEASURE    0xF3
#define HTU21D_TRIG_HUMI_MEASURE    0xF5

typedef struct {
    i2c_dev_t base;
    float temp;
    float humidity;
    uint16_t raw_data;
} htu21d_t;

// Thêm thiết bị vào bus
esp_err_t htu21d_init(i2c_master_bus_handle_t bus_handle, htu21d_t *sensor);
float htu21d_get_temperature(htu21d_t *sensor);
float htu21d_get_humidity(htu21d_t *sensor);

#endif
