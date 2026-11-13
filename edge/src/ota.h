#pragma once

#include <ArduinoJson.h>

/**
 * @brief Khởi tạo module OTA qua CoreIOT (T14 – Người 3).
 */
void otaInit();

/**
 * @brief Xử lý shared attributes OTA từ CoreIOT (fw_title, fw_version, fw_checksum, fw_size).
 */
void otaHandleAttributes(JsonObjectConst shared);

/**
 * @brief Chạy vòng lặp trạng thái OTA (tải, kiểm tra checksum, ghi flash & reboot).
 */
void otaLoop();

/**
 * @brief Trả về true nếu đang trong quá trình tải/cập nhật OTA.
 */
bool otaIsUpdating();
