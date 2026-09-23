#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include "ActiveDevice.h"
#include "ConverterStateGlobal.h"
#include "ESPInfo.h"
#include "esp_log.h"
#include "src/service/src/WebSocketService.h"

/**
 * Server.cpp - WiFi Management and RESTful API
 *
 * Uses ESPAsyncWebServer: HTTP and WebSocket on the same port (80), fully
 * non-blocking. HTTP handlers run on the async server's internal FreeRTOS task;
 * server.handleClient() is not needed and handleServerRequests() is a no-op.
 *
 * Java equivalent: com.serial.service.RestService (route handlers)
 */

static const char* TAG_SRV = "SERVER";

// Onboard LED used for fault signalling (GPIO 2 on ESP32-WROOM-32)
#define FAULT_LED_PIN 2

/**
 * Blink the onboard LED in an SOS pattern (... --- ...) and halt.
 * Called when a fatal startup condition is detected (e.g. WiFi failure).
 * Never returns.
 */
static void haltWithSOS() {
  pinMode(FAULT_LED_PIN, OUTPUT);
  const int dot = 150;   // ms
  const int dash = 450;  // ms
  const int gap = 150;   // ms between elements
  const int word = 700;  // ms between S and O groups

  for (;;) {
    // S: three dots
    for (int i = 0; i < 3; i++) {
      digitalWrite(FAULT_LED_PIN, HIGH);
      delay(dot);
      digitalWrite(FAULT_LED_PIN, LOW);
      delay(gap);
    }
    delay(word);
    // O: three dashes
    for (int i = 0; i < 3; i++) {
      digitalWrite(FAULT_LED_PIN, HIGH);
      delay(dash);
      digitalWrite(FAULT_LED_PIN, LOW);
      delay(gap);
    }
    delay(word);
    // S: three dots
    for (int i = 0; i < 3; i++) {
      digitalWrite(FAULT_LED_PIN, HIGH);
      delay(dot);
      digitalWrite(FAULT_LED_PIN, LOW);
      delay(gap);
    }
    delay(2000);  // pause before repeating
  }
}

// HTTP Status Codes
#define HTTP_CODE_OK 200
#define HTTP_CODE_NO_CONTENT 204
#define HTTP_CODE_BAD_REQUEST 400
#define HTTP_CODE_NOT_FOUND 404
#define HTTP_CODE_CONFLICT 409
#define HTTP_CODE_INTERNAL_SERVER_ERROR 500
#define HTTP_CODE_SERVICE_UNAVAILABLE 503

// Delay between Modbus write and read-back in verified write operations (ms).
// Java equivalent: DeviceService.VERIFIED_READBACK_DELAY_MS = 50L.
static constexpr uint32_t VERIFIED_READBACK_DELAY_MS = 50;

// Absolute tolerance for verified-write read-back comparison.
// Java equivalent: DeviceService.VERIFIED_TOLERANCE = 0.01.
static constexpr double VERIFIED_TOLERANCE = 0.01;

// Global objects — AsyncWebServer handles HTTP and WebSocket on the same port
AsyncWebServer server(80);
AsyncWebSocket ws("/ws/data");
WebSocketService wsService(&ws, &converterState);
WiFiManager wm;

// ---- Shared helper ---------------------------------------------------------

/**
 * Returns true if the active device has been detected (manufacturer and device
 * strings populated). Used as the device-online guard for write endpoints.
 *
 * Java equivalent: DeviceService#isDeviceDetected
 */
static bool isDeviceDetected() {
  return (activeDevice != nullptr) && (activeDevice->getDevice() != nullptr);
}

// ---- Legacy / deprecated endpoints ----------------------------------------

/**
 * GET /voltage
 * Returns cached measured output voltage from ConverterState.
 * Kept for backward compatibility; prefer GET /api/voltage.
 */
static void handleGetVoltage(AsyncWebServerRequest* request) {
  ESP_LOGI(TAG_SRV, "Request: GET /voltage");
  if (!converterState.isDeviceOnline()) {
    request->send(HTTP_CODE_SERVICE_UNAVAILABLE, "application/json", "{\"error\":\"Device offline\"}");
    return;
  }
  double voltage = converterState.getVoltageOut();
  JsonDocument doc;
  doc["voltage"] = serialized(String(voltage, 2));
  String json;
  serializeJson(doc, json);
  request->send(HTTP_CODE_OK, "application/json", json);
}

/**
 * POST /setVoltage?v=5.0
 * Deprecated alias: sets voltage setpoint via query parameter.
 * Kept for backward compatibility; prefer PUT /api/voltage with JSON body.
 *
 * Java equivalent: replaced by RestService#setVoltage (PUT /api/voltage).
 */
