#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

/* Khởi động task TCP client: kết nối tới TCP_SERVER_IP:TCP_SERVER_PORT,
 * tự reconnect khi mất kết nối. Mỗi byte nhận được xử lý như một
 * motor_cmd_t (giống lệnh BLE) và được chuyển cho driver_motor_handle_cmd.
 * Gọi sau khi WiFi đã kết nối (có IP). */
esp_err_t tcp_client_start(void);

/* Gửi dữ liệu lên server qua socket hiện tại.
 * Trả về số byte đã gửi, hoặc -1 nếu chưa kết nối/lỗi. */
int tcp_client_send(const void *data, size_t len);

bool tcp_client_is_connected(void);

#endif
