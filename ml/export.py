"""Lượng tử hóa TFLite int8 và sinh header C edge/src/model_data.h (T05)."""

import argparse
from pathlib import Path

import numpy as np
import tensorflow as tf


def write_c_header(tflite_bytes: bytes, header_path: Path, model_version: str) -> None:
    hex_lines = []
    for i in range(0, len(tflite_bytes), 12):
        chunk = ", ".join(f"0x{b:02x}" for b in tflite_bytes[i:i + 12])
        hex_lines.append(f"  {chunk}")
    array_body = ",\n".join(hex_lines)

    content = (
        "#pragma once\n\n"
        f"// Tự động sinh bởi ml/export.py (MODEL_VERSION={model_version})\n"
        f'#define MODEL_VERSION "{model_version}"\n\n'
        "alignas(16) const unsigned char g_model_data[] = {\n"
        f"{array_body}\n"
        "};\n"
        f"const unsigned int g_model_data_len = {len(tflite_bytes)};\n"
    )
    header_path.write_text(content, encoding="utf-8")


def evaluate_int8(tflite_bytes: bytes, x_test: np.ndarray, y_test: np.ndarray) -> float:
    interp = tf.lite.Interpreter(model_content=tflite_bytes)
    interp.allocate_tensors()
    in_det = interp.get_input_details()[0]
    out_det = interp.get_output_details()[0]

    scale, zp = in_det["quantization"]
    correct = 0
    for img, lbl in zip(x_test, y_test):
        q = np.clip(np.round(img / scale) + zp, -128, 127).astype(np.int8)
        interp.set_tensor(in_det["index"], q[np.newaxis, ...])
        interp.invoke()
        pred = int(np.argmax(interp.get_tensor(out_det["index"])[0]))
        if pred == int(lbl):
            correct += 1
    return correct / max(1, len(y_test))


def main() -> None:
    parser = argparse.ArgumentParser(description="Export Keras model sang TFLite int8 và model_data.h.")
    parser.add_argument("--model", default="models/best_model.keras")
    parser.add_argument("--split-cache", default="models/train_split_cache.npz")
    parser.add_argument("--out-tflite", default="models/model.tflite")
    parser.add_argument("--out-header", default="../edge/src/model_data.h")
    parser.add_argument("--model-version", default="1.0.0")
    args = parser.parse_args()

    model = tf.keras.models.load_model(args.model)
    cache = np.load(args.split_cache)
    x_train = cache["X_train"]

    conv = tf.lite.TFLiteConverter.from_keras_model(model)
    conv.optimizations = [tf.lite.Optimize.DEFAULT]
    conv.representative_dataset = lambda: ([x[None].astype("float32")] for x in x_train[:300])
    conv.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    conv.inference_input_type = tf.int8
    conv.inference_output_type = tf.int8
    tflite_bytes = conv.convert()

    out_tflite = Path(args.out_tflite)
    out_tflite.parent.mkdir(parents=True, exist_ok=True)
    out_tflite.write_bytes(tflite_bytes)

    out_header = Path(args.out_header)
    write_c_header(tflite_bytes, out_header, args.model_version)

    print(f"[export] Đã ghi {out_tflite} ({len(tflite_bytes) / 1024:.1f} KB)")
    print(f"[export] Đã sinh {out_header} với MODEL_VERSION={args.model_version}")

    if "X_test" in cache and "y_test" in cache:
        acc = evaluate_int8(tflite_bytes, cache["X_test"], cache["y_test"])
        print(f"[export] Int8 test accuracy: {acc * 100:.2f}%")


if __name__ == "__main__":
    main()