static void handleSetVoltage(AsyncWebServerRequest* request) {
  if (!request->hasArg("v")) {
    ESP_LOGW(TAG_SRV, "Bad Request: Missing 'v' parameter");
    request->send(HTTP_CODE_BAD_REQUEST, "text/plain", "Bad Request: Missing 'v' parameter");
    return;
  }
  float voltageValue = request->arg("v").toFloat();
  ESP_LOGI(TAG_SRV, "Request: POST /setVoltage?v=%.2f", voltageValue);
  if (activeDevice != nullptr && activeDevice->setVoltage(voltageValue)) {
    request->send(HTTP_CODE_OK, "text/plain", "Voltage set to: " + String(voltageValue, 2) + "V");
  } else {
    ESP_LOGE(TAG_SRV, "Voltage Write Failed (Modbus Error)");
    request->send(HTTP_CODE_SERVICE_UNAVAILABLE, "text/plain", "Riden Modbus Write Failed");
  }
}

// ---- GET /api/state --------------------------------------------------------

/**
 * GET /api/state
 * Returns the full ConverterState as JSON.
 *
 * Java equivalent: RestService#getState
 */
static void handleGetState(AsyncWebServerRequest* request) {
  ESP_LOGI(TAG_SRV, "Request: GET /api/state");
  JsonDocument doc;
  doc["deviceName"] = converterState.getDeviceName();
  doc["manufacturer"] = converterState.getManufacturer();
  doc["firmwareVersion"] = converterState.getFirmwareVersion();
  doc["deviceOnline"] = converterState.isDeviceOnline();
  doc["converterTopology"] = (int)converterState.getConverterTopology();
  doc["voltageOut"] = converterState.getVoltageOut();
  doc["currentOut"] = converterState.getCurrentOut();
  doc["powerOut"] = converterState.getPowerOut();
  doc["voltageIn"] = converterState.getVoltageIn();
  doc["temperatureCelsius"] = converterState.getTemperatureCelsius();
  doc["voltageSet"] = converterState.getVoltageSet();
  doc["currentSet"] = converterState.getCurrentSet();
  doc["outputEnabled"] = converterState.isOutputEnabled();
  doc["keypadLocked"] = converterState.isKeypadLocked();
  doc["cvMode"] = converterState.isCvMode();
  doc["protectionState"] = converterState.getProtectionState();
  doc["maxVoltage"] = converterState.getMaxVoltage();
  doc["minVoltage"] = converterState.getMinVoltage();
  doc["maxCurrent"] = converterState.getMaxCurrent();
  doc["minCurrent"] = converterState.getMinCurrent();
  doc["maxPower"] = converterState.getMaxPower();
  doc["configMaxVoltage"] = converterState.getConfigMaxVoltage();
  doc["configMaxCurrent"] = converterState.getConfigMaxCurrent();
  String json;
  serializeJson(doc, json);
  request->send(HTTP_CODE_OK, "application/json", json);
}

// ---- GET /api/limits -------------------------------------------------------

/**
 * GET /api/limits
 * Returns the device capability limits (min/max voltage, current, power).
 * Clients use this to set slider bounds without fetching the full state.
 *
 * Java equivalent: RestService#getLimits
 */
static void handleGetLimits(AsyncWebServerRequest* request) {
  ESP_LOGI(TAG_SRV, "Request: GET /api/limits");
  JsonDocument doc;
  doc["manufacturer"] = converterState.getManufacturer();
  doc["deviceName"] = converterState.getDeviceName();
  doc["minVoltage"] = converterState.getMinVoltage();
  doc["maxVoltage"] = converterState.getMaxVoltage();
  doc["minCurrent"] = converterState.getMinCurrent();
  doc["maxCurrent"] = converterState.getMaxCurrent();
  doc["maxPower"] = converterState.getMaxPower();
  String json;
  serializeJson(doc, json);
  request->send(HTTP_CODE_OK, "application/json", json);
}

// ---- GET /api/measurements -------------------------------------------------

/**
 * GET /api/measurements
 * Returns measured output voltage, current, and power from the last poll cycle.
 * No Modbus I/O on the request path — served from cache.
 *
 * Java equivalent: RestService#getMeasurements
 */
static void handleGetMeasurements(AsyncWebServerRequest* request) {
  ESP_LOGI(TAG_SRV, "Request: GET /api/measurements");
  JsonDocument doc;
  doc["voltage"] = converterState.getVoltageOut();
  doc["current"] = converterState.getCurrentOut();
  doc["power"] = converterState.getPowerOut();
  String json;
  serializeJson(doc, json);
  request->send(HTTP_CODE_OK, "application/json", json);
}

// ---- GET /api/voltage ------------------------------------------------------

/**
 * GET /api/voltage
 * Returns the most recently measured output voltage.
 *
 * Java equivalent: RestService#getVoltage
 */
