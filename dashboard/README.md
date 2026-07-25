# IMIC Robot — Web Dashboard

Trang web tĩnh (`index.html`) để **bất kỳ ai có link đều xem được telemetry và điều khiển robot**, kết nối thẳng tới AWS IoT Core qua MQTT-over-WebSocket (không cần backend server).

## Cách hoạt động

- Trình duyệt lấy **credentials tạm thời, không cần đăng nhập** từ một **Cognito Identity Pool** (unauthenticated identity).
- Dùng credentials đó kết nối MQTT tới AWS IoT Core (`wss://...`).
- Subscribe `imic-esp32/data` để hiển thị telemetry, publish `imic-esp32/cmd` để điều khiển motor.

## Bước 1 — Tạo Cognito Identity Pool

1. AWS Console → **Cognito** → **Identity pools** → **Create identity pool**.
2. Đặt tên, ví dụ `imic_dashboard_pool`.
3. Bật **"Enable access to unauthenticated identities"** (Guest access).
4. Tạo pool — Cognito sẽ tự tạo 2 IAM Role: `Cognito_imic_dashboard_poolAuth` và `Cognito_imic_dashboard_poolUnauth`.
5. Copy **Identity pool ID** (dạng `ap-southeast-2:xxxxxxxx-xxxx-...`) và dán vào `COGNITO_IDENTITY_POOL_ID` trong `index.html`.

## Bước 2 — Gắn quyền IoT cho Role Unauthenticated

Vào **IAM** → **Roles** → mở role `Cognito_imic_dashboard_poolUnauth` → **Add permissions** → **Create inline policy** → tab JSON, dán (thay `<REGION>` và `<ACCOUNT_ID>`):

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": "iot:Connect",
      "Resource": "arn:aws:iot:<REGION>:<ACCOUNT_ID>:client/dashboard-*"
    },
    {
      "Effect": "Allow",
      "Action": "iot:Subscribe",
      "Resource": "arn:aws:iot:<REGION>:<ACCOUNT_ID>:topicfilter/imic-esp32/data"
    },
    {
      "Effect": "Allow",
      "Action": "iot:Receive",
      "Resource": "arn:aws:iot:<REGION>:<ACCOUNT_ID>:topic/imic-esp32/data"
    },
    {
      "Effect": "Allow",
      "Action": "iot:Publish",
      "Resource": "arn:aws:iot:<REGION>:<ACCOUNT_ID>:topic/imic-esp32/cmd"
    }
  ]
}
```

- `<REGION>` = `ap-southeast-2`
- `<ACCOUNT_ID>` = ID tài khoản AWS (12 chữ số, xem góc trên phải Console)

> Đây là quyền **chỉ đọc telemetry + gửi lệnh điều khiển**, không cho phép truy cập gì khác trong account.

## Bước 3 — Host trang web

Có thể:

- **Mở trực tiếp file `index.html`** trên trình duyệt (đơn giản nhất để test).
- Hoặc host public bằng **S3 Static Website Hosting** (miễn phí gần như hoàn toàn) hoặc GitHub Pages, Netlify, Vercel... để có link chia sẻ cho mọi người.

### Host bằng S3 (tuỳ chọn)

1. Tạo bucket S3, ví dụ `imic-dashboard`.
2. Upload `index.html`.
3. Bật **Static website hosting** (Properties tab), set `index.html` làm Index document.
4. Bucket Policy cho phép `s3:GetObject` public-read (chỉ với object trong bucket này).
5. Truy cập qua URL endpoint của static website.

## Cấu hình trong `index.html`

```js
const AWS_REGION = "ap-southeast-2";
const AWS_IOT_ENDPOINT = "a1jnvdnvaug36x-ats.iot.ap-southeast-2.amazonaws.com";
const COGNITO_IDENTITY_POOL_ID = "ap-southeast-2:..."; // điền sau Bước 1
```

## Giao diện

- 5 ô hiển thị: nhiệt độ, độ ẩm, pin (V), pin (%), trạng thái motor — cập nhật mỗi 5s (theo `TELEMETRY_INTERVAL_MS` trong firmware).
- D-pad điều khiển: nhấn giữ để chạy (FORWARD/BACKWARD/LEFT/RIGHT), nhả ra tự gửi STOP — giống hành vi app Android.

## OTA qua Internet (upload firmware lên S3)

Tab "Cập nhật OTA" có nút **"Upload lên S3 & cập nhật qua Internet"**: dashboard
upload file `.bin` lên một bucket S3 (dùng credentials Cognito guest có sẵn),
tạo presigned URL rồi gửi lệnh `{"ota_url": "..."}` qua MQTT — ESP32 tải firmware
qua HTTPS, không cần cùng mạng LAN với dashboard.

### Bước 1 — Tạo bucket S3

1. AWS Console → **S3** → **Create bucket**, ví dụ `imic-ota-firmware`.
2. Giữ **Block all public access = ON** (không cần public, dùng presigned URL).
3. Vào tab **Permissions** → **CORS configuration**, dán:

```json
[
  {
    "AllowedHeaders": ["*"],
    "AllowedMethods": ["PUT", "GET"],
    "AllowedOrigins": ["*"],
    "ExposeHeaders": []
  }
]
```

(Có thể giới hạn `AllowedOrigins` về domain host dashboard thay vì `*`.)

### Bước 2 — Cấp quyền S3 cho Cognito Unauthenticated role

Vào **IAM** → **Roles** → mở role `Cognito_imic_dashboard_poolUnauth` (đã tạo ở
Bước 1 phần IoT trên) → **Add permissions** → **Create inline policy** → tab JSON,
thêm statement sau vào policy hiện có:

```json
{
  "Effect": "Allow",
  "Action": ["s3:PutObject", "s3:GetObject"],
  "Resource": "arn:aws:s3:::imic-ota-firmware/firmware/*"
}
```

### Bước 3 — Cấu hình `index.html`

```js
const OTA_S3_BUCKET = "imic-ota-firmware";
const OTA_S3_PREFIX = "firmware/";
```

> Lưu ý: vì identity pool cho phép **bất kỳ ai** (guest) ghi vào bucket này,
> chỉ cấp quyền trên prefix `firmware/*`, không cấp quyền rộng hơn trên bucket.
