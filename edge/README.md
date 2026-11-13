# Firmware Thiết bị biên (`edge/`) – ESP32-S3 + Camera + TinyML

Thiết bị biên đóng vai trò **IoT Gateway + Edge AI**: chụp ảnh bãi từ camera OV2640, cắt 5 vùng ROI (`4 ô đỗ + 1 vùng cấm`), chạy suy luận TinyML `int8`, kết hợp dữ liệu cảm biến ESP-NOW từ Node, gửi telemetry/sự kiện về CoreIOT, phục vụ Webserver hiệu chỉnh ROI tại chỗ và hỗ trợ OTA.

---

## 1. Phân chia module tránh xung đột merge (Mục 3.1)

| File trong `edge/src/` | Nội dung | Phụ trách |
|---|---|---|
| `camera.cpp` / `camera.h` | `camInit()`, `camCapture()`, `cropResize(roi, out96)` | **Người 1** |
| `web.cpp` / `web.h`, `espnow.cpp` / `espnow.h` | Webserver (`/capture`, `/roi`, `/api/*`), ROI calibration, ESP-NOW | **Người 1** |
| `ai.cpp` / `ai.h`, `model_data.h` | Suy luận TFLite Micro `int8`, tensor arena trong PSRAM | **Người 2** |
| `logic.cpp` / `logic.h`, `mqtt.cpp` / `mqtt.h` | Hậu xử lý, kết hợp cảm biến, vi phạm, MQTT telemetry & RPC | **Người 2** |
| `ota.cpp` / `ota.h` | Cập nhật firmware OTA qua CoreIOT | **Người 3** |
| `main.cpp` | Khởi tạo & điều phối các task FreeRTOS | **Người 2 giữ** |

---

## 2. Cấu hình phần cứng, PSRAM & Chân Camera

1. **Chốt chân camera trong [`include/camera_pins.h`](include/camera_pins.h)**:
   - Chạy ví dụ `CameraWebServer` của Arduino-ESP32 trước ở Tuần 11 để xác nhận đúng model chân (`CAMERA_MODEL_ESP32S3_EYE`, `CAMERA_MODEL_XIAO_ESP32S3`, hoặc `CAMERA_MODEL_CUSTOM`).
2. **Bắt buộc bật PSRAM**:
   - **PlatformIO**: đã khai báo `-DBOARD_HAS_PSRAM` và `memory_type = qio_opi` trong `platformio.ini` (đổi sang `qio_qspi` nếu board dùng PSRAM QSPI).
   - **Arduino IDE**: `Tools -> PSRAM -> OPI PSRAM` (hoặc `QSPI PSRAM` tùy board); `Partition Scheme -> 16M Flash (3MB APP / ...)` hoặc dùng `partitions_16MB_ota.csv` để đủ 2 vùng OTA cho firmware có model AI.
3. **Đồng bộ kênh ESP-NOW & WiFi**:
   - Edge kết nối WiFi router trước (`WiFi.begin`), khi đó ESP-NOW tự chạy trên kênh của router. Node cũng phải `WiFi.begin` vào **cùng router** để giao tiếp đúng kênh.
4. **Cấu hình MAC / SSID / Token**:
   ```bash
   cp edge/include/config.example.h edge/include/config.h
   ```
   Cập nhật `WIFI_SSID`, `WIFI_PASS`, `NODE_MAC` (địa chỉ MAC của Node YoloUNO) và `COREIOT_TOKEN`.

---

## 3. Luồng FreeRTOS (`edge/src/main.cpp`)

| Task | Chức năng |
|---|---|
| `espnowRx` (callback + queue) | Đẩy `node_pkt_t` vào queue FreeRTOS (không xử lý nặng trong callback) |
| `visionTask` | Chạy khi có `change_mask`, khi có RPC `captureNow`, hoặc định kỳ mỗi `5 s`: chụp -> cắt 5 vùng `96x96` -> `classifySlot` -> hậu xử lý -> cập nhật trạng thái; đo `frame_ms`, `infer_ms` |
| `mqttTask` | Duy trì MQTT, gửi telemetry mỗi `5 s` + sự kiện ngay khi đổi trạng thái, nhận RPC & Shared Attributes |
| `cmdTask` | Gửi `edge_cmd_t` xuống Node mỗi `1 s` hoặc khi có lệnh; chế độ `auto`: `free_slots == 0` -> đóng barrier; có vi phạm chưa xác nhận -> bật còi |

---

## 4. Phiên bản công cụ & thư viện đã chốt

*(Cập nhật sau Tuần 12 khi nhóm chốt board & thư viện thực tế)*:
- Arduino-ESP32 Core: `...`
- `ArduinoJson`: `^7.2.0`
- `PubSubClient` / `ThingsBoard`: `^2.8`
- TFLite Micro (`Chirale_TensorFlowLite` hoặc Edge Impulse): `...`

---

## 5. Minh chứng cần thu thập (Mục 4.4 & 5.4)

- [ ] Thời gian chụp 1 khung (`frame_ms`), thời gian cắt 4+1 vùng, `infer_ms`, RAM/PSRAM còn trống.
- [ ] Ảnh/video Webserver: ảnh camera, kéo thả vẽ lại ROI, điều khiển barrier tại chỗ.
- [ ] Log Serial OTA: `fw_version` và `model_version` thay đổi trước & sau OTA.
