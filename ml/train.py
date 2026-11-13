"""Huấn luyện mô hình phân loại 3 lớp (trống / đỗ đúng / sai vị trí) trên ảnh xám 96x96 (T05).

Đặc điểm bắt buộc theo thiết kế:
  - Chia train/test theo PHIÊN THU (session) để tránh rò rỉ dữ liệu giữa các khung liên tiếp.
  - Tăng cường dữ liệu chỉ dùng độ sáng, độ tương phản, nhiễu, dịch chuyển nhỏ (<= 3 px);
    KHÔNG lật hoặc xoay mạnh vì có thể biến ảnh "đỗ đúng" thành "sai vị trí".
"""

import argparse
import csv
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from PIL import Image
from sklearn.metrics import classification_report, confusion_matrix
from sklearn.model_selection import GroupShuffleSplit
import tensorflow as tf


CLASS_LABELS = ["trong (0)", "do_dung (1)", "sai_vi_tri (2)"]


def load_dataset(raw_dir: Path, csv_path: Path) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    xs, ys, sessions = [], [], []
    with csv_path.open("r", newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            img_path = raw_dir / row["filename"]
            if not img_path.exists():
                continue
            arr = np.asarray(Image.open(img_path).convert("L").resize((96, 96)), dtype=np.float32) / 255.0
            xs.append(arr[..., np.newaxis])
            ys.append(int(row["label"]))
            sessions.append(row["session"])
    return np.asarray(xs, dtype=np.float32), np.asarray(ys, dtype=np.int32), np.asarray(sessions)


def build_augmentation() -> tf.keras.Sequential:
    """Chỉ tăng cường độ sáng, tương phản, nhiễu và dịch chuyển <= 3 px (3/96 ~= 0.031)."""
    return tf.keras.Sequential([
        tf.keras.layers.RandomBrightness(0.15, value_range=(0.0, 1.0)),
        tf.keras.layers.RandomContrast(0.15),
        tf.keras.layers.RandomTranslation(height_factor=0.03, width_factor=0.03, fill_mode="nearest"),
        tf.keras.layers.GaussianNoise(0.02),
    ], name="safe_parking_aug")


def build_cnn_model() -> tf.keras.Model:
    """CNN gọn nhẹ tối ưu cho ESP32-S3 + TFLite Micro int8."""
    inputs = tf.keras.Input(shape=(96, 96, 1))
    x = tf.keras.layers.Conv2D(16, 3, strides=2, padding="same", activation="relu")(inputs)
    x = tf.keras.layers.DepthwiseConv2D(3, padding="same", activation="relu")(x)
    x = tf.keras.layers.Conv2D(24, 1, activation="relu")(x)
    x = tf.keras.layers.MaxPooling2D(2)(x)

    x = tf.keras.layers.DepthwiseConv2D(3, padding="same", activation="relu")(x)
    x = tf.keras.layers.Conv2D(32, 1, activation="relu")(x)
    x = tf.keras.layers.MaxPooling2D(2)(x)

    x = tf.keras.layers.DepthwiseConv2D(3, padding="same", activation="relu")(x)
    x = tf.keras.layers.Conv2D(48, 1, activation="relu")(x)
    x = tf.keras.layers.GlobalAveragePooling2D()(x)
    x = tf.keras.layers.Dropout(0.2)(x)
    outputs = tf.keras.layers.Dense(3, activation="softmax")(x)
    return tf.keras.Model(inputs, outputs, name="parking_tiny_cnn")


def split_by_session(
    x: np.ndarray, y: np.ndarray, sessions: np.ndarray, test_sessions: list[str]
) -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    if test_sessions:
        test_mask = np.isin(sessions, test_sessions)
        train_mask = ~test_mask
        return x[train_mask], x[test_mask], y[train_mask], y[test_mask]

    unique_sessions = np.unique(sessions)
    if len(unique_sessions) < 2:
        raise ValueError("Cần ít nhất 2 phiên (session) khác nhau để chia train/test không rò rỉ dữ liệu!")

    splitter = GroupShuffleSplit(n_splits=1, test_size=0.25, random_state=42)
    train_idx, test_idx = next(splitter.split(x, y, groups=sessions))
    return x[train_idx], x[test_idx], y[train_idx], y[test_idx]


def save_plots(history, cm: np.ndarray, out_dir: Path) -> None:
    fig, ax = plt.subplots(1, 2, figsize=(10, 4))
    ax[0].plot(history.history["accuracy"], label="train")
    ax[0].plot(history.history["val_accuracy"], label="test")
    ax[0].set_title("Accuracy")
    ax[0].legend()

    ax[1].plot(history.history["loss"], label="train")
    ax[1].plot(history.history["val_loss"], label="test")
    ax[1].set_title("Loss")
    ax[1].legend()
    fig.tight_layout()
    fig.savefig(out_dir / "training_curves.png", dpi=150)
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(5, 4))
    im = ax.imshow(cm, cmap="Blues")
    ax.set_xticks(range(3), CLASS_LABELS, rotation=20)
    ax.set_yticks(range(3), CLASS_LABELS)
    for i in range(3):
        for j in range(3):
            ax.text(j, i, str(cm[i, j]), ha="center", va="center")
    fig.colorbar(im, ax=ax)
    ax.set_xlabel("Predicted")
    ax.set_ylabel("True")
    ax.set_title("Confusion Matrix")
    fig.tight_layout()
    fig.savefig(out_dir / "confusion_matrix.png", dpi=150)
    plt.close(fig)


