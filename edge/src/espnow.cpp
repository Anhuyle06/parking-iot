/**
 * @file espnow.cpp
 * @brief Module ESP-NOW 2 chiều phía thiết bị biên – Người 1 (T06).
 *
 * Lưu ý: Callback nhận ESP-NOW không xử lý nặng, chỉ kiểm tra kích thước và đẩy vào queue FreeRTOS.
 */

#include "espnow.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_idf_version.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

static QueueHandle_t sensorQueue = nullptr;
static uint8_t targetNodeMac[6] = {0};

static volatile uint32_t rxLastSeq = 0;
static volatile uint32_t rxReceived = 0;
static volatile uint32_t rxMissed = 0;
static volatile uint32_t lastRxMs = 0;
static volatile uint32_t txFailCount = 0;

#if ESP_IDF_VERSION_MAJOR >= 5
static void onEspNowRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  (void)info;
#else
static void onEspNowRecv(const uint8_t *mac, const uint8_t *data, int len) {
  (void)mac;
#endif
  if (len != sizeof(node_pkt_t) || sensorQueue == nullptr) {
    return;
  }
  node_pkt_t pkt;
  memcpy(&pkt, data, sizeof(pkt));
  if (pkt.msg_type != MSG_SENSOR) {
    return;
  }

  if (rxLastSeq > 0 && pkt.seq > rxLastSeq + 1) {
    rxMissed += (pkt.seq - rxLastSeq - 1);
  }
  rxLastSeq = pkt.seq;
  rxReceived++;
  lastRxMs = millis();

  BaseType_t woken = pdFALSE;
  xQueueSendFromISR(sensorQueue, &pkt, &woken);
  if (woken == pdTRUE) {
    portYIELD_FROM_ISR();
  }
}

static void onEspNowSent(const uint8_t *mac, esp_now_send_status_t status) {
  (void)mac;
  if (status != ESP_NOW_SEND_SUCCESS) {
    txFailCount++;
  }
}

bool espnowInit(const uint8_t *nodeMac) {
  if (sensorQueue == nullptr) {
    sensorQueue = xQueueCreate(8, sizeof(node_pkt_t));
  }
  memcpy(targetNodeMac, nodeMac, 6);

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESPNOW] esp_now_init failed!");
    return false;
  }
  esp_now_register_recv_cb(onEspNowRecv);
  esp_now_register_send_cb(onEspNowSent);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, targetNodeMac, 6);
  peer.channel = 0; // Dùng kênh WiFi hiện tại của router
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("[ESPNOW] esp_now_add_peer failed!");
    return false;
  }
  return true;
}

bool espnowPopSensor(node_pkt_t *out) {
  if (sensorQueue == nullptr || out == nullptr) {
    return false;
  }
  return xQueueReceive(sensorQueue, out, 0) == pdTRUE;
}

bool espnowSendCmd(const edge_cmd_t *cmd) {
  if (cmd == nullptr) {
    return false;
  }
  esp_err_t err = esp_now_send(targetNodeMac, reinterpret_cast<const uint8_t *>(cmd), sizeof(edge_cmd_t));
  return err == ESP_OK;
}

float espnowGetPktLoss() {
  uint32_t total = rxReceived + rxMissed;
  if (total == 0) {
    return 0.0f;
  }
  return static_cast<float>(rxMissed) / static_cast<float>(total);
}

uint32_t espnowGetLastRxMs() {
  return lastRxMs;
}
