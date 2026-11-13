#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_camera.h"

#define ROI_INPUT_W 96
#define ROI_INPUT_H 96
#define CAM_FRAME_W 320
#define CAM_FRAME_H 240

typedef struct {
  int x;
  int y;
  int w;
  int h;
} roi_t; // Đơn vị pixel trên ảnh QVGA 320x240

/**
 * @brief Khởi tạo camera OV2640 chế độ ảnh xám QVGA 320x240 trong PSRAM.
 */
bool camInit();

/**
 * @brief Lấy một khung hình (thread-safe bằng mutex nội bộ). Nhớ gọi camReturn() sau khi dùng xong.
 */
camera_fb_t *camCapture();

/**
 * @brief Trả lại framebuffer cho driver camera và nhả mutex.
 */
void camReturn(camera_fb_t *fb);

/**
 * @brief Cắt vùng ROI và thu nhỏ (nearest neighbor) về 96x96 ảnh xám.
 */
void cropResize(const camera_fb_t *fb, roi_t r, uint8_t *out /* 96*96 */);

/**
 * @brief Chuyển khung ảnh xám sang JPEG (người gọi phải free(*outBuf) nếu trả về true).
 */
bool camFrameToJpeg(const camera_fb_t *fb, uint8_t quality, uint8_t **outBuf, size_t *outLen);
