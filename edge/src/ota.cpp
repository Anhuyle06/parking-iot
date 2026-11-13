/**
 * @file ota.cpp
 * @brief Cập nhật firmware OTA qua CoreIOT cho thiết bị biên – Người 3 (T14).
 *
 * Lưu ý kiến trúc (Mục 6.4):
 *  - Có thể dùng thư viện ThingsBoard SDK (OTA_Firmware_Update + Espressif_Updater),
 *    hoặc tự tải theo chunk qua MQTT (`v2/fw/request/0/chunk/<idx>`) / HTTP API của CoreIOT.
 *  - Báo trạng thái qua telemetry `fw_state`:
 *    DOWNLOADING -> DOWNLOADED -> VERIFIED -> UPDATING -> UPDATED / FAILED.
 *  - Bảng phân vùng phải có 2 vùng app OTA đủ lớn (xem partitions_16MB_ota.csv).
 */

#include "ota.h"

#include <Arduino.h>
#include <Update.h>
#include "mqtt.h"

#if __has_include("config.h")
#include "config.h"
#else
#include "config.example.h"
#endif

enum OtaStage {
  OTA_IDLE = 0,
  OTA_PENDING,
  OTA_RUNNING
};

static OtaStage stage = OTA_IDLE;
static String targetTitle;
static String targetVersion;
static String targetChecksum;
static String targetChecksumAlg;
static size_t targetSize = 0;

static void reportFwState(const char *state, const char *errMsg = nullptr) {
  JsonDocument doc;
  doc["fw_state"] = state;
  if (errMsg != nullptr) {
    doc["fw_error"] = errMsg;
  }
  String payload;
  serializeJson(doc, payload);
  mqttPublishTelemetry(payload.c_str());
  Serial.printf("[OTA] fw_state=%s %s\n", state, errMsg ? errMsg : "");
}

void otaInit() {
  stage = OTA_IDLE;
}

void otaHandleAttributes(JsonObjectConst shared) {
  if (!shared["fw_title"].is<const char *>() || !shared["fw_version"].is<const char *>()) {
    return;
  }
  String title = shared["fw_title"].as<String>();
  String version = shared["fw_version"].as<String>();

  if (title != FW_TITLE || version == FW_VERSION) {
    return; // Không phải firmware cho thiết bị này hoặc đã ở đúng phiên bản
  }

  targetTitle = title;
  targetVersion = version;
  targetChecksum = shared["fw_checksum"] | "";
  targetChecksumAlg = shared["fw_checksum_algorithm"] | "SHA256";
  targetSize = shared["fw_size"] | 0;

  Serial.printf("[OTA] Phat hien ban moi: %s v%s (%u bytes, %s)\n",
                targetTitle.c_str(), targetVersion.c_str(),
                static_cast<unsigned>(targetSize), targetChecksumAlg.c_str());
  stage = OTA_PENDING;
}

void otaLoop() {
  if (stage != OTA_PENDING) {
    return;
  }
  stage = OTA_RUNNING;
  reportFwState("DOWNLOADING");

  // TODO (Người 3 - T14):
  // 1. Kết nối ThingsBoard OTA SDK (OTA_Firmware_Update + Espressif_Updater) hoặc request
  //    MQTT topic `v2/fw/request/<reqId>/chunk/<chunkIdx>` với kích thước chunk (vd. 4096 bytes).
  // 2. Gọi Update.begin(targetSize) -> Update.write(...) -> kiểm tra checksum (SHA256/MD5).
  //    Nếu sai checksum -> Update.abort(); reportFwState("FAILED", "checksum mismatch");
  // 3. Nếu hợp lệ -> reportFwState("DOWNLOADED") -> reportFwState("VERIFIED") ->
  //    reportFwState("UPDATING") -> Update.end(true) -> reportFwState("UPDATED") -> ESP.restart().

  Serial.println("[OTA] Skeleton chua gan transport tai chunk; hay hoan thien trong Tuan 12-14.");
  reportFwState("FAILED", "OTA chunk downloader not wired yet");
  stage = OTA_IDLE;
}

bool otaIsUpdating() {
  return stage == OTA_RUNNING;
}
