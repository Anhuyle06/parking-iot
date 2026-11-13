## Mô tả thay đổi
<!-- Mô tả ngắn gọn thay đổi thuộc phần việc nào (T04–T14, N03) -->

## Nhánh nguồn
- [ ] `node-cam` (Người 1)
- [ ] `edge-ml` (Người 2)
- [ ] `coreiot` (Người 3)

## Checklist trước khi merge vào `main`
- [ ] Không thay đổi `common/protocol.h` ngoài thỏa thuận của cả 3 thành viên.
- [ ] Không commit thông tin nhạy cảm (`config.h`, WiFi password, CoreIOT token).
- [ ] Không commit dataset ảnh lớn (`ml/data/raw/`) hay file trọng số `.tflite`/`.keras`.
- [ ] Đã kiểm thử thực tế hoặc mô phỏng và cập nhật README tương ứng nếu cần.
