/**
 * @file ai.cpp
 * @brief Suy luận TinyML int8 trên ESP32-S3 với tensor arena trong PSRAM – Người 2 (T04).
 */

#include "ai.h"

#include <Arduino.h>
#include <math.h>
#include <esp_heap_caps.h>
#include "camera.h"
#include "model_data.h"

#if __has_include("config.h")
#include "config.h"
#else
#include "config.example.h"
#endif

#ifdef USE_TFLITE_MICRO
#include <Chirale_TensorFlowLite.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

static uint8_t *tensorArena = nullptr;
static tflite::AllOpsResolver resolver;
static const tflite::Model *model = nullptr;
static tflite::MicroInterpreter *interp = nullptr;
#endif

static bool aiReady = false;

bool aiInit() {
#ifdef USE_TFLITE_MICRO
  if (g_model_data_len == 0) {
    Serial.println("[AI] model_data.h dang la placeholder; hay chay ml/export.py truoc.");
    aiReady = false;
    return false;
  }
  tensorArena = static_cast<uint8_t *>(heap_caps_malloc(TENSOR_ARENA_SIZE, MALLOC_CAP_SPIRAM));
  if (tensorArena == nullptr) {
    Serial.println("[AI] Cap phat tensor arena trong PSRAM that bai!");
    return false;
  }
  model = tflite::GetModel(g_model_data);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("[AI] TFLite schema version khong khop!");
    return false;
  }
  static tflite::MicroInterpreter staticInterp(model, resolver, tensorArena, TENSOR_ARENA_SIZE);
  interp = &staticInterp;
  if (interp->AllocateTensors() != kTfLiteOk) {
    Serial.println("[AI] AllocateTensors that bai!");
    return false;
  }
  aiReady = true;
  Serial.printf("[AI] Ready (model=%s, arena_used=%u)\n",
                MODEL_VERSION, static_cast<unsigned>(interp->arena_used_bytes()));
  return true;
#else
  Serial.println("[AI] Che do stub (chua bat -DUSE_TFLITE_MICRO).");
  aiReady = false;
  return false;
#endif
}

int classifySlot(const uint8_t *img96, float *conf) {
  if (conf != nullptr) {
    *conf = 0.0f;
  }
#ifdef USE_TFLITE_MICRO
  if (!aiReady || interp == nullptr || img96 == nullptr) {
    return -1;
  }
  TfLiteTensor *in = interp->input(0);
  float s = in->params.scale;
  int zp = in->params.zero_point;
  for (int i = 0; i < ROI_INPUT_W * ROI_INPUT_H; ++i) {
    // pixel 0..255 -> int8 theo tham số lượng tử hóa của model
    in->data.int8[i] = static_cast<int8_t>(constrain(lroundf(img96[i] / 255.0f / s) + zp, -128, 127));
  }
  if (interp->Invoke() != kTfLiteOk) {
    return -1;
  }
  TfLiteTensor *out = interp->output(0);
  int best = 0;
  float p[3] = {0};
  for (int k = 0; k < 3; ++k) {
    p[k] = (out->data.int8[k] - out->params.zero_point) * out->params.scale;
    if (p[k] > p[best]) {
      best = k;
    }
  }
  if (conf != nullptr) {
    *conf = p[best];
  }
  return best; // 0 trống · 1 đúng · 2 sai vị trí
#else
  (void)img96;
  return -1;
#endif
}
