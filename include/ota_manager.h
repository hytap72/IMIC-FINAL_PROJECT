#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"

/* Bắt đầu cập nhật firmware OTA từ URL (HTTP/HTTPS) chứa file .bin.
 * Chạy bất đồng bộ trong task riêng; nếu thành công thiết bị sẽ tự
 * esp_restart() để chạy firmware mới. Trả về ESP_ERR_INVALID_STATE
 * nếu đang có một lần OTA khác chạy. */
esp_err_t ota_manager_start(const char *url);

/* true nếu đang trong quá trình tải/ghi firmware */
bool ota_manager_is_in_progress(void);

/* Phiên bản firmware hiện tại (PROJECT_VER, từ app description) */
const char *ota_manager_get_version(void);

/* Lưu lại địa chỉ IP hiện tại của thiết bị (dạng "a.b.c.d") để hiển thị
 * trên dashboard và dùng làm đích upload firmware */
void ota_manager_set_device_ip(const char *ip);

/* Địa chỉ IP hiện tại của thiết bị, hoặc "0.0.0.0" nếu chưa kết nối */
const char *ota_manager_get_device_ip(void);

/* Callback gọi mỗi khi trạng thái OTA (ota_in_progress) thay đổi, để publish
 * ngay lên MQTT trước khi mqtt_manager_stop() làm mất kết nối trong lúc tải
 * firmware -> dashboard thấy được "Đang cập nhật..." */
typedef void (*ota_status_cb_t)(void);
void ota_manager_set_status_cb(ota_status_cb_t cb);

#endif
