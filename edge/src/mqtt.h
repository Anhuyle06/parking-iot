#pragma once

/**
 * @brief Khởi tạo kết nối MQTT tới CoreIOT (app.coreiot.io:1883).
 */
void mqttInit();

/**
 * @brief Duy trì kết nối MQTT, xử lý gói nhận và gửi telemetry định kỳ / sự kiện.
 */
void mqttLoop();

/**
 * @brief Gửi một chuỗi JSON bất kỳ lên topic v1/devices/me/telemetry.
 */
bool mqttPublishTelemetry(const char *jsonPayload);
