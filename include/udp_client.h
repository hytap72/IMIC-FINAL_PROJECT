#ifndef UDP_CLIENT_H
#define UDP_CLIENT_H

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

/* Khởi tạo UDP socket tới UDP_SERVER_IP:UDP_SERVER_PORT.
 * Đồng thời lắng nghe lệnh đến trên cùng socket (mỗi byte nhận được
 * xử lý như một motor_cmd_t, giống lệnh BLE/TCP).
 * Gọi sau khi WiFi đã kết nối (có IP). */
esp_err_t udp_client_start(void);

/* Gửi dữ liệu (telemetry) tới UDP_SERVER_IP:UDP_SERVER_PORT.
 * Trả về số byte đã gửi, hoặc -1 nếu lỗi. */
int udp_client_send(const void *data, size_t len);

bool udp_client_is_ready(void);

#endif
