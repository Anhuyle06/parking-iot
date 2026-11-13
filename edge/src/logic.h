#pragma once

#include <Arduino.h>
#include <stdint.h>
#include "protocol.h"
#include "camera.h"

/**
 * @brief Kết quả suy luận thô cho 4 ô đỗ và vùng cấm.
 */
typedef struct {
  int rawSlotClass[N_SLOTS];
  float slotConf[N_SLOTS];
  int rawNoparkClass;
  float noparkConf;
  uint32_t frameMs;
  uint32_t inferMs;
} vision_result_t;

/**
 * @brief Khởi tạo trạng thái logic bãi đỗ xe, ROI mặc định và mutex đồng bộ.
 */
void logicInit();

/**
 * @brief Cập nhật dữ liệu cảm biến mới nhất từ Node ESP-NOW.
 */
void logicOnSensorPacket(const node_pkt_t &pkt);

/**
 * @brief Hậu xử lý kết quả AI + kết hợp cảm biến, chống dội và sinh sự kiện MQTT.
 */
void logicUpdateVision(const vision_result_t &res);

/**
 * @brief Lấy bản sao danh sách 5 vùng ROI (4 ô + 1 vùng cấm).
 */
void logicGetRois(roi_t outSlots[N_SLOTS], roi_t *outNopark);

/**
 * @brief Cập nhật ROI từ chuỗi JSON (Shared Attribute hoặc Webserver /api/roi).
 */
bool logicUpdateRoiFromJson(const char *jsonStr);

/**
 * @brief Trả chuỗi JSON biểu diễn ROI hiện hành.
 */
String logicGetRoiJson();

/**
 * @brief Cập nhật các ngưỡng suy luận/xác nhận từ Shared Attributes CoreIOT.
 */
void logicUpdateThresholds(float confMin, uint32_t wrongConfirmS, uint32_t noparkConfirmS);

/**
 * @brief Tạo JSON telemetry định kỳ gửi lên CoreIOT và Webserver /api/status.
 */
String logicBuildTelemetryJson();

/**
 * @brief Lấy sự kiện cần gửi ngay lên CoreIOT (nếu có trong hàng đợi nội bộ).
 */
bool logicPopEventJson(String *outJson);

/**
 * @brief Xây dựng gói lệnh edge_cmd_t gửi xuống Node.
 */
void logicBuildEdgeCmd(edge_cmd_t *outCmd);

// Điều khiển từ RPC / Webserver
void logicSetBarrier(bool open);
bool logicGetBarrier();
void logicSetAutoMode(bool autoMode);
bool logicGetAutoMode();
void logicSetSlotLedOverride(int slot1Based, const char *color);
void logicAckViolation(int slot1Based);
void logicAckAllViolations();
void logicRequestImmediateCapture();
bool logicConsumeCaptureRequest();
