/**
 * @file main.cpp
 * @brief Firmware Node cảm biến & chấp hành trên YoloUNO (ESP32-S3) – Người 1 (T06, N03).
 *
 * Chức năng:
 *  - Đọc 4 cảm biến siêu âm HC-SR04 (trung vị 3 mẫu, đo lần lượt >= 30 ms) và 4 cảm biến IR.
 *  - Phát hiện chỗ đổi trạng thái ổn định >= 2 s để bật bit tương ứng trong change_mask.
 *  - Gửi node_pkt_t về thiết bị biên 5 lần/giây qua ESP-NOW (cùng kênh WiFi router).
 *  - Nhận edge_cmd_t qua queue FreeRTOS (không xử lý nặng trong callback ESP-NOW).
 *  - Điều khiển WS2812, còi buzzer, servo SG90; tự chuyển sang fallback tại chỗ nếu mất gói > 3 s.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_idf_version.h>
#include <algorithm>
#include <Adafruit_NeoPixel.h>
#include <ESP32Servo.h>

#include "protocol.h"

#if __has_include("config.h")
#include "config.h"
#else
#include "config.example.h"
#endif

static Adafruit_NeoPixel strip(N_SLOTS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
static Servo barrierServo;
static QueueHandle_t cmdQueue = nullptr;

// Trạng thái nhận lệnh gần nhất từ thiết bị biên
static edge_cmd_t currentCmd = {};
static uint32_t lastCmdMs = 0;

// Bộ đếm ESP-NOW phục vụ đo tỉ lệ mất gói
static uint32_t txSeq = 0;
static volatile uint32_t txOkCount = 0;
static volatile uint32_t txFailCount = 0;

// Theo dõi ổn định 2 s cho change_mask
static bool lastRawOccupied[N_SLOTS] = {false};
static bool stableOccupied[N_SLOTS] = {false};
static uint32_t changeStartMs[N_SLOTS] = {0};

/**
 * @brief Đo một lần khoảng cách HC-SR04 (cm). Trả về -1.0f nếu timeout.
 */
static float measureSingleCm(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  unsigned long duration = pulseIn(echoPin, HIGH, 25000UL); // ~4.3 m timeout
  if (duration == 0) {
    return -1.0f;
  }
  return (duration * 0.0343f) * 0.5f;
}

/**
 * @brief Đọc cảm biến siêu âm cho ô i bằng trung vị 3 mẫu.
 */
float readUltrasonic(int slot) {
  float samples[3];
  for (int k = 0; k < 3; ++k) {
    samples[k] = measureSingleCm(TRIG_PINS[slot], ECHO_PINS[slot]);
    delay(10);
  }
  // Sắp xếp 3 phần tử đơn giản để lấy trung vị
  if (samples[0] > samples[1]) std::swap(samples[0], samples[1]);
  if (samples[1] > samples[2]) std::swap(samples[1], samples[2]);
  if (samples[0] > samples[1]) std::swap(samples[0], samples[1]);

  // Nếu có 2 mẫu lỗi (-1), coi như lỗi
  if (samples[1] < 0.0f) {
    return -1.0f;
  }
  return samples[1];
}

/**
 * @brief Đọc cảm biến hồng ngoại tại ô i.
 */
bool readIR(int slot) {
  int val = digitalRead(IR_PINS[slot]);
  return IR_ACTIVE_LOW ? (val == LOW) : (val == HIGH);
}

static void onEspNowSent(const uint8_t *mac, esp_now_send_status_t status) {
  (void)mac;
  if (status == ESP_NOW_SEND_SUCCESS) {
    txOkCount++;
  } else {
    txFailCount++;
  }
}

#if ESP_IDF_VERSION_MAJOR >= 5
static void onEspNowRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  (void)info;
#else
static void onEspNowRecv(const uint8_t *mac, const uint8_t *data, int len) {
  (void)mac;
#endif
  if (len != sizeof(edge_cmd_t)) {
    return;
  }
  edge_cmd_t pkt;
  memcpy(&pkt, data, sizeof(pkt));
  if (pkt.msg_type != MSG_CMD || cmdQueue == nullptr) {
    return;
  }
  BaseType_t woken = pdFALSE;
  xQueueOverwriteFromISR(cmdQueue, &pkt, &woken);
  if (woken == pdTRUE) {
    portYIELD_FROM_ISR();
  }
}

static bool initEspNow() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("[Node] Dang ket noi WiFi de dong bo kenh ESP-NOW");
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  Serial.printf("[Node] MAC: %s, WiFi channel: %d\n",
                WiFi.macAddress().c_str(), WiFi.channel());

  if (esp_now_init() != ESP_OK) {
    Serial.println("[Node] esp_now_init failed!");
    return false;
  }
  esp_now_register_send_cb(onEspNowSent);
  esp_now_register_recv_cb(onEspNowRecv);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, EDGE_MAC, 6);
  peer.channel = 0; // Dùng kênh WiFi hiện tại
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("[Node] esp_now_add_peer failed!");
    return false;
  }
  return true;
}

