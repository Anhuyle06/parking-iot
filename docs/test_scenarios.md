# Kịch bản ghép hệ thống & thử nghiệm (Tuần 14–15)

| # | Kịch bản | Kết quả mong đợi / Số liệu cần ghi lại | Mã | Trạng thái | Ghi chú / Số liệu đo |
|---:|---|---|---|---|---|
| 1 | Đỗ xe đúng vào ô 2 | LED ô 2 đỏ; web & dashboard cập nhật; `free_slots` giảm 1; sự kiện `car_in`. Ghi độ trễ đầu-cuối (ms) | T04, T06, T07, T11 | [ ] | |
| 2 | Đỗ xe đè vạch giữa ô 3 và 4 | Sau `wrong_confirm_s`: 2 ô báo sai vị trí, gộp 1 vi phạm `de_vach`; LED vàng nhấp nháy, còi; alarm + notification | T04, T12, T13 | [ ] | |
| 3 | Xe lùi vào chuồng chậm (tạm thời lệch rồi vào đúng) | Không báo vi phạm nhầm nhờ xác nhận theo thời gian (`wrong_confirm_s`) | T04 | [ ] | |
| 4 | Đỗ xe ở lối đi (vùng cấm) | Sau `nopark_confirm_s`: vi phạm `vung_cam`, alarm `NO_PARKING_ZONE` | T04, T12 | [ ] | |
| 5 | Để vi phạm quá 2 phút, rồi bấm `ackViolation` | Alarm nâng lên `Critical`; bấm xác nhận trên dashboard -> còi tắt | T11, T12 | [ ] | |
| 6 | Lấp đủ 4 chỗ | RuleChain gửi RPC đóng barrier, alarm `PARKING_FULL`, notification | T12, T13 | [ ] | |
| 7 | Dịch camera, vẽ lại vùng ô trên Webserver | Hệ thống nhận đúng lại sau khi hiệu chỉnh ROI, không cần nạp lại code | T07 | [ ] | |
| 8 | Tắt đèn / rút nguồn thiết bị biên | Tối: chuyển chế độ chỉ đếm bằng cảm biến. Mất Edge: Node fallback, CoreIOT báo `EDGE_OFFLINE` | T06, T12 | [ ] | |
| 9 | OTA `v1.0.0` -> `v1.0.1` (model v2) | Tiến trình OTA `UPDATED`, `fw_version` & `model_version` đổi trên dashboard, độ chính xác model mới | T14 | [ ] | |
| 10 | Chạy liên tục 1 giờ | Mất gói ESP-NOW (`pkt_loss`), số lần mất MQTT, `heap_free` theo thời gian (kiểm tra rò rỉ bộ nhớ) | Đo đạc | [ ] | |

---

## Bảng đo thực nghiệm mô hình AI (Mục 5.4)

| Điều kiện ánh sáng | Đỗ đúng (20 lần) | Đỗ sai (20 lần) | Vùng cấm (10 lần) | Độ chính xác tổng | Độ trễ từ lúc đỗ đến Dashboard (s) |
|---|---|---|---|---|---|
| Ánh sáng đủ (đèn LED trắng) | / 20 | / 20 | / 10 | % | s |
| Ánh sáng yếu / bóng | / 20 | / 20 | / 10 | % | s |
