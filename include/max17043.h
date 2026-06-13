#ifndef MAX17043_H
#define MAX17043_H

#include <stdint.h>
#include "i2c_bus.h"


#define MAX17043_ADDR 0x36
#define REG_VCELL 0x02
#define REG_SOC 0x04

typedef struct
{
    i2c_dev_t base;
    float voltage;
    float soc;
} max17043_t;


esp_err_t max17043_init(i2c_master_bus_handle_t bus_handle, max17043_t *sensor);

float max17043_get_battery_voltage(max17043_t *sensor);

float max17043_get_soc(max17043_t *sensor);

#endif