static void handleGetApiVoltage(AsyncWebServerRequest* request) {
  ESP_LOGI(TAG_SRV, "Request: GET /api/voltage");
  JsonDocument doc;
  doc["voltage"] = converterState.getVoltageOut();
  String json;
  serializeJson(doc, json);
  request->send(HTTP_CODE_OK, "application/json", json);
}

// ---- GET /api/current ------------------------------------------------------

/**
 * GET /api/current
 * Returns the most recently measured output current.
 *
 * Java equivalent: RestService#getCurrent
 */
static void handleGetApiCurrent(AsyncWebServerRequest* request) {
  ESP_LOGI(TAG_SRV, "Request: GET /api/current");
  JsonDocument doc;
  doc["current"] = converterState.getCurrentOut();
  String json;
  serializeJson(doc, json);
  request->send(HTTP_CODE_OK, "application/json", json);
}

// ---- GET /api/power --------------------------------------------------------

/**
 * GET /api/power
 * Returns the most recently measured output power.
 *
 * Java equivalent: RestService#getPower
 */
static void handleGetApiPower(AsyncWebServerRequest* request) {
  ESP_LOGI(TAG_SRV, "Request: GET /api/power");
  JsonDocument doc;
  doc["power"] = converterState.getPowerOut();
  String json;
  serializeJson(doc, json);
  request->send(HTTP_CODE_OK, "application/json", json);
}

// ---- PUT /api/measurements -------------------------------------------------

/**
 * PUT /api/measurements
 * Sets voltage and current setpoints atomically in a single 0x10 frame.
 * Request body: {"voltage": 5.0, "current": 1.0, "power": 0}  (power ignored).
 *
 * Java equivalent: RestService#setMeasurements → DeviceService#setMeasurements
 */
static void handlePutMeasurements(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
  ESP_LOGI(TAG_SRV, "Request: PUT /api/measurements");
  if (!isDeviceDetected()) {
    request->send(HTTP_CODE_SERVICE_UNAVAILABLE, "text/plain", "No device connected");
    return;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, data, len);
  if (err) {
    ESP_LOGW(TAG_SRV, "PUT /api/measurements: malformed JSON: %s", err.c_str());
    request->send(HTTP_CODE_BAD_REQUEST, "text/plain", "Malformed JSON");
    return;
  }
  if (!doc.containsKey("voltage") || !doc.containsKey("current")) {
    request->send(HTTP_CODE_BAD_REQUEST, "text/plain", "Missing 'voltage' or 'current' field");
    return;
  }
  double volts = doc["voltage"].as<double>();
  double amperes = doc["current"].as<double>();
  double maxV = converterState.getMaxVoltage();
  double minV = converterState.getMinVoltage();
  double maxI = converterState.getMaxCurrent();
  double minI = converterState.getMinCurrent();
  if (maxV > 0.0 && (volts < minV || volts > maxV)) {
    request->send(HTTP_CODE_BAD_REQUEST, "text/plain",
                  "Voltage out of range [" + String(minV, 2) + ", " + String(maxV, 2) + "]");
    return;
  }
  if (maxI > 0.0 && (amperes < minI || amperes > maxI)) {
    request->send(HTTP_CODE_BAD_REQUEST, "text/plain",
                  "Current out of range [" + String(minI, 3) + ", " + String(maxI, 3) + "]");
    return;
  }
  if (!activeDevice->setVoltageCurrent(volts, amperes)) {
    ESP_LOGE(TAG_SRV, "PUT /api/measurements: Modbus write failed");
    request->send(HTTP_CODE_INTERNAL_SERVER_ERROR, "text/plain", "Device write failed");
    return;
  }
  converterState.setVoltageSet(volts);
  converterState.setCurrentSet(amperes);
  request->send(HTTP_CODE_NO_CONTENT);
}

// ---- PUT /api/voltage ------------------------------------------------------

/**
 * PUT /api/voltage
 * Sets the output voltage setpoint.
 * Request body: {"voltage": 5.0}
 *
 * Java equivalent: RestService#setVoltage → DeviceService#setVoltage
 */
static void handlePutVoltage(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
  ESP_LOGI(TAG_SRV, "Request: PUT /api/voltage");
  if (!isDeviceDetected()) {
    request->send(HTTP_CODE_SERVICE_UNAVAILABLE, "text/plain", "No device connected");
    return;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, data, len);
  if (err || !doc.containsKey("voltage")) {
    request->send(HTTP_CODE_BAD_REQUEST, "text/plain", "Missing or malformed 'voltage' field");
    return;
  }
  double volts = doc["voltage"].as<double>();
  double maxV = converterState.getMaxVoltage();
  double minV = converterState.getMinVoltage();
  if (maxV > 0.0 && (volts < minV || volts > maxV)) {
    request->send(HTTP_CODE_BAD_REQUEST, "text/plain",
                  "Voltage out of range [" + String(minV, 2) + ", " + String(maxV, 2) + "]");
    return;
  }
  if (!activeDevice->setVoltage(volts)) {
    ESP_LOGE(TAG_SRV, "PUT /api/voltage: Modbus write failed");
    request->send(HTTP_CODE_INTERNAL_SERVER_ERROR, "text/plain", "Device write failed");
    return;
  }
  converterState.setVoltageSet(volts);
  request->send(HTTP_CODE_NO_CONTENT);
}

