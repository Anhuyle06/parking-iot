"""Công cụ gán nhãn ảnh từng ô bằng phím tắt nhanh (T05).

Phím tắt:
    0 : Trống (empty)
    1 : Đỗ đúng (ok)
    2 : Sai vị trí (wrong)
    A : Gán nhãn toàn khung cùng lớp vừa bấm / hoặc chọn lớp cho cả khung hiện tại
    Z : Hoàn tác (Undo)
    S / Q / ESC : Lưu labels.csv và thoát
"""

import argparse
import csv
from pathlib import Path

import cv2
import numpy as np

CLASS_NAMES = {
    0: "0: TRONG",
    1: "1: DO DUNG",
    2: "2: SAI VI TRI",
}
FIELDNAMES = ["filename", "session", "frame", "region", "label"]


def parse_item_meta(root: Path, img_path: Path) -> tuple[str, str, str, str]:
    """Tách (filename, session, frame, region) từ đường dẫn dạng <session>/f0001_slot1.png."""
    rel = img_path.relative_to(root).as_posix()
    session = img_path.parent.name
    stem = img_path.stem
    if "_" in stem:
        frame, region = stem.split("_", 1)
    else:
        frame, region = stem, "unknown"
    return rel, session, frame, region


def load_existing_labels(csv_path: Path) -> list[dict]:
    if not csv_path.exists():
        return []
    with csv_path.open("r", newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def save_labels(csv_path: Path, rows: list[dict]) -> None:
    csv_path.parent.mkdir(parents=True, exist_ok=True)
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=FIELDNAMES)
        writer.writeheader()
        writer.writerows(rows)


def render_preview(img_path: Path, idx: int, total: int, session: str, frame: str, region: str) -> np.ndarray:
    gray = cv2.imread(str(img_path), cv2.IMREAD_GRAYSCALE)
    if gray is None:
        gray = np.zeros((96, 96), dtype=np.uint8)
    vis = cv2.resize(cv2.cvtColor(gray, cv2.COLOR_GRAY2BGR), (384, 384), interpolation=cv2.INTER_NEAREST)
    canvas = np.full((480, 440, 3), 24, dtype=np.uint8)
    canvas[20:404, 28:412] = vis

    lines = [
        f"[{idx + 1}/{total}] {session} / {frame} / {region}",
        "0: Trong | 1: Do dung | 2: Sai vi tri",
        "A: Gan tat ca vung trong khung | Z: Undo | Q: Luu & Thoat",
    ]
    for i, text in enumerate(lines):
        cv2.putText(canvas, text, (16, 428 + i * 18), cv2.FONT_HERSHEY_SIMPLEX, 0.45, (235, 235, 235), 1)
    return canvas


def main() -> None:
    parser = argparse.ArgumentParser(description="Gán nhãn ảnh cắt 96x96 cho TinyML.")
    parser.add_argument("--raw-dir", default="data/raw", help="Thư mục ảnh thô theo phiên")
    parser.add_argument("--out-csv", default="data/labels.csv", help="File CSV đầu ra")
    args = parser.parse_args()

    raw_dir = Path(args.raw_dir)
    csv_path = Path(args.out_csv)

    all_images = sorted(
        p for p in raw_dir.rglob("*.png")
        if "_full" not in p.stem
    )
    if not all_images:
        print(f"[label_tool] Không tìm thấy ảnh .png trong {raw_dir}")
        return

    rows = load_existing_labels(csv_path)
    labeled_set = {r["filename"] for r in rows}

    # Tạo danh sách item chưa gán nhãn
    pending = []
    for p in all_images:
        rel, session, frame, region = parse_item_meta(raw_dir, p)
        if rel not in labeled_set:
            pending.append((p, rel, session, frame, region))

    if not pending:
        print(f"[label_tool] Tất cả {len(all_images)} ảnh đã được gán nhãn trong {csv_path}.")
        return

    history: list[tuple[int, int]] = []  # (pending_idx, count_added)
    idx = 0
    window_name = "Parking Label Tool"
    cv2.namedWindow(window_name, cv2.WINDOW_AUTOSIZE)

    while 0 <= idx < len(pending):
        p, rel, session, frame, region = pending[idx]
        canvas = render_preview(p, idx, len(pending), session, frame, region)
        cv2.imshow(window_name, canvas)
        key = cv2.waitKey(0) & 0xFF

        if key in (ord("0"), ord("1"), ord("2")):
            lbl = chr(key)
            rows.append({
                "filename": rel, "session": session, "frame": frame, "region": region, "label": lbl
            })
            history.append((idx, 1))
            idx += 1
        elif key in (ord("a"), ord("A")):
            # Gán cùng nhãn cho toàn bộ vùng còn lại thuộc cùng (session, frame)
            print(f"[label_tool] Nhấn 0/1/2 để gán cho cả khung {session}/{frame}...")
            batch_key = cv2.waitKey(0) & 0xFF
            if batch_key in (ord("0"), ord("1"), ord("2")):
                lbl = chr(batch_key)
                added = 0
                while idx + added < len(pending):
                    _, b_rel, b_sess, b_frame, b_reg = pending[idx + added]
                    if b_sess != session or b_frame != frame:
                        break
                    rows.append({
                        "filename": b_rel, "session": b_sess, "frame": b_frame, "region": b_reg, "label": lbl
                    })
                    added += 1
                history.append((idx, added))
                idx += added
        elif key in (ord("z"), ord("Z")) and history:
            prev_idx, count_added = history.pop()
            del rows[-count_added:]
            idx = prev_idx
        elif key in (ord("q"), ord("Q"), ord("s"), ord("S"), 27):
            break

    cv2.destroyAllWindows()
    save_labels(csv_path, rows)
    print(f"[label_tool] Đã lưu {len(rows)} nhãn vào {csv_path}")


if __name__ == "__main__":
    main()
