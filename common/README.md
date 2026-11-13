# Common Data Contract (`common/`)

Thư mục này chứa hợp đồng dữ liệu dùng chung cho cả 3 thành viên. Mọi thay đổi trong [`protocol.h`](protocol.h) hoặc định dạng JSON bên dưới cần được thông báo và thống nhất trước khi merge.

---

## 1. Giao thức ESP-NOW (`common/protocol.h`)

- Giới hạn gói ESP-NOW: `<= 250 byte` (`sizeof(node_pkt_t) = 24 byte`, `sizeof(edge_cmd_t) = 12 byte`).
- Node và Thiết bị biên phải chạy **cùng kênh WiFi** với router (cả hai gọi `WiFi.begin(...)` vào cùng mạng WiFi).

### `node_pkt_t` (Node -> Thiết bị biên, tần suất 5 lần/giây)
| Trường | Kiểu | Ý nghĩa |
|---|---|---|
| `msg_type` | `uint8_t` | `MSG_SENSOR` (`1`) |
| `node_id` | `uint8_t` | ID của node (mặc định `1`) |
| `seq` | `uint32_t` | Số thứ tự tăng dần để tính `pkt_loss` |
| `dist_cm[4]` | `float[4]` | Khoảng cách HC-SR04 (cm); `-1.0f` nếu đọc lỗi/timeout |
| `ir_mask` | `uint8_t` | Bit `i` = `1` nếu cảm biến IR chỗ `i` phát hiện vật |
| `change_mask` | `uint8_t` | Bit `i` = `1` khi chỗ `i` đổi trạng thái ổn định `>= 2 s` (kích hoạt chụp ngay) |

### `edge_cmd_t` (Thiết bị biên -> Node, mỗi 1 giây hoặc ngay khi có lệnh)
| Trường | Kiểu | Ý nghĩa |
|---|---|---|
| `msg_type` | `uint8_t` | `MSG_CMD` (`2`) |
| `barrier` | `uint8_t` | `1` = mở barrier, `0` = đóng barrier |
| `buzzer` | `uint8_t` | `1` = bật còi báo vi phạm, `0` = tắt |
| `free_slots` | `uint8_t` | Số chỗ trống hiện tại (`0..4`) |
| `slot_state[4]` | `uint8_t[4]` | `ST_EMPTY (0)` xanh, `ST_OK (1)` đỏ, `ST_WRONG (2)` vàng nhấp nháy, `ST_UNKNOWN (3)` tắt/chờ |
| `led_override[4]` | `uint8_t[4]` | `0` (`LED_OVR_AUTO`) = theo `slot_state`; `1..4` = màu đặt qua RPC `setSlotLed` |

---

## 2. MQTT Telemetry & Attributes (`Thiết bị biên -> CoreIOT`)

- Host: `app.coreiot.io`, Port: `1883`, Username: `Access Token` của thiết bị `parking-edge-01`.
- Topic telemetry: `v1/devices/me/telemetry`

### Telemetry định kỳ (mỗi 5 s)
```json
{
  "slot1": 1, "slot2": 0, "slot3": 2, "slot4": 2,
  "slot1_conf": 0.93, "slot2_conf": 0.96, "slot3_conf": 0.81, "slot4_conf": 0.84,
  "free_slots": 1, "total_slots": 4, "violations": 1, "nopark_zone": 0,
  "infer_ms": 410, "frame_ms": 95, "pkt_loss": 0.01, "heap_free": 81234
}
```
Quy ước mã trạng thái ô: `0` = trống · `1` = đỗ đúng · `2` = sai vị trí · `3` = không chắc.

### Telemetry sự kiện (chỉ gửi khi đổi trạng thái đã chống dội)
```json
{ "event": "car_in", "event_slot": 2 }
{ "event": "wrong_parking", "event_slot": 3, "violation_type": "de_vach" }
{ "event": "violation_cleared", "event_slot": 3, "duration_s": 74 }
```
Giá trị `violation_type`: `de_vach` | `ngoai_o` | `vung_cam`.

### Attributes
- **Client attributes** (gửi khi khởi động): `fw_version`, `model_version`, `total_slots`, (tuỳ chọn: `last_violation_img` dạng base64 JPEG `<= 15 KB`).
- **Shared attributes** (Người 3 đặt trên CoreIOT, thiết bị biên đọc và áp dụng ngay):
  - `roi_json` (tọa độ 4 ô + vùng cấm trên ảnh 320x240)
  - `conf_min` (mặc định `0.7`)
  - `wrong_confirm_s` (mặc định `10`)
  - `nopark_confirm_s` (mặc định `10`)

---

## 3. RPC (`CoreIOT -> Thiết bị biên`)

- Request topic: `v1/devices/me/rpc/request/+`
- Response topic: `v1/devices/me/rpc/response/<id>`

| `method` | `params` | Ý nghĩa / Phản hồi |
|---|---|---|
| `setBarrier` | `{"state":"open"}` / `{"state":"close"}` | Mở/đóng barrier. Trả về `{"ok":true,"state":"open"}` |
| `getBarrier` | `—` | Trả về `true` / `false` (dùng cho widget Switch) |
| `setSlotLed` | `{"slot":2,"color":"yellow"}` hoặc `"auto"` | Đặt màu đèn một chỗ (`green`/`red`/`yellow`/`off`/`auto`) |
| `captureNow` | `—` | Chụp + suy luận ngay, trả kết quả 4 ô |
| `ackViolation` | `{"slot":3}` | Quản lý xác nhận đã xử lý: tắt còi chỗ đó |
| `setMode` | `{"mode":"auto"}` / `{"mode":"manual"}` | `auto`: barrier tự đóng khi bãi đầy (`free_slots == 0`) |
| `getStatus` | `—` | Trả về snapshot trạng thái hiện tại |
| `reboot` | `—` | Khởi động lại thiết bị biên |
