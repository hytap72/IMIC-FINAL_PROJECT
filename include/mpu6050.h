#ifndef MPU6050_H
#define MPU6050_H

#include "i2c_bus.h"

/* GY-521 (MPU6050), AD0 nối GND -> địa chỉ 0x68, AD0 nối VCC -> 0x69 */
#define MPU6050_ADDR        0x68

#define MPU6050_REG_PWR_MGMT_1  0x6B
#define MPU6050_REG_ACCEL_XOUT  0x3B

/* LSB/đơn vị ở dải đo mặc định: accel ±2g, gyro ±250 °/s */
#define MPU6050_ACCEL_LSB_PER_G     16384.0f
#define MPU6050_GYRO_LSB_PER_DPS    131.0f

typedef struct {
    i2c_dev_t base;
    float accel_x, accel_y, accel_z; /* g */
    float gyro_x, gyro_y, gyro_z;    /* độ/giây */
    float temp;                       /* độ C */
} mpu6050_t;

esp_err_t mpu6050_init(i2c_master_bus_handle_t bus_handle, mpu6050_t *sensor);

/* Đọc accel + gyro + nhiệt độ, cập nhật các trường trong sensor */
esp_err_t mpu6050_read(mpu6050_t *sensor);

#endif