// ---- PUT /api/voltage/verified ---------------------------------------------

/**
 * PUT /api/voltage/verified
 * Sets the voltage setpoint and synchronously verifies the device accepted it
 * via an immediate Modbus read-back. Retries once after VERIFIED_READBACK_DELAY_MS.
 * Response body: {"voltageSet": 5.00}
 *
 * Java equivalent: RestService#setVoltageVerified → DeviceService#setVoltageVerified
 */
static void handlePutVoltageVerified(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
  ESP_LOGI(TAG_SRV, "Request: PUT /api/voltage/verified");
  if (!isDeviceDetected()) {
    request->send(HTTP_CODE_SERVICE_UNAVAILABLE, "text/plain", "No device connected");
    return;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, data, len);
  if (err || !doc.containsKey("voltage")) {
    request->send(HTTP_CODE_BAD_REQUEST, "text/plain", "Missing or malformed 'voltage' field");
    return;
  }
  double volts = doc["voltage"].as<double>();
  double maxV = converterState.getMaxVoltage();
  double minV = converterState.getMinVoltage();
  if (maxV > 0.0 && (volts < minV || volts > maxV)) {
    request->send(HTTP_CODE_BAD_REQUEST, "text/plain",
                  "Voltage out of range [" + String(minV, 2) + ", " + String(maxV, 2) + "]");
    return;
  }
  if (!activeDevice->setVoltage(volts)) {
    ESP_LOGE(TAG_SRV, "PUT /api/voltage/verified: Modbus write failed");
    request->send(HTTP_CODE_INTERNAL_SERVER_ERROR, "text/plain", "Device write failed");
    return;
  }
  converterState.setVoltageSet(volts);
  // Read-back verification — Java equivalent: DeviceService#setVoltageVerified
  vTaskDelay(pdMS_TO_TICKS(VERIFIED_READBACK_DELAY_MS));
  double confirmed = activeDevice->getVoltageSetVerified();
  if (fabs(confirmed - volts) > VERIFIED_TOLERANCE) {
    ESP_LOGD(TAG_SRV, "PUT /api/voltage/verified: first read-back %.3f, retrying", confirmed);
    vTaskDelay(pdMS_TO_TICKS(VERIFIED_READBACK_DELAY_MS));
    confirmed = activeDevice->getVoltageSetVerified();
    if (fabs(confirmed - volts) > VERIFIED_TOLERANCE) {
      ESP_LOGW(TAG_SRV, "PUT /api/voltage/verified: device did not accept %.3f V (read back %.3f V)", volts, confirmed);
      request->send(HTTP_CODE_CONFLICT, "text/plain",
                    "Voltage setpoint not accepted by device: requested " + String(volts, 3) +
                        " V, read back " + String(confirmed, 3) + " V");
      return;
    }
  }
  converterState.setVoltageSet(confirmed);
  JsonDocument resp;
  resp["voltageSet"] = confirmed;
  String json;
  serializeJson(resp, json);
  request->send(HTTP_CODE_OK, "application/json", json);
}

// ---- PUT /api/current ------------------------------------------------------

/**
 * PUT /api/current
 * Sets the output current setpoint.
 * Request body: {"current": 1.0}
 *
 * Java equivalent: RestService#setCurrent → DeviceService#setCurrent
 */
static void handlePutCurrent(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
  ESP_LOGI(TAG_SRV, "Request: PUT /api/current");
  if (!isDeviceDetected()) {
    request->send(HTTP_CODE_SERVICE_UNAVAILABLE, "text/plain", "No device connected");
    return;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, data, len);
  if (err || !doc.containsKey("current")) {
    request->send(HTTP_CODE_BAD_REQUEST, "text/plain", "Missing or malformed 'current' field");
    return;
  }
  double amperes = doc["current"].as<double>();
  double maxI = converterState.getMaxCurrent();
  double minI = converterState.getMinCurrent();
  if (maxI > 0.0 && (amperes < minI || amperes > maxI)) {
    request->send(HTTP_CODE_BAD_REQUEST, "text/plain",
                  "Current out of range [" + String(minI, 3) + ", " + String(maxI, 3) + "]");
    return;
  }
  if (!activeDevice->setCurrent(amperes)) {
    ESP_LOGE(TAG_SRV, "PUT /api/current: Modbus write failed");
    request->send(HTTP_CODE_INTERNAL_SERVER_ERROR, "text/plain", "Device write failed");
    return;
  }
  converterState.setCurrentSet(amperes);
  request->send(HTTP_CODE_NO_CONTENT);
}

