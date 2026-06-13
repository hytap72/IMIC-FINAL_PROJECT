#include "mpu6050.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"

static const char *TAG = "MPU6050";

esp_err_t mpu6050_init(i2c_master_bus_handle_t bus_handle, mpu6050_t *sensor)
{
    sensor->base.address = MPU6050_ADDR;
    sensor->accel_x = sensor->accel_y = sensor->accel_z = 0.0f;
    sensor->gyro_x  = sensor->gyro_y  = sensor->gyro_z  = 0.0f;
    sensor->temp = 0.0f;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = sensor->base.address,
        .scl_speed_hz    = I2C_MASTER_FREQ,
    };
    ESP_RETURN_ON_ERROR(
        i2c_master_bus_add_device(bus_handle, &dev_cfg, &sensor->base.dev_handle),
        TAG, "Failed to add MPU6050 to I2C bus");

    /* Đánh thức MPU6050 (mặc định sau reset ở chế độ sleep) */
    uint8_t cmd[2] = { MPU6050_REG_PWR_MGMT_1, 0x00 };
    ESP_RETURN_ON_ERROR(
        i2c_master_transmit(sensor->base.dev_handle, cmd, sizeof(cmd), pdMS_TO_TICKS(100)),
        TAG, "Wake up failed");

    ESP_LOGI(TAG, "MPU6050 initialized at addr=0x%02X", sensor->base.address);
    return ESP_OK;
}

esp_err_t mpu6050_read(mpu6050_t *sensor)
{
    uint8_t reg = MPU6050_REG_ACCEL_XOUT;
    uint8_t data[14] = {0};

    ESP_RETURN_ON_ERROR(
        i2c_master_transmit_receive(sensor->base.dev_handle, &reg, 1, data, sizeof(data), pdMS_TO_TICKS(100)),
        TAG, "Read failed");

    int16_t raw_accel_x = (data[0]  << 8) | data[1];
    int16_t raw_accel_y = (data[2]  << 8) | data[3];
    int16_t raw_accel_z = (data[4]  << 8) | data[5];
    int16_t raw_temp    = (data[6]  << 8) | data[7];
    int16_t raw_gyro_x  = (data[8]  << 8) | data[9];
    int16_t raw_gyro_y  = (data[10] << 8) | data[11];
    int16_t raw_gyro_z  = (data[12] << 8) | data[13];

    sensor->accel_x = (float)raw_accel_x / MPU6050_ACCEL_LSB_PER_G;
    sensor->accel_y = (float)raw_accel_y / MPU6050_ACCEL_LSB_PER_G;
    sensor->accel_z = (float)raw_accel_z / MPU6050_ACCEL_LSB_PER_G;

    sensor->gyro_x = (float)raw_gyro_x / MPU6050_GYRO_LSB_PER_DPS;
    sensor->gyro_y = (float)raw_gyro_y / MPU6050_GYRO_LSB_PER_DPS;
    sensor->gyro_z = (float)raw_gyro_z / MPU6050_GYRO_LSB_PER_DPS;

    sensor->temp = (float)raw_temp / 340.0f + 36.53f;

    return ESP_OK;
}
