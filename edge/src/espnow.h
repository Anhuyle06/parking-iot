#pragma once

#include <stdint.h>
#include "protocol.h"

/**
 * @brief Khởi tạo ESP-NOW và đăng ký peer Node (gọi sau khi WiFi.begin kết nối cùng kênh).
 */
bool espnowInit(const uint8_t *nodeMac);

/**
 * @brief Lấy gói dữ liệu cảm biến mới nhất từ queue nhận ESP-NOW.
 */
bool espnowPopSensor(node_pkt_t *out);

/**
 * @brief Gửi lệnh điều khiển edge_cmd_t xuống Node.
 */
bool espnowSendCmd(const edge_cmd_t *cmd);

/**
 * @brief Ước lượng tỉ lệ mất gói ESP-NOW từ trường seq của Node (0.0 .. 1.0).
 */
float espnowGetPktLoss();

/**
 * @brief Thời điểm (millis) nhận gói gần nhất từ Node.
 */
uint32_t espnowGetLastRxMs();