// ---- PUT /api/current/verified ---------------------------------------------

/**
 * PUT /api/current/verified
 * Sets the current setpoint and synchronously verifies the device accepted it
 * via an immediate Modbus read-back. Retries once after VERIFIED_READBACK_DELAY_MS.
 * Response body: {"currentSet": 1.000}
 *
 * Java equivalent: RestService#setCurrentVerified → DeviceService#setCurrentVerified
 */
static void handlePutCurrentVerified(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
  ESP_LOGI(TAG_SRV, "Request: PUT /api/current/verified");
  if (!isDeviceDetected()) {
    request->send(HTTP_CODE_SERVICE_UNAVAILABLE, "text/plain", "No device connected");
    return;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, data, len);
  if (err || !doc.containsKey("current")) {
    request->send(HTTP_CODE_BAD_REQUEST, "text/plain", "Missing or malformed 'current' field");
    return;
  }
  double amperes = doc["current"].as<double>();
  double maxI = converterState.getMaxCurrent();
  double minI = converterState.getMinCurrent();
  if (maxI > 0.0 && (amperes < minI || amperes > maxI)) {
    request->send(HTTP_CODE_BAD_REQUEST, "text/plain",
                  "Current out of range [" + String(minI, 3) + ", " + String(maxI, 3) + "]");
    return;
  }
  if (!activeDevice->setCurrent(amperes)) {
    ESP_LOGE(TAG_SRV, "PUT /api/current/verified: Modbus write failed");
    request->send(HTTP_CODE_INTERNAL_SERVER_ERROR, "text/plain", "Device write failed");
    return;
  }
  converterState.setCurrentSet(amperes);
  // Read-back verification — Java equivalent: DeviceService#setCurrentVerified
  vTaskDelay(pdMS_TO_TICKS(VERIFIED_READBACK_DELAY_MS));
  double confirmed = activeDevice->getCurrentSetVerified();
  if (fabs(confirmed - amperes) > VERIFIED_TOLERANCE) {
    ESP_LOGD(TAG_SRV, "PUT /api/current/verified: first read-back %.3f, retrying", confirmed);
    vTaskDelay(pdMS_TO_TICKS(VERIFIED_READBACK_DELAY_MS));
    confirmed = activeDevice->getCurrentSetVerified();
    if (fabs(confirmed - amperes) > VERIFIED_TOLERANCE) {
      ESP_LOGW(TAG_SRV, "PUT /api/current/verified: device did not accept %.3f A (read back %.3f A)", amperes, confirmed);
      request->send(HTTP_CODE_CONFLICT, "text/plain",
                    "Current setpoint not accepted by device: requested " + String(amperes, 3) +
                        " A, read back " + String(confirmed, 3) + " A");
      return;
    }
  }
  converterState.setCurrentSet(confirmed);
  JsonDocument resp;
  resp["currentSet"] = confirmed;
  String json;
  serializeJson(resp, json);
  request->send(HTTP_CODE_OK, "application/json", json);
}

// ---- PUT /api/output -------------------------------------------------------

/**
 * PUT /api/output
 * Enables or disables the converter output.
 * Request body: {"outputEnable": true}
 *
 * Java equivalent: RestService#setOutput → DeviceService#setOutput
 */
static void handlePutOutput(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
  ESP_LOGI(TAG_SRV, "Request: PUT /api/output");
  if (!isDeviceDetected()) {
    request->send(HTTP_CODE_SERVICE_UNAVAILABLE, "text/plain", "No device connected");
    return;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, data, len);
  if (err || !doc.containsKey("outputEnable")) {
    request->send(HTTP_CODE_BAD_REQUEST, "text/plain", "Missing or malformed 'outputEnable' field");
    return;
  }
  bool on = doc["outputEnable"].as<bool>();
  if (!activeDevice->setOutput(on)) {
    ESP_LOGE(TAG_SRV, "PUT /api/output: Modbus write failed");
    request->send(HTTP_CODE_INTERNAL_SERVER_ERROR, "text/plain", "Device write failed");
    return;
  }
  converterState.setOutputEnabled(on);
  request->send(HTTP_CODE_NO_CONTENT);
}

// ---- PUT /api/keypad -------------------------------------------------------

/**
 * PUT /api/keypad
 * Locks or unlocks the keypad (child lock).
 * Request body: {"keypadLock": true}
 *
 * Java equivalent: RestService#setKeypad → DeviceService#setKeypad
 */
