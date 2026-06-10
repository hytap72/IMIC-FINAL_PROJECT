#ifndef AWS_IOT_H
#define AWS_IOT_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* Kết nối tới AWS IoT Core (MQTT qua mTLS) và subscribe topic lệnh.
 * Gọi sau khi WiFi đã kết nối (có IP). */
esp_err_t aws_iot_start(void);

/* Publish telemetry (sensor + battery + trạng thái motor) dạng JSON
 * lên topic AWS_IOT_TOPIC_DATA */
esp_err_t aws_iot_publish_telemetry(float temperature, float humidity,
                                     float battery_voltage, float battery_soc,
                                     uint8_t motor_state);

bool aws_iot_is_connected(void);

#endif
