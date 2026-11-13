# Node cảm biến & chấp hành (`node/`) – Người 1 (T06, N03)

Firmware chạy trên **YoloUNO (ESP32-S3)** đóng vai trò đọc cảm biến tại từng chỗ đỗ, phát hiện thay đổi ổn định để kích hoạt camera thiết bị biên, và điều khiển LED / còi / barrier theo lệnh từ thiết bị biên qua **ESP-NOW 2 chiều**.

---

## 1. Phần cứng & Sơ đồ chân mặc định

| Linh kiện | SL | Chân mặc định (`config.example.h`) | Lưu ý đấu nối |
|---|---:|---|---|
| YoloUNO (ESP32-S3) | 1 | — | Nguồn USB 5V / mạch ổn áp |
| HC-SR04 (ô 1–4) | 4 | TRIG `{4,6,8,10}`, ECHO `{5,7,9,11}` | **ECHO 5V phải qua cầu phân áp 1kΩ / 2kΩ xuống 3.3V** |
| Cảm biến IR (ô 1–4) | 4 | `{12,13,14,15}` | Cấp 3.3V (hoặc kiểm tra mức logic DO) |
| Dải LED WS2812 (4 bóng) | 1 | `16` | Dùng `Adafruit NeoPixel`, trở ~330Ω nối tiếp chân DIN |
| Còi Buzzer | 1 | `17` | Kích qua transistor NPN + diode bảo vệ |
| Servo SG90 (Barrier) | 1 | `18` | **Cấp nguồn 5V riêng cho servo, nối chung GND** |
| *(Tuỳ chọn N03)* Màn hình LVGL | 1 | SPI / I2C | Hiển thị `"Còn X/4 chỗ"` + ô đang vi phạm |

*(Cập nhật lại bảng chân trong `node/include/config.h` và bổ sung ảnh sơ đồ đấu nối + ảnh mạch thật vào `docs/evidence/`.)*

---

## 2. Cấu hình & Nạp firmware

1. Sao chép cấu hình mẫu:
   ```bash
   cp node/include/config.example.h node/include/config.h
   ```
2. Chỉnh trong `node/include/config.h`:
   - `WIFI_SSID` / `WIFI_PASS`: **trùng với mạng WiFi mà thiết bị biên đang kết nối** để ESP-NOW dùng chung kênh router.
   - `EDGE_MAC`: địa chỉ MAC WiFi STA của thiết bị biên (lấy từ Serial log khi Edge khởi động).
3. Build & Upload bằng PlatformIO:
   ```bash
   cd node
   pio run -t upload
   pio device monitor
   ```
   *(Hoặc dùng Arduino IDE với cấu trúc tương đương và trỏ include path tới `../common/protocol.h`.)*

---

## 3. Logic hoạt động & Chế độ dự phòng

- **Đo siêu âm**: đọc trung vị 3 mẫu (`readUltrasonic(i)`), đo tuần tự các ô cách nhau `>= 35 ms` để tránh nhiễu chéo.
- **Kích hoạt chụp (`change_mask`)**: khi trạng thái có xe/trống thay đổi và giữ ổn định `>= 2 s`, bật bit tương ứng gửi về Edge để Edge chụp ngay thay vì chờ chu kỳ `5 s`.
- **LED trạng thái**:
  - `ST_EMPTY (0)`: xanh lá
  - `ST_OK (1)`: đỏ
  - `ST_WRONG (2)`: vàng nhấp nháy
  - `led_override[i] != 0`: đổi màu theo lệnh RPC từ CoreIOT (`setSlotLed`)
- **Fallback khi mất kết nối Edge (`> 3 s`)**: LED tự chuyển sang hiển thị theo cảm biến siêu âm + IR tại chỗ và tắt còi để tránh kẹt báo động giả.

---

## 4. Minh chứng cần thu thập (Mục 4.4)

- [ ] Sơ đồ đấu nối + ảnh mạch thật + ảnh mô hình bãi & giá treo camera.
- [ ] Log Serial ESP-NOW (`seq`, `ok`, `fail` -> tỉ lệ thành công, độ trễ RTT/2).
- [ ] Video thử nghiệm fallback khi tắt nguồn thiết bị biên.