static void handlePutKeypad(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
  ESP_LOGI(TAG_SRV, "Request: PUT /api/keypad");
  if (!isDeviceDetected()) {
    request->send(HTTP_CODE_SERVICE_UNAVAILABLE, "text/plain", "No device connected");
    return;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, data, len);
  if (err || !doc.containsKey("keypadLock")) {
    request->send(HTTP_CODE_BAD_REQUEST, "text/plain", "Missing or malformed 'keypadLock' field");
    return;
  }
  bool locked = doc["keypadLock"].as<bool>();
  if (!activeDevice->setKeypad(locked)) {
    ESP_LOGE(TAG_SRV, "PUT /api/keypad: Modbus write failed");
    request->send(HTTP_CODE_INTERNAL_SERVER_ERROR, "text/plain", "Device write failed");
    return;
  }
  converterState.setKeypadLocked(locked);
  request->send(HTTP_CODE_NO_CONTENT);
}

// ---- POST /api/protection/clear --------------------------------------------

/**
 * POST /api/protection/clear
 * Clears a tripped protection condition (OVP, OCP, etc.) on the device.
 *
 * Java equivalent: RestService#clearProtection → DeviceService#clearProtection
 */
static void handlePostProtectionClear(AsyncWebServerRequest* request) {
  ESP_LOGI(TAG_SRV, "Request: POST /api/protection/clear");
  if (!isDeviceDetected()) {
    request->send(HTTP_CODE_SERVICE_UNAVAILABLE, "text/plain", "No device connected");
    return;
  }
  if (!activeDevice->setProtectionState(false)) {
    ESP_LOGE(TAG_SRV, "POST /api/protection/clear: Modbus write failed");
    request->send(HTTP_CODE_INTERNAL_SERVER_ERROR, "text/plain", "Device write failed");
    return;
  }
  converterState.setProtectionState(0);
  request->send(HTTP_CODE_NO_CONTENT);
}

// ---- GET /status -----------------------------------------------------------

/**
 * GET /status
 */
static void handleGetStatus(AsyncWebServerRequest* request) {
  ESP_LOGI(TAG_SRV, "Request: GET /status");
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  fillESPInfo(root);
  String json;
  serializeJson(doc, json);
  request->send(HTTP_CODE_OK, "application/json", json);
}

// ---- GET /reset ------------------------------------------------------------

/**
 * GET /reset
 */
static void handleReset(AsyncWebServerRequest* request) {
  ESP_LOGW(TAG_SRV, "Request: GET /reset - CLEARING SETTINGS");
  request->send(HTTP_CODE_OK, "text/plain",
                "WiFi settings cleared. ESP32 rebooting to Configuration Mode...");
  delay(200);
  wm.resetSettings();
  ESP.restart();
}

// ---- GET / — HTML landing page ---------------------------------------------

/**
 * GET /  — HTML landing page listing all API endpoints.
 *
 * Java equivalent: no direct equivalent — Javalin serves Swagger UI at /openapi/ui.
 */
