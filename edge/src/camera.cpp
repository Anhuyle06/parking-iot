/**
 * @file camera.cpp
 * @brief Module camera OV2640 & cắt ROI 96x96 trên thiết bị biên – Người 1.
 */

#include "camera.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "img_converters.h"
#include "camera_pins.h"

static SemaphoreHandle_t camMutex = nullptr;

bool camInit() {
  if (camMutex == nullptr) {
    camMutex = xSemaphoreCreateMutex();
  }

  camera_config_t c = {};
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer = LEDC_TIMER_0;
  c.pin_d0 = Y2_GPIO_NUM;
  c.pin_d1 = Y3_GPIO_NUM;
  c.pin_d2 = Y4_GPIO_NUM;
  c.pin_d3 = Y5_GPIO_NUM;
  c.pin_d4 = Y6_GPIO_NUM;
  c.pin_d5 = Y7_GPIO_NUM;
  c.pin_d6 = Y8_GPIO_NUM;
  c.pin_d7 = Y9_GPIO_NUM;
  c.pin_xclk = XCLK_GPIO_NUM;
  c.pin_pclk = PCLK_GPIO_NUM;
  c.pin_vsync = VSYNC_GPIO_NUM;
  c.pin_href = HREF_GPIO_NUM;
  c.pin_sccb_sda = SIOD_GPIO_NUM;
  c.pin_sccb_scl = SIOC_GPIO_NUM;
  c.pin_pwdn = PWDN_GPIO_NUM;
  c.pin_reset = RESET_GPIO_NUM;

  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_GRAYSCALE; // Model dùng ảnh xám -> nhẹ hơn RGB
  c.frame_size = FRAMESIZE_QVGA;        // 320x240
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.fb_count = 1;
  c.grab_mode = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&c);
  if (err != ESP_OK) {
    Serial.printf("[Cam] esp_camera_init failed: 0x%x\n", err);
    return false;
  }
  return true;
}

camera_fb_t *camCapture() {
  if (camMutex != nullptr && xSemaphoreTake(camMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
    return nullptr;
  }
  camera_fb_t *fb = esp_camera_fb_get();
  if (fb == nullptr && camMutex != nullptr) {
    xSemaphoreGive(camMutex);
  }
  return fb;
}

void camReturn(camera_fb_t *fb) {
  if (fb != nullptr) {
    esp_camera_fb_return(fb);
  }
  if (camMutex != nullptr) {
    xSemaphoreGive(camMutex);
  }
}

void cropResize(const camera_fb_t *fb, roi_t r, uint8_t *out) {
  if (fb == nullptr || fb->buf == nullptr || out == nullptr || r.w <= 0 || r.h <= 0) {
    return;
  }
  const int maxW = static_cast<int>(fb->width);
  const int maxH = static_cast<int>(fb->height);

  for (int y = 0; y < ROI_INPUT_H; ++y) {
    int sy = r.y + (y * r.h) / ROI_INPUT_H;
    sy = constrain(sy, 0, maxH - 1);
    for (int x = 0; x < ROI_INPUT_W; ++x) {
      int sx = r.x + (x * r.w) / ROI_INPUT_W;
      sx = constrain(sx, 0, maxW - 1);
      out[y * ROI_INPUT_W + x] = fb->buf[sy * maxW + sx];
    }
  }
}

bool camFrameToJpeg(const camera_fb_t *fb, uint8_t quality, uint8_t **outBuf, size_t *outLen) {
  if (fb == nullptr || outBuf == nullptr || outLen == nullptr) {
    return false;
  }
  return frame2jpg(const_cast<camera_fb_t *>(fb), quality, outBuf, outLen);
}
