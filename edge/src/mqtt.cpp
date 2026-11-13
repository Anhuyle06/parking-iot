/**
 * @file mqtt.cpp
 * @brief Kết nối CoreIOT (ThingsBoard MQTT API): telemetry, sự kiện, shared attributes & RPC – Người 2.
 */

#include "mqtt.h"

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "protocol.h"
#include "model_data.h"
#include "logic.h"
#include "ota.h"

#if __has_include("config.h")
#include "config.h"
#else
#include "config.example.h"
#endif

static WiFiClient wifiClient;
static PubSubClient mqttClient(wifiClient);
static uint32_t lastReconnectMs = 0;
static uint32_t lastTelemetryMs = 0;
static bool pendingReboot = false;

static void publishClientAttributes() {
  JsonDocument doc;
  doc["fw_version"] = FW_VERSION;
  doc["model_version"] = MODEL_VERSION;
  doc["total_slots"] = N_SLOTS;
  String out;
  serializeJson(doc, out);
  mqttClient.publish("v1/devices/me/attributes", out.c_str());
}

static void requestSharedAttributes() {
  mqttClient.publish(
    "v1/devices/me/attributes/request/1",
    "{\"sharedKeys\":\"roi_json,conf_min,wrong_confirm_s,nopark_confirm_s,fw_title,fw_version,fw_checksum,fw_checksum_algorithm,fw_size\"}"
  );
}

static void applySharedAttributes(JsonObjectConst shared) {
  if (shared["roi_json"].is<const char *>()) {
    logicUpdateRoiFromJson(shared["roi_json"].as<const char *>());
  } else if (shared["roi_json"].is<JsonObjectConst>()) {
    String roiStr;
    serializeJson(shared["roi_json"], roiStr);
    logicUpdateRoiFromJson(roiStr.c_str());
  }

  float confMin = shared["conf_min"] | 0.0f;
  uint32_t wrongS = shared["wrong_confirm_s"] | 0;
  uint32_t noparkS = shared["nopark_confirm_s"] | 0;
  if (confMin > 0.0f || wrongS > 0 || noparkS > 0) {
    logicUpdateThresholds(confMin, wrongS, noparkS);
  }

  otaHandleAttributes(shared);
}

static void handleRpc(const String &reqId, JsonDocument &doc) {
  const char *method = doc["method"] | "";
  JsonVariantConst params = doc["params"];
  String respTopic = String("v1/devices/me/rpc/response/") + reqId;
  String respPayload = "{\"ok\":true}";

  if (strcmp(method, "setBarrier") == 0) {
    const char *state = params["state"] | "open";
    bool open = (strcmp(state, "open") == 0);
    logicSetBarrier(open);
    respPayload = String("{\"ok\":true,\"state\":\"") + (open ? "open" : "close") + "\"}";
  } else if (strcmp(method, "getBarrier") == 0) {
    respPayload = logicGetBarrier() ? "true" : "false";
  } else if (strcmp(method, "setSlotLed") == 0) {
    int slot = params["slot"] | 0;
    const char *color = params["color"] | "auto";
    logicSetSlotLedOverride(slot, color);
  } else if (strcmp(method, "captureNow") == 0) {
    logicRequestImmediateCapture();
    respPayload = logicBuildTelemetryJson();
  } else if (strcmp(method, "ackViolation") == 0) {
    int slot = params["slot"] | 0;
    logicAckViolation(slot);
  } else if (strcmp(method, "setMode") == 0) {
    const char *mode = params["mode"] | "auto";
    logicSetAutoMode(strcmp(mode, "auto") == 0);
  } else if (strcmp(method, "getStatus") == 0) {
    respPayload = logicBuildTelemetryJson();
  } else if (strcmp(method, "reboot") == 0) {
    pendingReboot = true;
  } else {
    respPayload = "{\"ok\":false,\"error\":\"unknown_method\"}";
  }

  mqttClient.publish(respTopic.c_str(), respPayload.c_str());
}

static void onMqttMessage(char *topic, byte *payload, unsigned int length) {
  String topicStr(topic);
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err != DeserializationError::Ok) {
    Serial.printf("[MQTT] JSON parse error on %s: %s\n", topic, err.c_str());
    return;
  }

  if (topicStr.startsWith("v1/devices/me/rpc/request/")) {
    String reqId = topicStr.substring(strlen("v1/devices/me/rpc/request/"));
    handleRpc(reqId, doc);
  } else if (topicStr == "v1/devices/me/attributes") {
    applySharedAttributes(doc.as<JsonObjectConst>());
  } else if (topicStr.startsWith("v1/devices/me/attributes/response/")) {
    if (doc["shared"].is<JsonObjectConst>()) {
      applySharedAttributes(doc["shared"].as<JsonObjectConst>());
    }
  }
}

static void ensureConnected() {
  if (mqttClient.connected() || WiFi.status() != WL_CONNECTED) {
    return;
  }
  uint32_t now = millis();
  if (now - lastReconnectMs < 3000) {
    return;
  }
  lastReconnectMs = now;

  String clientId = String("parking-edge-") + String(static_cast<uint32_t>(ESP.getEfuseMac()), HEX);
  Serial.printf("[MQTT] Dang ket noi CoreIOT (%s:%u)...\n", COREIOT_HOST, COREIOT_PORT);
  if (mqttClient.connect(clientId.c_str(), COREIOT_TOKEN, nullptr)) {
    Serial.println("[MQTT] Da ket noi CoreIOT!");
    mqttClient.subscribe("v1/devices/me/rpc/request/+");
    mqttClient.subscribe("v1/devices/me/attributes");
    mqttClient.subscribe("v1/devices/me/attributes/response/+");
    publishClientAttributes();
    requestSharedAttributes();
  } else {
    Serial.printf("[MQTT] Ket noi that bai, rc=%d\n", mqttClient.state());
  }
}

void mqttInit() {
  mqttClient.setServer(COREIOT_HOST, COREIOT_PORT);
  mqttClient.setBufferSize(2048);
  mqttClient.setCallback(onMqttMessage);
}

bool mqttPublishTelemetry(const char *jsonPayload) {
  if (!mqttClient.connected() || jsonPayload == nullptr) {
    return false;
  }
  return mqttClient.publish("v1/devices/me/telemetry", jsonPayload);
}

void mqttLoop() {
  ensureConnected();
  mqttClient.loop();

  if (pendingReboot) {
    delay(200);
    ESP.restart();
  }

  // Gửi ngay các sự kiện đã chống dội
  String evJson;
  while (logicPopEventJson(&evJson)) {
    mqttPublishTelemetry(evJson.c_str());
  }

  // Gửi telemetry định kỳ mỗi 5 giây
  uint32_t now = millis();
  if (mqttClient.connected() && (now - lastTelemetryMs >= 5000)) {
    lastTelemetryMs = now;
    String telemetry = logicBuildTelemetryJson();
    mqttPublishTelemetry(telemetry.c_str());
  }
}