def main() -> None:
    parser = argparse.ArgumentParser(description="Train mô hình phân loại ô đỗ xe 96x96.")
    parser.add_argument("--raw-dir", default="data/raw")
    parser.add_argument("--labels-csv", default="data/labels.csv")
    parser.add_argument("--out-dir", default="models")
    parser.add_argument("--epochs", type=int, default=35)
    parser.add_argument("--batch-size", type=int, default=32)
    parser.add_argument("--test-sessions", nargs="*", default=[], help="Danh sách session dùng làm tập test")
    args = parser.parse_args()

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    x, y, sessions = load_dataset(Path(args.raw_dir), Path(args.labels_csv))
    if len(x) == 0:
        raise RuntimeError("Dataset trống. Hãy thu ảnh bằng collect.py và gán nhãn bằng label_tool.py.")

    x_train, x_test, y_train, y_test = split_by_session(x, y, sessions, args.test_sessions)
    print(f"[train] Train samples: {len(x_train)} | Test samples: {len(x_test)}")

    aug = build_augmentation()
    train_ds = (
        tf.data.Dataset.from_tensor_slices((x_train, y_train))
        .shuffle(len(x_train))
        .batch(args.batch_size)
        .map(lambda imgs, lbls: (tf.clip_by_value(aug(imgs, training=True), 0.0, 1.0), lbls),
             num_parallel_calls=tf.data.AUTOTUNE)
        .prefetch(tf.data.AUTOTUNE)
    )
    test_ds = tf.data.Dataset.from_tensor_slices((x_test, y_test)).batch(args.batch_size)

    model = build_cnn_model()
    model.compile(
        optimizer=tf.keras.optimizers.Adam(1e-3),
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"],
    )

    ckpt_path = out_dir / "best_model.keras"
    callbacks = [
        tf.keras.callbacks.ModelCheckpoint(ckpt_path, monitor="val_accuracy", save_best_only=True),
        tf.keras.callbacks.ReduceLROnPlateau(monitor="val_loss", factor=0.5, patience=4),
        tf.keras.callbacks.EarlyStopping(monitor="val_loss", patience=8, restore_best_weights=True),
    ]
    history = model.fit(train_ds, validation_data=test_ds, epochs=args.epochs, callbacks=callbacks)

    best_model = tf.keras.models.load_model(ckpt_path)
    y_pred = np.argmax(best_model.predict(x_test, verbose=0), axis=1)
    cm = confusion_matrix(y_test, y_pred, labels=[0, 1, 2])
    print("\n=== Classification Report ===")
    print(classification_report(y_test, y_pred, labels=[0, 1, 2], target_names=CLASS_LABELS, zero_division=0))
    print("=== Confusion Matrix ===")
    print(cm)

    save_plots(history, cm, out_dir)
    np.savez_compressed(out_dir / "train_split_cache.npz", X_train=x_train, X_test=x_test, y_test=y_test)
    print(f"[train] Đã lưu model tốt nhất tại {ckpt_path}")


if __name__ == "__main__":
    main()