static void handleRoot(AsyncWebServerRequest* request) {
  ESP_LOGI(TAG_SRV, "Request: GET /");
  String ip = WiFi.localIP().toString();
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>SerialController API</title>";
  html += "<style>";
  html += "body{font-family:sans-serif;line-height:1.6;padding:20px;color:#333;max-width:900px;margin:auto}";
  html += "h1,h2{border-bottom:2px solid #eee;padding-bottom:6px}";
  html += "code{background:#f4f4f4;padding:2px 5px;border-radius:3px;font-family:monospace}";
  html += "pre{background:#f4f4f4;padding:12px;border-radius:5px;overflow-x:auto;border-left:4px solid #007bff;margin:4px 0}";
  html += ".ep{margin-bottom:18px;border:1px solid #eee;padding:12px;border-radius:8px}";
  html += ".m{display:inline-block;padding:2px 7px;border-radius:4px;font-size:12px;font-weight:bold;color:#fff;margin-right:6px}";
  html += ".GET{background:#28a745}.PUT{background:#fd7e14}.POST{background:#007bff}";
  html += "a{color:#007bff;text-decoration:none}a:hover{text-decoration:underline}";
  html += ".warn{color:#856404;background:#fff3cd;padding:8px;border-radius:4px;border:1px solid #ffeeba}";
  html += ".dep{color:#6c757d;font-size:12px}";
  html += "</style></head><body>";

  html += "<h1>SerialController REST API</h1>";
  html += "<p>IP: <strong>" + ip + "</strong> | SSID: <strong>" + WiFi.SSID() + "</strong></p>";
  html += "<p>WebSocket: <code>ws://" + ip + "/ws/data</code></p>";

  html += "<h2>State &amp; Limits</h2>";

  html += "<div class='ep'><span class='m GET'>GET</span><a href='/api/state'><code>/api/state</code></a>";
  html += "<p>Full ConverterState snapshot (all fields).</p>";
  html += "<pre>curl http://" + ip + "/api/state</pre></div>";

  html += "<div class='ep'><span class='m GET'>GET</span><a href='/api/limits'><code>/api/limits</code></a>";
  html += "<p>Device capability limits: min/max voltage, current, power.</p>";
  html += "<pre>curl http://" + ip + "/api/limits</pre></div>";

  html += "<h2>Measurements (read-only)</h2>";

  html += "<div class='ep'><span class='m GET'>GET</span><a href='/api/measurements'><code>/api/measurements</code></a>";
  html += "<p>Measured output voltage, current and power.</p>";
  html += "<pre>curl http://" + ip + "/api/measurements</pre></div>";

  html += "<div class='ep'><span class='m GET'>GET</span><a href='/api/voltage'><code>/api/voltage</code></a>";
  html += "<p>Measured output voltage (V).</p>";
  html += "<pre>curl http://" + ip + "/api/voltage</pre></div>";

  html += "<div class='ep'><span class='m GET'>GET</span><a href='/api/current'><code>/api/current</code></a>";
  html += "<p>Measured output current (A).</p>";
  html += "<pre>curl http://" + ip + "/api/current</pre></div>";

  html += "<div class='ep'><span class='m GET'>GET</span><a href='/api/power'><code>/api/power</code></a>";
  html += "<p>Measured output power (W).</p>";
  html += "<pre>curl http://" + ip + "/api/power</pre></div>";

  html += "<h2>Setpoints (write)</h2>";

  html += "<div class='ep'><span class='m PUT'>PUT</span><code>/api/measurements</code>";
  html += "<p>Set voltage and current setpoints atomically.</p>";
  html += "<pre>curl -X PUT http://" + ip + "/api/measurements -H 'Content-Type: application/json' -d '{\"voltage\":5.0,\"current\":1.0,\"power\":0}'</pre></div>";

  html += "<div class='ep'><span class='m PUT'>PUT</span><code>/api/voltage</code>";
  html += "<p>Set output voltage setpoint. Returns 204 on success.</p>";
  html += "<pre>curl -X PUT http://" + ip + "/api/voltage -H 'Content-Type: application/json' -d '{\"voltage\":5.0}'</pre></div>";

  html += "<div class='ep'><span class='m PUT'>PUT</span><code>/api/voltage/verified</code>";
  html += "<p>Set voltage setpoint with synchronous read-back confirmation. Returns 200 + <code>{\"voltageSet\":…}</code>.</p>";
  html += "<pre>curl -X PUT http://" + ip + "/api/voltage/verified -H 'Content-Type: application/json' -d '{\"voltage\":5.0}'</pre></div>";

  html += "<div class='ep'><span class='m PUT'>PUT</span><code>/api/current</code>";
  html += "<p>Set output current setpoint. Returns 204 on success.</p>";
  html += "<pre>curl -X PUT http://" + ip + "/api/current -H 'Content-Type: application/json' -d '{\"current\":1.0}'</pre></div>";

  html += "<div class='ep'><span class='m PUT'>PUT</span><code>/api/current/verified</code>";
  html += "<p>Set current setpoint with synchronous read-back confirmation. Returns 200 + <code>{\"currentSet\":…}</code>.</p>";
  html += "<pre>curl -X PUT http://" + ip + "/api/current/verified -H 'Content-Type: application/json' -d '{\"current\":1.0}'</pre></div>";

  html += "<div class='ep'><span class='m PUT'>PUT</span><code>/api/output</code>";
  html += "<p>Enable or disable the converter output. Returns 204.</p>";
  html += "<pre>curl -X PUT http://" + ip + "/api/output -H 'Content-Type: application/json' -d '{\"outputEnable\":true}'</pre></div>";

  html += "<div class='ep'><span class='m PUT'>PUT</span><code>/api/keypad</code>";
  html += "<p>Lock or unlock the keypad (child lock). Returns 204.</p>";
  html += "<pre>curl -X PUT http://" + ip + "/api/keypad -H 'Content-Type: application/json' -d '{\"keypadLock\":true}'</pre></div>";

  html += "<h2>Device control</h2>";

  html += "<div class='ep'><span class='m POST'>POST</span><code>/api/protection/clear</code>";
  html += "<p>Clear a tripped protection condition (OVP, OCP, etc.). Returns 204.</p>";
  html += "<pre>curl -X POST http://" + ip + "/api/protection/clear</pre></div>";

  html += "<h2>Diagnostics</h2>";

  html += "<div class='ep'><span class='m GET'>GET</span><a href='/status'><code>/status</code></a>";
  html += "<p>ESP32 hardware and WiFi diagnostics.</p>";
  html += "<pre>curl http://" + ip + "/status</pre></div>";

  html += "<div class='ep'><span class='m GET'>GET</span><a href='/reset'><code>/reset</code></a>";
  html += "<p class='warn'><strong>WARNING:</strong> Clears saved WiFi credentials and reboots into Configuration Mode.</p>";
  html += "<pre>curl http://" + ip + "/reset</pre></div>";

  html += "<h2>Deprecated</h2>";
  html += "<div class='ep'><span class='m GET'>GET</span><code class='dep'>/voltage</code> &nbsp;";
  html += "<span class='m POST'>POST</span><code class='dep'>/setVoltage?v=x</code>";
  html += "<p class='dep'>Legacy endpoints kept for backward compatibility. Use <code>/api/voltage</code> and <code>PUT /api/voltage</code> instead.</p></div>";

  html += "</body></html>";
  request->send(HTTP_CODE_OK, "text/html", html);
}

