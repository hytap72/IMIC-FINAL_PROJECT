#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include "esp_err.h"
#include <stdint.h>

/*
 * GATT Services
 *
 * WiFi Provision Service (UUID: 0xAA01)
 *   - SSID Characteristic      (Write)           UUID: 0xAA11
 *   - Password Characteristic  (Write)           UUID: 0xAA12
 *   - Status Characteristic    (Read | Notify)   UUID: 0xAA13
 *
 * Motor Control Service (UUID: 0xAA02)
 *   - Command Characteristic   (Write)           UUID: 0xAA21
 */

/* Trạng thái WiFi notify về app */
typedef enum {
    WIFI_STATUS_IDLE        = 0x00,
    WIFI_STATUS_CONNECTING  = 0x01,
    WIFI_STATUS_CONNECTED   = 0x02,
    WIFI_STATUS_FAILED      = 0x03,
} ble_wifi_status_t;

/* Callback gọi khi WiFi STA kết nối thành công và nhận được IP */
typedef void (*ble_wifi_connected_cb_t)(void);

/* Khởi tạo BLE stack và bắt đầu advertise */
esp_err_t ble_manager_init(const char *device_name);

/* Đăng ký callback gọi khi WiFi kết nối thành công (có IP) */
void ble_manager_set_wifi_connected_cb(ble_wifi_connected_cb_t cb);

/* Gửi notify trạng thái WiFi về app đang kết nối */
void ble_manager_notify_wifi_status(ble_wifi_status_t status);

/* Dừng BLE (gọi sau khi WiFi đã kết nối thành công nếu muốn tiết kiệm RAM) */
void ble_manager_deinit(void);

#endif
