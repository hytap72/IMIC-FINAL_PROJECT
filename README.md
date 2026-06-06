# IMIC Final Project — Robot Thám Hiểm

> Đề tài: **Robot "thám hiểm"** — hệ thống nhúng trên ESP32 thu thập dữ liệu môi trường, điều khiển di chuyển từ xa, truyền dữ liệu qua MQTT/AWS IoT Core.

---

## Nhóm thực hiện

| Thành viên | Đảm nhiệm | Khối lượng chính |
|---|---|---|
| **Chính** | Di chuyển, Camera | Driver motor, BLE/WiFi nhận lệnh điều khiển |
| **Phát** | Nhiệt độ, Độ ẩm | HTU21D, Wrapper I2C (timeout + retry) |
| **Nghĩa** | Ánh sáng | BH1750, tổng hợp dữ liệu sensor |
| **Duy** | Giám sát pin | MAX17043, cơ chế error/retry tập trung |

---

## Định hướng dự án

**Input:**
- Hardware: cảm biến đo mức pin, góc, hướng (la bàn), gia tốc, nhiệt độ, độ ẩm, áp suất, ánh sáng, gió, vật cản, khoảng cách (siêu âm, IR, laser...), camera
- Software: user app / domain, server

**Output:**
- Motor, LED, buzzer, voice
- MQTT / AWS IoT Core, data stream

**App:**
- User/admin interface: control, monitor, log, security

---

## Kiến trúc tổng thể

```
[Sensors] → [ESP32] → [WiFi/MQTT] → [AWS IoT Core] → [Dashboard Grafana + Lambda]
                ↑
         [Remote Command]  ←  [Server / Mobile App]
```

---

## Pending

### Chung
- [ ] Định nghĩa giao thức lệnh giữa server và ESP32 (BLE / MQTT topic)
- [ ] Thiết kế kiến trúc task FreeRTOS (skeleton task) và phân chia priority
- [ ] Gắc naming convention + áp dụng SOLID cho toàn bộ codebase
- [ ] Cấu hình CI (build check tự động khi push)
- [ ] Review PR trước khi merge vào `main`

### Chính — Di chuyển & Camera
- [ ] Tích hợp `driver_motor` vào luồng nhận lệnh BLE/WiFi
- [ ] Thêm PWM để điều chỉnh tốc độ động cơ
- [ ] Module camera (stream / chụp ảnh gửi server)

### Phát — Nhiệt độ & Độ ẩm
- [ ] Wrapper I2C dùng chung với timeout + retry
- [ ] Đóng gói dữ liệu HTU21D → publish MQTT

### Nghĩa — Ánh sáng
- [ ] Hoàn thiện BH1750 driver
- [ ] Tổng hợp tất cả dữ liệu sensor vào một struct chung
- [ ] Gửi payload JSON qua MQTT/TLS lên AWS IoT Core

### Duy — Giám sát pin
- [ ] Hoàn thiện MAX17043 đọc SOC + voltage
- [ ] Cảnh báo pin thấp (log / LED / gửi alert)
- [ ] Kiểm thử dashboard Grafana + Lambda

---

Hệ thống nhúng chạy trên ESP32 sử dụng ESP-IDF framework, tích hợp nhiều cảm biến và module điều khiển động cơ qua giao tiếp I2C.

---

## Phần cứng

| Thành phần | Vai trò |
|---|---|
| ESP32 DevKit V1 | Vi điều khiển trung tâm |
| HTU21D | Cảm biến nhiệt độ & độ ẩm (I2C) |
| BH1750 | Cảm biến ánh sáng (I2C) |
| MAX17043 | Đo điện áp & dung lượng pin (I2C) |
| Motor Driver (L298N) | Điều khiển động cơ DC |

---

## Cấu trúc thư mục

```
IMIC-FINAL_PROJECT/
├── include/
│   ├── htu21d.h          # Cảm biến nhiệt độ, độ ẩm
│   ├── BH_1750.h         # Cảm biến ánh sáng
│   ├── max17043.h        # Đo pin
│   └── driver_motor.h    # Điều khiển động cơ
├── src/
│   ├── main.c            # Entry point
│   ├── htu21d.c
│   ├── BH_1750.c
│   ├── max17043.c
│   └── driver_motor.c    # Xử lý lệnh điều khiển động cơ
├── platformio.ini
└── CMakeLists.txt
```

---

## Các module

### HTU21D — Nhiệt độ & Độ ẩm
- Giao tiếp I2C
- `htu21d_get_temperature()` — trả về °C
- `htu21d_get_humidity()` — trả về %RH

### BH1750 — Ánh sáng
- Giao tiếp I2C, địa chỉ `0x23` (ADDR=GND) hoặc `0x5C` (ADDR=VCC)
- Hỗ trợ nhiều chế độ đo (Continuous / One-time, độ phân giải 0.5–4 lx)
- `bh1750_read_lux()` — trả về lux

### MAX17043 — Giám sát pin
- Giao tiếp I2C, địa chỉ `0x36`
- SCL: GPIO22, SDA: GPIO21
- `read_battery_voltage()` — điện áp (V)
- `read_soc()` — dung lượng pin (%)

### Driver Motor — Điều khiển động cơ
- Nhận 6 lệnh từ server qua BLE/WiFi:

| Lệnh | Giá trị | Hành động |
|---|---|---|
| `MOTOR_CMD_FORWARD`  | `0x01` | Tiến |
| `MOTOR_CMD_BACKWARD` | `0x02` | Lùi |
| `MOTOR_CMD_LEFT`     | `0x03` | Rẽ trái |
| `MOTOR_CMD_RIGHT`    | `0x04` | Rẽ phải |
| `MOTOR_CMD_RUN`      | `0x05` | Chạy |
| `MOTOR_CMD_STOP`     | `0x06` | Dừng |

- GPIO điều khiển: IN1=25, IN2=26, IN3=27, IN4=14

---

## Build & Flash

```bash
# Build
pio run

# Flash
pio run --target upload

# Monitor serial
pio device monitor
```

---

## Yêu cầu

- [PlatformIO](https://platformio.org/)
- Framework: `espidf`
- Board: `esp32doit-devkit-v1`
