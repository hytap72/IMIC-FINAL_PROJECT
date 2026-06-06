#ifndef MAX17043_H
#define MAX17043_H

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"

#define I2C_SCL_PIN 22
#define I2C_SDA_PIN 21
#define I2C_MASTER_FREQ 100000
#define MAX17043_ADDR 0x36
#define REG_VCELL 0x02
#define REG_SOC 0x04

void i2c_init(void);

float read_battery_voltage(void);

float read_soc(void);

#endif