static uint32_t colorForOverride(uint8_t ovr) {
  switch (ovr) {
    case LED_OVR_GREEN:  return strip.Color(0, 180, 0);
    case LED_OVR_RED:    return strip.Color(180, 0, 0);
    case LED_OVR_YELLOW: return strip.Color(180, 120, 0);
    case LED_OVR_OFF:
    default:             return 0;
  }
}

static void updateActuators(const float distCm[N_SLOTS], uint8_t irMask) {
  uint32_t now = millis();
  bool fallback = (lastCmdMs == 0) || (now - lastCmdMs > FALLBACK_TIMEOUT_MS);
  bool blinkOn = ((now / 400) % 2) == 0;

  for (int i = 0; i < N_SLOTS; ++i) {
    uint32_t color = 0;
    if (fallback) {
      bool occupied = ((irMask >> i) & 0x01) ||
                      (distCm[i] > 0.0f && distCm[i] < OCCUPIED_DIST_CM);
      color = occupied ? strip.Color(180, 0, 0) : strip.Color(0, 180, 0);
    } else if (currentCmd.led_override[i] != LED_OVR_AUTO) {
      color = colorForOverride(currentCmd.led_override[i]);
    } else {
      switch (currentCmd.slot_state[i]) {
        case ST_EMPTY:   color = strip.Color(0, 180, 0); break;
        case ST_OK:      color = strip.Color(180, 0, 0); break;
        case ST_WRONG:   color = blinkOn ? strip.Color(200, 140, 0) : 0; break;
        case ST_UNKNOWN:
        default:         color = 0; break;
      }
    }
    strip.setPixelColor(i, color);
  }
  strip.show();

  if (fallback) {
    digitalWrite(BUZZER_PIN, LOW);
  } else {
    digitalWrite(BUZZER_PIN, currentCmd.buzzer ? HIGH : LOW);
    barrierServo.write(currentCmd.barrier ? SERVO_OPEN_DEG : SERVO_CLOSE_DEG);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  for (int i = 0; i < N_SLOTS; ++i) {
    pinMode(TRIG_PINS[i], OUTPUT);
    pinMode(ECHO_PINS[i], INPUT);
    pinMode(IR_PINS[i], INPUT);
  }
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  strip.begin();
  strip.setBrightness(80);
  strip.show();

  barrierServo.attach(SERVO_PIN);
  barrierServo.write(SERVO_OPEN_DEG);

  cmdQueue = xQueueCreate(1, sizeof(edge_cmd_t));
  initEspNow();

#ifdef ENABLE_LVGL_GATE
  // TODO (N03): Khởi tạo màn hình LVGL hiển thị "Còn X/4 chỗ" + sơ đồ ô vi phạm
#endif
}

void loop() {
  static uint32_t lastSendMs = 0;
  static float distCm[N_SLOTS] = {0};
  static uint8_t irMask = 0;

  edge_cmd_t incoming;
  if (cmdQueue != nullptr && xQueueReceive(cmdQueue, &incoming, 0) == pdTRUE) {
    currentCmd = incoming;
    lastCmdMs = millis();
  }

  uint32_t now = millis();
  if (now - lastSendMs >= SENSOR_PERIOD_MS) {
    lastSendMs = now;

    node_pkt_t pkt = {};
    pkt.msg_type = MSG_SENSOR;
    pkt.node_id = NODE_ID;
    pkt.seq = ++txSeq;

    irMask = 0;
    uint8_t changeMask = 0;

    for (int i = 0; i < N_SLOTS; ++i) {
      distCm[i] = readUltrasonic(i);
      bool ir = readIR(i);
      if (ir) {
        irMask |= (1U << i);
      }

      bool rawOccupied = ir || (distCm[i] > 0.0f && distCm[i] < OCCUPIED_DIST_CM);
      if (rawOccupied != lastRawOccupied[i]) {
        lastRawOccupied[i] = rawOccupied;
        changeStartMs[i] = now;
      } else if (rawOccupied != stableOccupied[i] && (now - changeStartMs[i] >= STABLE_CHANGE_MS)) {
        stableOccupied[i] = rawOccupied;
        changeMask |= (1U << i);
      }

      if (i + 1 < N_SLOTS) {
        delay(SONAR_GAP_MS); // Tránh nhiễu chéo giữa các HC-SR04
      }
    }

    pkt.ir_mask = irMask;
    pkt.change_mask = changeMask;
    esp_now_send(EDGE_MAC, reinterpret_cast<const uint8_t *>(&pkt), sizeof(pkt));

    if (txSeq % 25 == 0) {
      Serial.printf("[Node] seq=%lu ok=%lu fail=%lu ir=0x%02X chg=0x%02X\n",
                    static_cast<unsigned long>(txSeq),
                    static_cast<unsigned long>(txOkCount),
                    static_cast<unsigned long>(txFailCount),
                    irMask, changeMask);
    }
  }

  updateActuators(distCm, irMask);
  delay(20);
}
