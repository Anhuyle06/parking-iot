/**
 * @file logic.cpp
 * @brief Hậu xử lý TinyML, kết hợp cảm biến, chống dội sự kiện & tổng hợp lệnh Node – Người 2 (T04).
 */

#include "logic.h"

#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

#include "espnow.h"

static SemaphoreHandle_t stateMutex = nullptr;
static QueueHandle_t eventQueue = nullptr;

static roi_t slotRois[N_SLOTS] = {
  {10,  20, 70, 95},
  {85,  20, 70, 95},
  {165, 20, 70, 95},
  {240, 20, 70, 95}
};
static roi_t noparkRoi = {10, 135, 300, 90};

static float cfgConfMin = 0.7f;
static uint32_t cfgWrongConfirmS = 10;
static uint32_t cfgNoparkConfirmS = 10;

static node_pkt_t latestSensor = {};
static bool hasSensor = false;

static uint8_t slotState[N_SLOTS] = {ST_EMPTY, ST_EMPTY, ST_EMPTY, ST_EMPTY};
static float slotConf[N_SLOTS] = {0};
static uint8_t ledOverride[N_SLOTS] = {LED_OVR_AUTO};
static bool slotAcked[N_SLOTS] = {false};
static uint32_t wrongCandidateStartMs[N_SLOTS] = {0};
static uint32_t violationActiveStartMs[N_SLOTS] = {0};

static bool noparkActive = false;
static uint32_t noparkCandidateStartMs = 0;

static bool barrierOpen = true;
static bool autoMode = true;
static volatile bool captureRequested = false;

static uint32_t lastFrameMs = 0;
static uint32_t lastInferMs = 0;

static const float ULTRASONIC_OCCUPIED_CM = 15.0f;

static void pushEvent(const String &json) {
  if (eventQueue == nullptr) {
    return;
  }
  char buf[192];
  strncpy(buf, json.c_str(), sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  xQueueSend(eventQueue, buf, 0);
}

static uint8_t parseLedOverrideColor(const char *color) {
  if (color == nullptr || strcmp(color, "auto") == 0) return LED_OVR_AUTO;
  if (strcmp(color, "green") == 0)  return LED_OVR_GREEN;
  if (strcmp(color, "red") == 0)    return LED_OVR_RED;
  if (strcmp(color, "yellow") == 0) return LED_OVR_YELLOW;
  if (strcmp(color, "off") == 0)    return LED_OVR_OFF;
  return LED_OVR_AUTO;
}

static bool parseRoiField(JsonVariantConst v, roi_t *dst) {
  if (!v.is<JsonObjectConst>() || dst == nullptr) {
    return false;
  }
  int x = v["x"] | -1;
  int y = v["y"] | -1;
  int w = v["w"] | -1;
  int h = v["h"] | -1;
  if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > CAM_FRAME_W || y + h > CAM_FRAME_H) {
    return false;
  }
  dst->x = x;
  dst->y = y;
  dst->w = w;
  dst->h = h;
  return true;
}

void logicInit() {
  if (stateMutex == nullptr) {
    stateMutex = xSemaphoreCreateMutex();
  }
  if (eventQueue == nullptr) {
    eventQueue = xQueueCreate(8, 192);
  }
}

void logicOnSensorPacket(const node_pkt_t &pkt) {
  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    latestSensor = pkt;
    hasSensor = true;
    if (pkt.change_mask != 0) {
      captureRequested = true;
    }
    xSemaphoreGive(stateMutex);
  }
}

