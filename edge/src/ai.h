#pragma once

#include <stdint.h>

/**
 * @brief Khởi tạo TFLite Micro interpreter và cấp phát tensor arena trong PSRAM.
 */
bool aiInit();

/**
 * @brief Suy luận một vùng ảnh xám 96x96.
 * @param img96 Con trỏ vùng nhớ 96*96 byte (0..255).
 * @param conf  Đầu ra độ tin cậy lớp dự đoán (0.0 .. 1.0).
 * @return 0 = trống, 1 = đỗ đúng, 2 = sai vị trí, -1 nếu model chưa sẵn sàng.
 */
int classifySlot(const uint8_t *img96, float *conf);
