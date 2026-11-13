"""Thu thập ảnh từng ô và vùng cấm từ endpoint /capture của thiết bị biên (T05).

Ví dụ sử dụng:
    python collect.py --edge-url http://192.168.1.50 --session s01 --interval 1.5 --sync-roi
"""

import argparse
import io
import json
import time
from pathlib import Path

import requests
from PIL import Image

REGIONS = ("slot1", "slot2", "slot3", "slot4", "nopark")
TARGET_SIZE = (96, 96)


def load_rois(roi_path: Path, edge_url: str | None, sync_roi: bool) -> dict:
    """Đọc ROI từ file cục bộ hoặc đồng bộ trực tiếp từ endpoint /roi của edge."""
    if sync_roi and edge_url:
        resp = requests.get(f"{edge_url.rstrip('/')}/roi", timeout=5)
        resp.raise_for_status()
        rois = resp.json()
        roi_path.write_text(json.dumps(rois, indent=2), encoding="utf-8")
        print(f"[collect] Đã đồng bộ ROI từ {edge_url}/roi -> {roi_path}")
        return rois
    return json.loads(roi_path.read_text(encoding="utf-8"))


def crop_and_resize_gray(img: Image.Image, roi: dict) -> Image.Image:
    """Cắt ROI và thu nhỏ nearest-neighbor về 96x96 ảnh xám (khớp cropResize trên ESP32-S3)."""
    x, y, w, h = int(roi["x"]), int(roi["y"]), int(roi["w"]), int(roi["h"])
    crop = img.convert("L").crop((x, y, x + w, y + h))
    return crop.resize(TARGET_SIZE, Image.Resampling.NEAREST)


def main() -> None:
    parser = argparse.ArgumentParser(description="Thu thập ảnh ROI từ ESP32-S3-CAM.")
    parser.add_argument("--edge-url", required=True, help="VD: http://192.168.1.50")
    parser.add_argument("--session", required=True, help="Tên phiên thu (VD: s01_sang)")
    parser.add_argument("--roi", default="roi.json", help="Đường dẫn roi.json")
    parser.add_argument("--out-dir", default="data/raw", help="Thư mục lưu ảnh cắt")
    parser.add_argument("--interval", type=float, default=1.5, help="Chu kỳ chụp (giây)")
    parser.add_argument("--count", type=int, default=0, help="Số khung cần chụp (0 = chạy đến khi Ctrl+C)")
    parser.add_argument("--sync-roi", action="store_true", help="Tải /roi mới nhất từ edge trước khi thu")
    parser.add_argument("--save-full", action="store_true", help="Lưu thêm ảnh toàn khung 320x240")
    args = parser.parse_args()

    roi_path = Path(args.roi)
    rois = load_rois(roi_path, args.edge_url, args.sync_roi)

    session_dir = Path(args.out_dir) / args.session
    session_dir.mkdir(parents=True, exist_ok=True)
    capture_url = f"{args.edge_url.rstrip('/')}/capture"

    frame_idx = 0
    print(f"[collect] Bắt đầu thu tại {capture_url} -> {session_dir} (Ctrl+C để dừng)")
    try:
        while args.count == 0 or frame_idx < args.count:
            t0 = time.monotonic()
            resp = requests.get(capture_url, timeout=5)
            resp.raise_for_status()
            img = Image.open(io.BytesIO(resp.content))

            frame_idx += 1
            frame_stem = f"f{frame_idx:04d}"
            if args.save_full:
                img.save(session_dir / f"{frame_stem}_full.jpg")

            for region in REGIONS:
                if region not in rois:
                    continue
                patch = crop_and_resize_gray(img, rois[region])
                patch.save(session_dir / f"{frame_stem}_{region}.png")

            print(f"[collect] Đã lưu {frame_stem} ({len(REGIONS)} vùng)")
            elapsed = time.monotonic() - t0
            if elapsed < args.interval:
                time.sleep(args.interval - elapsed)
    except KeyboardInterrupt:
        print(f"\n[collect] Đã dừng. Tổng số khung đã thu: {frame_idx}")


if __name__ == "__main__":
    main()
