# CoreIOT: Device Profile, Dashboard, RuleChain, Notification & OTA (`coreiot/`) – Người 3

Thư mục này lưu cấu hình xuất từ nền tảng **CoreIOT (`app.coreiot.io`)** và ảnh chụp minh chứng cho các yếu tố **T11 (1.0đ), T12 (1.0đ), T13 (0.5đ), T14 (2.0đ)**.

---

## 1. T11 – Device Profile, Device & Dashboard

1. **Device Profile**: tạo profile `ParkingEdgeCam` với:
   - `Inactivity timeout`: `60 s`
   - Default RuleChain: `Parking Logic` (Mục 2)
   - Firmware OTA: bật hỗ trợ OTA cho `parking-edge`
2. **Device**: tạo thiết bị `parking-edge-01` thuộc profile `ParkingEdgeCam`, lấy **Access Token** đưa vào `edge/include/config.h` (`COREIOT_TOKEN`).
3. **Shared Attributes**: cấu hình theo mẫu [`shared_attributes.example.json`](shared_attributes.example.json):
   - `roi_json`, `conf_min` (`0.7`), `wrong_confirm_s` (`10`), `nopark_confirm_s` (`10`).
4. **Dashboard** (`dashboard.json`):
   - Card `"Còn X / 4 chỗ"` + `"Vi phạm: N"` (`free_slots`, `violations`).
   - Sơ đồ bãi (Image Map / HTML card) hiển thị 4 ô + vùng cấm (xanh trống, đỏ đỗ đúng, vàng nhấp nháy sai vị trí).
   - Bảng vi phạm từ telemetry events + biểu đồ theo thời gian.
   - Các widget điều khiển RPC (lưu ý đổi widget Switch mặc định từ `getValue`/`setValue` sang `getBarrier`/`setBarrier` với payload `{"state":"open"}` / `{"state":"close"}` khớp `common/README.md`).
   - Widget chỉnh Shared Attributes (`conf_min`, `wrong_confirm_s`) và bảng chẩn đoán (`infer_ms`, `pkt_loss`, `heap_free`, `fw_version`, `model_version`).

---

## 2. T12 – RuleChain `Parking Logic`

Tạo RuleChain `Parking Logic` với 5 luồng chính (bật Debug trên từng rule node để chụp minh chứng):
1. **Đỗ sai vị trí**: `Post telemetry` -> `msg.event === 'wrong_parking'` -> Create Alarm `WRONG_PARKING` (`Warning`, lưu ô & loại vi phạm vào details). Nhận `violation_cleared` -> Clear Alarm.
2. **Vi phạm kéo dài**: lưu thời điểm bắt đầu vào server attribute `slotN_wrong_since`; mỗi telemetry nếu vẫn sai `> 2 phút` -> nâng alarm lên `Critical` + gửi RPC bật còi.
3. **Bãi đầy -> đóng barrier**: `msg.free_slots === 0` -> RPC `setBarrier {"state":"close"}` + Alarm `PARKING_FULL`; ngược lại Clear Alarm + mở barrier.
4. **Đỗ chắn lối đi**: `msg.violation_type === 'vung_cam'` -> Alarm `NO_PARKING_ZONE` (`Major`).
5. **Mất kết nối**: `Inactivity Event` -> Alarm `EDGE_OFFLINE`; `Activity Event` -> Clear Alarm.

Sau khi hoàn thiện, **Export RuleChain** từ CoreIOT ghi đè vào [`rulechain.json`](rulechain.json) và **Export Dashboard** ghi đè vào [`dashboard.json`](dashboard.json).

---

## 3. T13 – Notification

1. Vào **Notification center** -> tạo template (vd: `"Xe đỗ sai vị trí tại ô ${slot}, loại: ${type}"`) và recipients.
2. Tạo **Notification rule** trigger theo `Alarm`, lọc các loại alarm ở mục 2 (`WRONG_PARKING`, `NO_PARKING_ZONE`, `PARKING_FULL`, `EDGE_OFFLINE`); bật kênh Web (+ Email nếu tài khoản hỗ trợ).
3. Chụp ảnh thông báo nhận được kèm dấu thời gian khớp với alarm lưu vào `coreiot/screenshots/`.

---

## 4. T14 – Firmware OTA cho Thiết bị biên

1. Thử sớm trên sketch tối giản ở Tuần 12–13 trước khi ghép vào `edge/src/ota.cpp`.
2. Build file `.bin` bằng PlatformIO (`edge/.pio/build/esp32s3_edge/firmware.bin`) hoặc Arduino IDE (`Export Compiled Binary`), kiểm tra kích thước vừa với phân vùng trong `edge/partitions_16MB_ota.csv`.
3. Upload lên **CoreIOT -> OTA updates**: `title = parking-edge`, `version = 1.0.1`, gán cho device/profile.
4. Thử nghiệm demo:
   - Nâng cấp `v1.0.0` (model v1) -> `v1.0.1` (model v2) và chụp `fw_version`, `model_version` đổi trên Dashboard + log Serial.
   - Thử sai checksum để xác nhận thiết bị từ chối (`FAILED`).

---

## 5. Minh chứng cần lưu vào `coreiot/screenshots/` (Mục 6.6)

- [ ] Ảnh Device Profile, Shared Attributes, Dashboard hoàn chỉnh.
- [ ] Ảnh RuleChain `Parking Logic` + Debug events đi qua từng node.
- [ ] Danh sách Alarm & ảnh Notification nhận được.
- [ ] Ảnh tiến trình OTA (`DOWNLOADING -> UPDATED`) + log Serial trước/sau nâng cấp.
