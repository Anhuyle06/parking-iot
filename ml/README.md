# Pipeline TinyML (`ml/`) – Người 2 (T05)

Bộ công cụ thu thập ảnh từ thiết bị biên, gán nhãn theo từng ô `96x96` xám, huấn luyện mô hình phân loại 3 lớp (`0=trống`, `1=đỗ đúng`, `2=sai vị trí`) và lượng tử hoá `int8` thành `edge/src/model_data.h`.

---

## 1. Cài đặt môi trường Python

```bash
cd ml
python -m venv .venv
# Windows PowerShell: .venv\Scripts\Activate.ps1
# Linux/macOS: source .venv/bin/activate
pip install -r requirements.txt
```

---

## 2. Quy trình thực hiện (Tuần 12–14)

### Bước 1: Thu thập dữ liệu (`collect.py`)
Đảm bảo Thiết bị biên đã chạy và có endpoint `/capture` + `/roi`:
```bash
python collect.py --edge-url http://<IP_EDGE> --session s01_sang --interval 1.5 --sync-roi
```
**Mục tiêu dữ liệu**: `>= 300 ảnh/lớp`, gồm `3–4` mẫu xe khác màu/cỡ, đặt đúng, lệch trái/phải, đè vạch, nhô ra ngoài, `3` mức ánh sáng và có bóng tay/người trong khung. Không commit ảnh trong `ml/data/raw/` lên Git; đồng bộ lên Google Drive và gắn link vào `README.md` gốc.

### Bước 2: Gán nhãn (`label_tool.py`)
```bash
python label_tool.py --raw-dir data/raw --out-csv data/labels.csv
```
- `0`: Trống · `1`: Đỗ đúng · `2`: Sai vị trí (đè vạch / nhô > 1/4 xe / vùng cấm)
- `A` + `0/1/2`: gán nhanh cả khung khi bố trí xe cố định
- `Z`: hoàn tác · `Q`: lưu vào `data/labels.csv` và thoát

### Bước 3: Huấn luyện (`train.py`)
```bash
python train.py --raw-dir data/raw --labels-csv data/labels.csv --epochs 35 --test-sessions s03_toi s04_bong
```
- Chia tập train/test **theo phiên (`session`)** để tránh rò rỉ dữ liệu.
- Chỉ tăng cường độ sáng, tương phản, nhiễu và dịch chuyển `<= 3 px`; **không lật hoặc xoay mạnh**.
- Xuất `models/best_model.keras`, `models/training_curves.png` và `models/confusion_matrix.png`.

### Bước 4: Export `int8` & Nạp model vào Firmware (`export.py`)
```bash
python export.py --model models/best_model.keras --out-header ../edge/src/model_data.h --model-version 1.0.0
```
Sau đó bật `-DUSE_TFLITE_MICRO` trong `edge/platformio.ini` (hoặc cài thư viện `Chirale_TensorFlowLite` trong Arduino IDE) rồi biên dịch và nạp lại firmware cho ESP32-S3. Khi cập nhật model `v1.0.1` ở Tuần 14–15, tăng `--model-version 1.0.1` và build file `.bin` để Người 3 đẩy OTA qua CoreIOT.

---

## 3. Minh chứng cần thu thập (Mục 5.4)

- [ ] Ảnh giao diện `collect.py` / `label_tool.py`, bảng thống kê số ảnh theo lớp và theo phiên.
- [ ] Đồ thị huấn luyện (`training_curves.png`), `confusion_matrix.png`, Precision/Recall lớp `sai vị trí`.
- [ ] Bảng so sánh: **chỉ cảm biến siêu âm** vs **Camera + TinyML** (đếm chỗ & phát hiện sai vị trí).
- [ ] Kết quả thử nghiệm thực tế: 20 lần đỗ đúng, 20 lần đỗ sai, 10 lần vùng cấm ở 2 mức ánh sáng.
