/**
 * @file web.cpp
 * @brief Webserver hiệu chỉnh vùng ô, chụp ảnh thu thập dữ liệu & điều khiển tại chỗ – Người 1 (T07).
 */

#include "web.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <Preferences.h>

#include "camera.h"
#include "logic.h"

static WebServer server(80);
static Preferences prefs;

static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="vi">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width,initial-scale=1" />
  <title>Parking Edge – ROI & Status</title>
  <style>
    body { font-family: system-ui, sans-serif; max-width: 820px; margin: 16px auto; padding: 0 12px; }
    .row { display: flex; gap: 12px; flex-wrap: wrap; align-items: flex-start; }
    canvas { border: 1px solid #444; cursor: crosshair; max-width: 100%; }
    button, select { padding: 6px 10px; margin: 4px 2px; }
    table { border-collapse: collapse; width: 100%; margin-top: 10px; }
    th, td { border: 1px solid #ccc; padding: 6px 8px; text-align: left; }
  </style>
</head>
<body>
  <h2>Thiết bị biên – Hiệu chỉnh vùng ô & Giám sát tại chỗ</h2>
  <div>
    Vùng đang vẽ:
    <select id="zoneSel">
      <option value="slot1">Ô 1 (slot1)</option>
      <option value="slot2">Ô 2 (slot2)</option>
      <option value="slot3">Ô 3 (slot3)</option>
      <option value="slot4">Ô 4 (slot4)</option>
      <option value="nopark">Vùng cấm (nopark)</option>
    </select>
    <button onclick="reloadSnap()">Tải lại ảnh</button>
    <button onclick="saveRoi()">Lưu ROI</button>
    <button onclick="setBarrier('open')">Mở Barrier</button>
    <button onclick="setBarrier('close')">Đóng Barrier</button>
    <button onclick="muteBuzzer()">Tắt còi</button>
  </div>
  <div class="row">
    <canvas id="cv" width="320" height="240"></canvas>
    <div style="flex:1; min-width:260px;">
      <table id="tbl">
        <thead><tr><th>Vùng</th><th>Trạng thái</th><th>Conf</th></tr></thead>
        <tbody></tbody>
      </table>
      <p id="meta"></p>
    </div>
  </div>
<script>
const COLORS = { slot1:'#22c55e', slot2:'#3b82f6', slot3:'#f59e0b', slot4:'#a855f7', nopark:'#ef4444' };
const cv = document.getElementById('cv'), ctx = cv.getContext('2d');
let rois = {}, bg = new Image(), dragging = false, sx = 0, sy = 0;

function draw() {
  if (bg.complete && bg.naturalWidth) ctx.drawImage(bg, 0, 0, 320, 240);
  for (const [k, r] of Object.entries(rois)) {
    if (!r) continue;
    ctx.strokeStyle = COLORS[k] || '#fff'; ctx.lineWidth = 2;
    ctx.strokeRect(r.x, r.y, r.w, r.h);
    ctx.fillStyle = COLORS[k] || '#fff';
    ctx.fillText(k, r.x + 3, Math.max(12, r.y + 12));
  }
}
function reloadSnap() { bg.onload = draw; bg.src = '/capture?t=' + Date.now(); }

cv.onmousedown = e => {
  const rect = cv.getBoundingClientRect();
  sx = Math.round((e.clientX - rect.left) * 320 / rect.width);
  sy = Math.round((e.clientY - rect.top) * 240 / rect.height);
  dragging = true;
};
cv.onmousemove = e => {
  if (!dragging) return;
  const rect = cv.getBoundingClientRect();
  const cx = Math.round((e.clientX - rect.left) * 320 / rect.width);
  const cy = Math.round((e.clientY - rect.top) * 240 / rect.height);
  const k = document.getElementById('zoneSel').value;
  rois[k] = { x: Math.min(sx, cx), y: Math.min(sy, cy), w: Math.abs(cx - sx), h: Math.abs(cy - sy) };
  draw();
};
cv.onmouseup = () => { dragging = false; };

async function saveRoi() {
  await fetch('/api/roi', { method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify(rois) });
  alert('Đã lưu ROI vào thiết bị!');
}
async function setBarrier(state) {
  await fetch('/api/barrier', { method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({state}) });
  refreshStatus();
}
async function muteBuzzer() {
  await fetch('/api/barrier', { method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({ack_all:true}) });
  refreshStatus();
}
async function refreshStatus() {
  const s = await (await fetch('/api/status')).json();
  const tb = document.querySelector('#tbl tbody');
  tb.innerHTML = [1,2,3,4].map(i =>
    `<tr><td>Ô ${i}</td><td>${s['slot'+i]}</td><td>${(s['slot'+i+'_conf']||0).toFixed(2)}</td></tr>`
  ).join('') + `<tr><td>Vùng cấm</td><td>${s.nopark_zone}</td><td>-</td></tr>`;
  document.getElementById('meta').textContent =
    `Trống: ${s.free_slots}/4 | Vi phạm: ${s.violations} | Barrier: ${s.barrier ? 'Mở':'Đóng'} | frame=${s.frame_ms}ms infer=${s.infer_ms}ms`;
}
(async () => {
  rois = await (await fetch('/roi')).json();
  reloadSnap();
  refreshStatus();
  setInterval(refreshStatus, 2000);
})();
</script>
</body>
</html>
)rawliteral";

static void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

static void handleCapture() {
  camera_fb_t *fb = camCapture();
  if (fb == nullptr) {
    server.send(503, "text/plain", "Camera capture failed");
    return;
  }
  uint8_t *jpg = nullptr;
  size_t len = 0;
  bool ok = camFrameToJpeg(fb, 80, &jpg, &len);
  camReturn(fb);

  if (!ok || jpg == nullptr) {
    server.send(500, "text/plain", "JPEG encode failed");
    return;
  }
  server.send_P(200, "image/jpeg", reinterpret_cast<const char *>(jpg), len);
  free(jpg);
}

static void handleGetRoi() {
  server.send(200, "application/json", logicGetRoiJson());
}

static void handlePostRoi() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"missing body\"}");
    return;
  }
  String body = server.arg("plain");
  if (!logicUpdateRoiFromJson(body.c_str())) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid roi json\"}");
    return;
  }
  prefs.begin("parking", false);
  prefs.putString("roi_json", body);
  prefs.end();

  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleGetStatus() {
  server.send(200, "application/json", logicBuildTelemetryJson());
}

static void handlePostBarrier() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  if (doc["state"].is<const char *>()) {
    bool open = (strcmp(doc["state"], "open") == 0);
    logicSetBarrier(open);
  }
  if (doc["ack_all"] | false) {
    logicAckAllViolations();
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

void webInit() {
  prefs.begin("parking", true);
  String savedRoi = prefs.getString("roi_json", "");
  prefs.end();
  if (!savedRoi.isEmpty()) {
    logicUpdateRoiFromJson(savedRoi.c_str());
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/roi", HTTP_GET, handleGetRoi);
  server.on("/api/roi", HTTP_POST, handlePostRoi);
  server.on("/api/status", HTTP_GET, handleGetStatus);
  server.on("/api/barrier", HTTP_POST, handlePostBarrier);
  server.begin();
  Serial.println("[Web] HTTP server started on port 80");
}

void webLoop() {
  server.handleClient();
}
