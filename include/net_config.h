#ifndef NET_CONFIG_H
#define NET_CONFIG_H

/* ============================================================
 * Cấu hình chung cho các module mạng (MQTT/AWS IoT, TCP, UDP)
 * Chỉnh các giá trị bên dưới cho phù hợp với hệ thống của bạn.
 * ============================================================ */

/* ─── AWS IoT Core (MQTT qua mTLS) ──────────────────────────
 * Lấy AWS_IOT_ENDPOINT tại AWS IoT Console → Settings → Device data endpoint
 * Cert/key thật điền trong include/aws_certs.h (file này không commit lên git)
 */
#define AWS_IOT_ENDPOINT      "your-ats-endpoint.iot.ap-southeast-2.amazonaws.com"
#define AWS_IOT_PORT          8883
#define AWS_IOT_THING_NAME    "imic-final-esp32"

#define AWS_IOT_TOPIC_DATA    AWS_IOT_THING_NAME "/data"
#define AWS_IOT_TOPIC_CMD     AWS_IOT_THING_NAME "/cmd"

/* ─── TCP client (server điều khiển trong LAN) ──────────────── */
#define TCP_SERVER_IP    "192.168.1.100"
#define TCP_SERVER_PORT  5000

/* ─── UDP client (telemetry trong LAN) ──────────────────────── */
#define UDP_SERVER_IP    "192.168.1.100"
#define UDP_SERVER_PORT  5001

/* ─── Chu kỳ gửi telemetry (ms) ──────────────────────────────── */
#define TELEMETRY_INTERVAL_MS  5000

#endif
