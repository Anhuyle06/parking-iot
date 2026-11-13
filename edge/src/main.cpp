/**
 * @file main.cpp
 * @brief Điểm vào chính của Firmware Thiết bị biên (ESP32-S3 + Camera + TinyML) – Người 2 giữ.
 *
 * Điều phối các module:
 *  - Người 1: camera, web, espnow
 *  - Người 2: ai, logic, mqtt, main
 *  - Người 3: ota
 */

#include <Arduino.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "protocol.h"
#include "camera.h"
#include "web.h"
#include "espnow.h"
#include "ai.h"
#include "logic.h"
#include "mqtt.h"
#include "ota.h"

#if __has_include("config.h")
#include "config.h"
#else
#include "config.example.h"
#endif

static uint8_t roiBuffer[ROI_INPUT_W * ROI_INPUT_H];

static void runVisionCycle() {
  uint32_t t0 = millis();
  camera_fb_t *fb = camCapture();
  uint32_t tCap = millis();

  if (fb == nullptr) {
    Serial.println("[Vision] camCapture failed!");
    return;
  }

  roi_t slots[N_SLOTS];
  roi_t nopark;
  logicGetRois(slots, &nopark);

  vision_result_t res = {};
  res.frameMs = tCap - t0;

  uint32_t tInfer0 = millis();
  for (int i = 0; i < N_SLOTS; ++i) {
    cropResize(fb, slots[i], roiBuffer);
    res.rawSlotClass[i] = classifySlot(roiBuffer, &res.slotConf[i]);
  }
  cropResize(fb, nopark, roiBuffer);
  res.rawNoparkClass = classifySlot(roiBuffer, &res.noparkConf);
  res.inferMs = millis() - tInfer0;

  camReturn(fb);
  logicUpdateVision(res);
}

static void visionTask(void *param) {
  (void)param;
  uint32_t lastPeriodicMs = 0;

  for (;;) {
    // Đọc hết các gói cảm biến mới nhất từ queue ESP-NOW
    node_pkt_t pkt;
    while (espnowPopSensor(&pkt)) {
      logicOnSensorPacket(pkt);
    }

    uint32_t now = millis();
    if (logicConsumeCaptureRequest() || (now - lastPeriodicMs >= 5000)) {
      lastPeriodicMs = now;
      if (!otaIsUpdating()) {
        runVisionCycle();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

static void mqttTask(void *param) {
  (void)param;
  for (;;) {
    mqttLoop();
    otaLoop();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

static void cmdTask(void *param) {
  (void)param;
  for (;;) {
    edge_cmd_t cmd;
    logicBuildEdgeCmd(&cmd);
    espnowSendCmd(&cmd);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("[Edge] Dang ket noi WiFi");
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  Serial.printf("[Edge] IP: %s | MAC: %s | Channel: %d | PSRAM free: %u\n",
                WiFi.localIP().toString().c_str(),
                WiFi.macAddress().c_str(),
                WiFi.channel(),
                static_cast<unsigned>(ESP.getFreePsram()));

  logicInit();
  camInit();
  espnowInit(NODE_MAC);
  aiInit();
  webInit();
  mqttInit();
  otaInit();

  // Ghim task lên các core của ESP32-S3 để tránh xung đột với WiFi/WebServer
  xTaskCreatePinnedToCore(visionTask, "visionTask", 8192, nullptr, 2, nullptr, 1);
  xTaskCreatePinnedToCore(mqttTask,   "mqttTask",   8192, nullptr, 2, nullptr, 0);
  xTaskCreatePinnedToCore(cmdTask,    "cmdTask",    4096, nullptr, 2, nullptr, 0);
}

void loop() {
  webLoop();
  delay(10);
}