void logicUpdateVision(const vision_result_t &res) {
  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    return;
  }

  uint32_t now = millis();
  lastFrameMs = res.frameMs;
  lastInferMs = res.inferMs;

  uint8_t prevState[N_SLOTS];
  memcpy(prevState, slotState, sizeof(prevState));

  for (int i = 0; i < N_SLOTS; ++i) {
    int cls = res.rawSlotClass[i];
    float conf = res.slotConf[i];
    slotConf[i] = conf;

    bool sensorOccupied = hasSensor &&
      (((latestSensor.ir_mask >> i) & 0x01) ||
       (latestSensor.dist_cm[i] > 0.0f && latestSensor.dist_cm[i] < ULTRASONIC_OCCUPIED_CM));

    uint8_t candidate = ST_UNKNOWN;
    if (cls < 0) {
      // Model chưa sẵn sàng -> fallback cảm biến
      candidate = sensorOccupied ? ST_OK : ST_EMPTY;
    } else if (conf < cfgConfMin) {
      candidate = ST_UNKNOWN;
    } else if (cls == 0 && sensorOccupied) {
      // Camera nói trống nhưng cảm biến thấy vật < 15 cm -> ST_UNKNOWN
      candidate = ST_UNKNOWN;
    } else if (cls == 0) {
      candidate = ST_EMPTY;
    } else if (cls == 1) {
      candidate = ST_OK;
    } else {
      candidate = ST_WRONG;
    }

    // Xác nhận theo thời gian trước khi chuyển sang ST_WRONG
    if (candidate == ST_WRONG) {
      if (wrongCandidateStartMs[i] == 0) {
        wrongCandidateStartMs[i] = now;
      }
      if (now - wrongCandidateStartMs[i] >= cfgWrongConfirmS * 1000UL) {
        slotState[i] = ST_WRONG;
      }
    } else {
      wrongCandidateStartMs[i] = 0;
      if (candidate != ST_UNKNOWN) {
        slotState[i] = candidate;
      } else if (cls < 0 || conf < cfgConfMin) {
        slotState[i] = ST_UNKNOWN;
      }
    }

    // Phát hiện sự kiện chuyển trạng thái đã chống dội
    if (prevState[i] != ST_OK && slotState[i] == ST_OK) {
      JsonDocument ev;
      ev["event"] = "car_in";
      ev["event_slot"] = i + 1;
      String out;
      serializeJson(ev, out);
      pushEvent(out);
    }

    if (prevState[i] != ST_WRONG && slotState[i] == ST_WRONG) {
      violationActiveStartMs[i] = now;
      slotAcked[i] = false;
      bool adjacentWrong = (i > 0 && slotState[i - 1] == ST_WRONG) ||
                           (i + 1 < N_SLOTS && slotState[i + 1] == ST_WRONG);
      JsonDocument ev;
      ev["event"] = "wrong_parking";
      ev["event_slot"] = i + 1;
      ev["violation_type"] = adjacentWrong ? "de_vach" : "ngoai_o";
      String out;
      serializeJson(ev, out);
      pushEvent(out);
    } else if (prevState[i] == ST_WRONG && slotState[i] != ST_WRONG) {
      uint32_t durS = (violationActiveStartMs[i] > 0) ? (now - violationActiveStartMs[i]) / 1000UL : 0;
      violationActiveStartMs[i] = 0;
      slotAcked[i] = false;
      JsonDocument ev;
      ev["event"] = "violation_cleared";
      ev["event_slot"] = i + 1;
      ev["duration_s"] = durS;
      String out;
      serializeJson(ev, out);
      pushEvent(out);
    }
  }

  // Vùng cấm đỗ: kết quả khác "trống" (cls == 1 hoặc 2) và conf >= conf_min kéo dài >= nopark_confirm_s
  bool noparkOccupied = (res.rawNoparkClass > 0 && res.noparkConf >= cfgConfMin);
  if (noparkOccupied) {
    if (noparkCandidateStartMs == 0) {
      noparkCandidateStartMs = now;
    }
    if (!noparkActive && (now - noparkCandidateStartMs >= cfgNoparkConfirmS * 1000UL)) {
      noparkActive = true;
      JsonDocument ev;
      ev["event"] = "wrong_parking";
      ev["event_slot"] = 0;
      ev["violation_type"] = "vung_cam";
      String out;
      serializeJson(ev, out);
      pushEvent(out);
    }
  } else {
    noparkCandidateStartMs = 0;
    noparkActive = false;
  }

  // Chế độ auto: bãi đầy -> đóng barrier
  if (autoMode) {
    int freeCount = 0;
    for (int i = 0; i < N_SLOTS; ++i) {
      if (slotState[i] == ST_EMPTY) {
        freeCount++;
      }
    }
    barrierOpen = (freeCount > 0);
  }

  xSemaphoreGive(stateMutex);
}

void logicGetRois(roi_t outSlots[N_SLOTS], roi_t *outNopark) {
  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    for (int i = 0; i < N_SLOTS; ++i) {
      outSlots[i] = slotRois[i];
    }
    if (outNopark != nullptr) {
      *outNopark = noparkRoi;
    }
    xSemaphoreGive(stateMutex);
  }
}

bool logicUpdateRoiFromJson(const char *jsonStr) {
  if (jsonStr == nullptr) {
    return false;
  }
  JsonDocument doc;
  if (deserializeJson(doc, jsonStr) != DeserializationError::Ok) {
    return false;
  }

  roi_t nextSlots[N_SLOTS];
  roi_t nextNopark = noparkRoi;
  for (int i = 0; i < N_SLOTS; ++i) {
    String key = String("slot") + String(i + 1);
    nextSlots[i] = slotRois[i];
    if (doc[key].is<JsonObjectConst>() && !parseRoiField(doc[key], &nextSlots[i])) {
      return false;
    }
  }
  if (doc["nopark"].is<JsonObjectConst>() && !parseRoiField(doc["nopark"], &nextNopark)) {
    return false;
  }

  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    for (int i = 0; i < N_SLOTS; ++i) {
      slotRois[i] = nextSlots[i];
    }
    noparkRoi = nextNopark;
    xSemaphoreGive(stateMutex);
  }
  return true;
}

String logicGetRoiJson() {
  JsonDocument doc;
  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    for (int i = 0; i < N_SLOTS; ++i) {
      JsonObject obj = doc[String("slot") + String(i + 1)].to<JsonObject>();
      obj["x"] = slotRois[i].x;
      obj["y"] = slotRois[i].y;
      obj["w"] = slotRois[i].w;
      obj["h"] = slotRois[i].h;
    }
    JsonObject np = doc["nopark"].to<JsonObject>();
    np["x"] = noparkRoi.x;
    np["y"] = noparkRoi.y;
    np["w"] = noparkRoi.w;
    np["h"] = noparkRoi.h;
    xSemaphoreGive(stateMutex);
  }
  String out;
  serializeJson(doc, out);
  return out;
}

