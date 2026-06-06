#ifndef DRIVER_MOTOR_H
#define DRIVER_MOTOR_H

#include "esp_err.h"

/* Các lệnh điều khiển nhận từ server */
typedef enum {
    MOTOR_CMD_FORWARD  = 0x01,
    MOTOR_CMD_BACKWARD = 0x02,
    MOTOR_CMD_LEFT     = 0x03,
    MOTOR_CMD_RIGHT    = 0x04,
    MOTOR_CMD_RUN      = 0x05,
    MOTOR_CMD_STOP     = 0x06,
} motor_cmd_t;

/* Khởi tạo GPIO điều khiển motor */
esp_err_t driver_motor_init(void);

/* Xử lý lệnh nhận từ server */
void driver_motor_handle_cmd(motor_cmd_t cmd);

#endif
