#include "driver/i2c_master.h"
#include "esp_err.h"

/* I2C Address */
#define BH1750_I2C_ADDR_LOW     0x23    // ADDR pin = GND
#define BH1750_I2C_ADDR_HIGH    0x5C    // ADDR pin = VCC

/* Opcodes */
#define BH1750_CMD_POWER_DOWN   0x00
#define BH1750_CMD_POWER_ON     0x01
#define BH1750_CMD_RESET        0x07

/* Measurement modes */
typedef enum {
    BH1750_MODE_CONT_H      = 0x10, // Continuous, 1 lx resolution, ~120ms
    BH1750_MODE_CONT_H2     = 0x11, // Continuous, 0.5 lx resolution, ~120ms
    BH1750_MODE_CONT_L      = 0x13, // Continuous, 4 lx resolution, ~16ms
    BH1750_MODE_ONE_H       = 0x20, // One-time, 1 lx resolution
    BH1750_MODE_ONE_H2      = 0x21, // One-time, 0.5 lx resolution
    BH1750_MODE_ONE_L       = 0x23, // One-time, 4 lx resolution
} bh1750_mode_t;

typedef struct {
    i2c_master_dev_handle_t i2c_handle;
    bh1750_mode_t           mode;
} bh1750_handle_t;

esp_err_t bh1750_init(i2c_master_bus_handle_t bus_handle,
                      uint8_t dev_addr,
                      bh1750_handle_t *out_handle);

esp_err_t bh1750_set_mode(bh1750_handle_t *sensor, bh1750_mode_t mode);

esp_err_t bh1750_read_lux(bh1750_handle_t *sensor, float *lux);