// ---- setupServer -----------------------------------------------------------

/**
 * Initialize WiFi using WiFiManager, then start the async HTTP + WebSocket server.
 */
void setupServer() {
  ESP_LOGI(TAG_SRV, "Initializing WiFiManager...");
  bool res = wm.autoConnect("SerialController");

  if (!res) {
    ESP_LOGE(TAG_SRV, "WiFi Connection Failed! Halting with SOS signal.");
    haltWithSOS();  // never returns
  }
  ESP_LOGI(TAG_SRV, "WiFi Connected! IP: %s", WiFi.localIP().toString().c_str());

  // Initialise WebSocketService: registers event handler, starts broadcast task.
  // Java equivalent: WebSocketService#start called after Javalin server starts.
  wsService.begin();
  server.addHandler(&ws);

  // HTTP routes — qualify with AsyncWebRequestMethod:: to avoid ambiguity with
  // the http_parser HTTP_GET/HTTP_POST macros pulled in via WiFiManager → WebServer.h

  // Landing page
  server.on("/", AsyncWebRequestMethod::HTTP_GET, handleRoot);

  // State & limits (read-only, no body)
  server.on("/api/state", AsyncWebRequestMethod::HTTP_GET, handleGetState);
  server.on("/api/limits", AsyncWebRequestMethod::HTTP_GET, handleGetLimits);

  // Measurements (read-only, no body)
  server.on("/api/measurements", AsyncWebRequestMethod::HTTP_GET, handleGetMeasurements);
  server.on("/api/voltage", AsyncWebRequestMethod::HTTP_GET, handleGetApiVoltage);
  server.on("/api/current", AsyncWebRequestMethod::HTTP_GET, handleGetApiCurrent);
  server.on("/api/power", AsyncWebRequestMethod::HTTP_GET, handleGetApiPower);

  // Setpoints (write, JSON body via onBody callback)
  // Java equivalent: router.put(URI_MEASUREMENTS, this::setMeasurements) etc.
  server.on(
      "/api/measurements", AsyncWebRequestMethod::HTTP_PUT, [](AsyncWebServerRequest* r) {}, nullptr, handlePutMeasurements);
  server.on(
      "/api/voltage", AsyncWebRequestMethod::HTTP_PUT, [](AsyncWebServerRequest* r) {}, nullptr, handlePutVoltage);
  server.on(
      "/api/voltage/verified", AsyncWebRequestMethod::HTTP_PUT, [](AsyncWebServerRequest* r) {}, nullptr, handlePutVoltageVerified);
  server.on(
      "/api/current", AsyncWebRequestMethod::HTTP_PUT, [](AsyncWebServerRequest* r) {}, nullptr, handlePutCurrent);
  server.on(
      "/api/current/verified", AsyncWebRequestMethod::HTTP_PUT, [](AsyncWebServerRequest* r) {}, nullptr, handlePutCurrentVerified);
  server.on(
      "/api/output", AsyncWebRequestMethod::HTTP_PUT, [](AsyncWebServerRequest* r) {}, nullptr, handlePutOutput);
  server.on(
      "/api/keypad", AsyncWebRequestMethod::HTTP_PUT, [](AsyncWebServerRequest* r) {}, nullptr, handlePutKeypad);

  // Device control (no body)
  server.on("/api/protection/clear", AsyncWebRequestMethod::HTTP_POST, handlePostProtectionClear);

  // Diagnostics
  server.on("/status", AsyncWebRequestMethod::HTTP_GET, handleGetStatus);
  server.on("/reset", AsyncWebRequestMethod::HTTP_GET, handleReset);

  // Deprecated legacy endpoints (backward compatibility)
  server.on("/voltage", AsyncWebRequestMethod::HTTP_GET, handleGetVoltage);
  server.on("/setVoltage", AsyncWebRequestMethod::HTTP_POST, handleSetVoltage);

  server.begin();
  ESP_LOGI(TAG_SRV, "AsyncWebServer started on port 80 (HTTP + WS on /ws/data).");
}

/**
 * No-op: ESPAsyncWebServer handles all requests on its own internal FreeRTOS task.
 * Kept for API compatibility with the call site in applicationLoop().
 */
void handleServerRequests() {
  // intentionally empty
}
