# AWS IoT Core — Reference Config (từ project `imic`)

> Tài liệu tổng hợp cấu hình AWS IoT Core hiện tại để tham khảo / áp dụng cho project IoT khác.
> ⚠️ Không commit private key lên git — xem mục "Lưu ý bảo mật" cuối file.

---

## 1. Thông tin kết nối

| Thông tin | Giá trị | Nguồn |
|---|---|---|
| AWS IoT Endpoint | `a1jnvdnvaug36x-ats.iot.ap-southeast-2.amazonaws.com` | [src/main.cpp:27](src/main.cpp#L27) |
| Region | `ap-southeast-2` (Sydney) — suy ra từ endpoint | |
| Thing name | `imic-esp32` | [src/main.cpp:28](src/main.cpp#L28) |
| Port | `8883` (MQTTS / TLS) | [include/AWSIoT.h:7](include/AWSIoT.h#L7) |
| Keepalive | `60s` | [include/AWSIoT.h:9](include/AWSIoT.h#L9) |
| MQTT buffer size | `512 bytes` | [include/AWSIoT.h:8](include/AWSIoT.h#L8) |

## 2. Topic schema (MQTT)

| Topic | Chiều | Mục đích |
|---|---|---|
| `imic-esp32/data` | publish | telemetry/status |
| `imic-esp32/cmd` | subscribe | nhận lệnh remote |

## 3. Certificate / Key files (mutual TLS)

### a) Embedded trong firmware — [include/aws_certs.h](include/aws_certs.h)
- `AWS_ROOT_CA` — Amazon Root CA 1 (public, dùng chung mọi project)
- `AWS_DEVICE_CERT` — device certificate (gắn với Thing `imic-esp32`)
- `AWS_PRIVATE_KEY` — RSA private key tương ứng (⚠️ bí mật)

### b) File rời ở root repo (tải từ AWS IoT Console khi tạo Thing)
- `1ef6c9ed7ef756e6d68b430494c87d19565303eba2b84f0fa8caa826a5f8b598-certificate.pem.crt`
- `1ef6c9ed7ef756e6d68b430494c87d19565303eba2b84f0fa8caa826a5f8b598-private.pem.key` ⚠️ bí mật
- `1ef6c9ed7ef756e6d68b430494c87d19565303eba2b84f0fa8caa826a5f8b598-public.pem.key`
- `AmazonRootCA1.pem`
- `AmazonRootCA3.pem`

→ Tiền tố `1ef6c9ed7e...` là **Certificate ID** của AWS IoT — dùng để tra cứu/attach policy trong AWS Console.

## 4. Áp dụng cho project IoT khác

### Dùng chung được (không đổi)
- `AmazonRootCA1.pem` / `AWS_ROOT_CA` — CA gốc AWS, giống nhau mọi region/account.
- Pattern code class `AWSIoT` (mutual TLS qua `WiFiClientSecure` + `PubSubClient`).

### Phải tạo mới riêng cho mỗi project/device
1. Tạo **Thing mới** trong AWS IoT Console (vd `project2-esp32`).
2. Tạo **certificate + key pair mới** (1-Click certificate creation) → bộ 3 file `.pem.crt/.key` mới với Certificate ID mới.
3. Tạo **IoT Policy** mới gắn với cert mới, scope đúng topic của project đó (vd `project2-esp32/data`, `project2-esp32/cmd`) — tránh `AllowAll` cho prod.
4. Cập nhật `AWS_ENDPOINT` nếu khác account/region (lấy từ **AWS IoT Console → Settings → Device data endpoint**).
5. Đổi topic schema theo chuẩn `robot/{id}/...` (theo `iot_robot_project_plan.md` mục 8) thay vì `imic-esp32/...`.

## 5. Lưu ý bảo mật

- Các file `*-private.pem.key`, `*-public.pem.key`, `*-certificate.pem.crt` và nội dung `include/aws_certs.h` đang **untracked** trong git nhưng nằm trong working tree — thêm vào `.gitignore`, **không commit**.
- Mỗi project/device mới nên dùng cert + private key riêng, không tái sử dụng giữa các thiết bị/project.