void logicUpdateThresholds(float confMin, uint32_t wrongConfirmS, uint32_t noparkConfirmS) {
  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    if (confMin > 0.0f && confMin <= 1.0f) cfgConfMin = confMin;
    if (wrongConfirmS > 0) cfgWrongConfirmS = wrongConfirmS;
    if (noparkConfirmS > 0) cfgNoparkConfirmS = noparkConfirmS;
    xSemaphoreGive(stateMutex);
  }
}

String logicBuildTelemetryJson() {
  JsonDocument doc;
  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    int freeCount = 0;
    for (int i = 0; i < N_SLOTS; ++i) {
      doc[String("slot") + String(i + 1)] = slotState[i];
      doc[String("slot") + String(i + 1) + "_conf"] = serialized(String(slotConf[i], 2));
      if (slotState[i] == ST_EMPTY) freeCount++;
    }
    // Gộp 2 ô liền kề cùng ST_WRONG thành 1 vi phạm đè vạch khi đếm tổng vi phạm
    int violations = 0;
    for (int i = 0; i < N_SLOTS; ++i) {
      if (slotState[i] == ST_WRONG) {
        if (i == 0 || slotState[i - 1] != ST_WRONG) {
          violations++;
        }
      }
    }
    if (noparkActive) {
      violations++;
    }

    doc["free_slots"] = freeCount;
    doc["total_slots"] = N_SLOTS;
    doc["violations"] = violations;
    doc["nopark_zone"] = noparkActive ? 1 : 0;
    doc["barrier"] = barrierOpen ? 1 : 0;
    doc["mode"] = autoMode ? "auto" : "manual";
    doc["infer_ms"] = lastInferMs;
    doc["frame_ms"] = lastFrameMs;
    doc["pkt_loss"] = serialized(String(espnowGetPktLoss(), 3));
    doc["heap_free"] = ESP.getFreeHeap();
    xSemaphoreGive(stateMutex);
  }
  String out;
  serializeJson(doc, out);
  return out;
}

bool logicPopEventJson(String *outJson) {
  if (eventQueue == nullptr || outJson == nullptr) {
    return false;
  }
  char buf[192];
  if (xQueueReceive(eventQueue, buf, 0) == pdTRUE) {
    *outJson = String(buf);
    return true;
  }
  return false;
}

void logicBuildEdgeCmd(edge_cmd_t *outCmd) {
  if (outCmd == nullptr) {
    return;
  }
  memset(outCmd, 0, sizeof(*outCmd));
  outCmd->msg_type = MSG_CMD;

  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    uint8_t freeCount = 0;
    bool unackedViolation = noparkActive;
    for (int i = 0; i < N_SLOTS; ++i) {
      outCmd->slot_state[i] = slotState[i];
      outCmd->led_override[i] = ledOverride[i];
      if (slotState[i] == ST_EMPTY) {
        freeCount++;
      }
      if (slotState[i] == ST_WRONG && !slotAcked[i]) {
        unackedViolation = true;
      }
    }
    outCmd->free_slots = freeCount;
    outCmd->barrier = barrierOpen ? 1 : 0;
    outCmd->buzzer = unackedViolation ? 1 : 0;
    xSemaphoreGive(stateMutex);
  }
}

void logicSetBarrier(bool open) {
  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    barrierOpen = open;
    xSemaphoreGive(stateMutex);
  }
}

bool logicGetBarrier() {
  bool val = true;
  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    val = barrierOpen;
    xSemaphoreGive(stateMutex);
  }
  return val;
}

void logicSetAutoMode(bool isAuto) {
  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    autoMode = isAuto;
    xSemaphoreGive(stateMutex);
  }
}

bool logicGetAutoMode() {
  bool val = true;
  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    val = autoMode;
    xSemaphoreGive(stateMutex);
  }
  return val;
}

void logicSetSlotLedOverride(int slot1Based, const char *color) {
  if (slot1Based < 1 || slot1Based > N_SLOTS) {
    return;
  }
  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    ledOverride[slot1Based - 1] = parseLedOverrideColor(color);
    xSemaphoreGive(stateMutex);
  }
}

void logicAckViolation(int slot1Based) {
  if (slot1Based < 1 || slot1Based > N_SLOTS) {
    return;
  }
  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    slotAcked[slot1Based - 1] = true;
    xSemaphoreGive(stateMutex);
  }
}

void logicAckAllViolations() {
  if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    for (int i = 0; i < N_SLOTS; ++i) {
      slotAcked[i] = true;
    }
    xSemaphoreGive(stateMutex);
  }
}

void logicRequestImmediateCapture() {
  captureRequested = true;
}

bool logicConsumeCaptureRequest() {
  bool req = captureRequested;
  captureRequested = false;
  return req;
}
