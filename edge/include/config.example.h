#pragma once
#include <stdint.h>

// Copy file này thành edge/include/config.h và điền thông tin thực tế (config.h đã nằm trong .gitignore).

static const char *WIFI_SSID = "YOUR_WIFI_SSID";
static const char *WIFI_PASS = "YOUR_WIFI_PASSWORD";

// Địa chỉ MAC WiFi STA của Node YoloUNO (in từ Serial của Node)
static const uint8_t NODE_MAC[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

// CoreIOT (ThingsBoard MQTT API)
static const char *COREIOT_HOST  = "app.coreiot.io";
static const uint16_t COREIOT_PORT = 1883;
static const char *COREIOT_TOKEN = "YOUR_DEVICE_ACCESS_TOKEN";

// Phiên bản firmware (cập nhật khi demo OTA T14: v1.0.0 -> v1.0.1)
static const char *FW_TITLE   = "parking-edge";
static const char *FW_VERSION = "1.0.0";

// Bộ nhớ cho TFLite Micro tensor arena trong PSRAM (điều chỉnh theo kích thước model thực tế)
static const size_t TENSOR_ARENA_SIZE = 128 * 1024;
