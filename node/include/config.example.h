#pragma once
#include <stdint.h>

// Copy file này thành node/include/config.h và điền thông tin thực tế (config.h đã nằm trong .gitignore).

#define NODE_ID 1

// Node cần kết nối cùng router WiFi với thiết bị biên để ESP-NOW chạy đúng kênh.
static const char *WIFI_SSID = "YOUR_WIFI_SSID";
static const char *WIFI_PASS = "YOUR_WIFI_PASSWORD";

// Địa chỉ MAC WiFi STA của thiết bị biên (in ra từ Serial của edge khi khởi động).
static const uint8_t EDGE_MAC[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

// Sơ đồ chân trên YoloUNO (ESP32-S3) — cập nhật theo đấu nối thực tế của Người 1.
// Lưu ý: Chân ECHO của HC-SR04 (5V) PHẢI đi qua cầu phân áp 1kOhm / 2kOhm xuống 3.3V.
static const int TRIG_PINS[4] = {4, 6, 8, 10};
static const int ECHO_PINS[4] = {5, 7, 9, 11};
static const int IR_PINS[4]   = {12, 13, 14, 15};

static const int NEOPIXEL_PIN = 16; // Dải 4 LED WS2812
static const int BUZZER_PIN   = 17; // Điều khiển qua transistor
static const int SERVO_PIN    = 18; // Servo SG90 (nguồn 5V riêng, chung GND)

// Ngưỡng & chu kỳ thời gian
static const float    OCCUPIED_DIST_CM   = 15.0f;  // < 15 cm coi như có vật ở ô
static const bool     IR_ACTIVE_LOW      = true;   // Phổ biến với module IR tránh vật cản
static const uint32_t SONAR_GAP_MS       = 35;     // >= 30 ms giữa các cảm biến siêu âm
static const uint32_t SENSOR_PERIOD_MS   = 200;    // 5 lần/giây
static const uint32_t STABLE_CHANGE_MS   = 2000;   // Đổi trạng thái ổn định 2 s -> bật change_mask
static const uint32_t FALLBACK_TIMEOUT_MS = 3000;  // Mất gói lệnh > 3 s -> LED theo cảm biến tại chỗ

static const int SERVO_OPEN_DEG  = 90;
static const int SERVO_CLOSE_DEG = 0;
