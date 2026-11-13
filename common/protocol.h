// common/protocol.h
// Hợp đồng dữ liệu giữa Node cảm biến (YoloUNO) và Thiết bị biên (ESP32-S3 + Camera)
// Không tự ý thay đổi cấu trúc gói tin mà chưa thống nhất với cả nhóm.
#pragma once

#include <stdint.h>

#define N_SLOTS 4

enum {
  MSG_SENSOR = 1,
  MSG_CMD = 2
};

typedef struct __attribute__((packed)) { // node -> edge, 5 lần/giây
  uint8_t msg_type;                      // MSG_SENSOR
  uint8_t node_id;
  uint32_t seq;                          // để tính tỉ lệ mất gói
  float dist_cm[N_SLOTS];                // -1 nếu lỗi
  uint8_t ir_mask;                       // bit i = IR chỗ i phát hiện vật
  uint8_t change_mask;                   // bit i = chỗ i vừa đổi trạng thái và đã ổn định 2 s -> edge chụp ngay
} node_pkt_t;

enum {
  ST_EMPTY = 0,
  ST_OK = 1,
  ST_WRONG = 2,
  ST_UNKNOWN = 3
};

// Giá trị quy ước cho led_override[i] (0 = theo slot_state, khác 0 = màu do RPC đặt)
enum {
  LED_OVR_AUTO = 0,
  LED_OVR_GREEN = 1,
  LED_OVR_RED = 2,
  LED_OVR_YELLOW = 3,
  LED_OVR_OFF = 4
};

typedef struct __attribute__((packed)) { // edge -> node, mỗi 1 s hoặc khi có lệnh
  uint8_t msg_type;                      // MSG_CMD
  uint8_t barrier;                       // 1 = mở, 0 = đóng
  uint8_t buzzer;                        // 1 = kêu báo vi phạm
  uint8_t free_slots;
  uint8_t slot_state[N_SLOTS];           // ST_* -> node tự đổi màu LED: xanh/đỏ/vàng nhấp nháy/tắt
  uint8_t led_override[N_SLOTS];         // 0 = theo slot_state, khác 0 = màu do RPC đặt
} edge_cmd_t;
