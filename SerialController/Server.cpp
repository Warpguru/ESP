#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include "ActiveDevice.h"
#include "ConverterStateGlobal.h"
#include "ESPInfo.h"
#include "esp_log.h"

/**
 * Server.ino - WiFi Management and RESTful API
 *
 * Uses ESPAsyncWebServer: HTTP and WebSocket on the same port (80), fully
 * non-blocking. HTTP handlers run on the async server's internal FreeRTOS task;
 * server.handleClient() is not needed and handleServerRequests() is a no-op.
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
#define HTTP_CODE_BAD_REQUEST 400
#define HTTP_CODE_NOT_FOUND 404
#define HTTP_CODE_SERVICE_UNAVAILABLE 503

// Global objects — AsyncWebServer handles HTTP and WebSocket on the same port
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
WiFiManager wm;

/**
 * WebSocket event handler (PoC stub — broadcast logic added in Step 5).
 * Endpoint: ws://<ip>/ws  (port 80, same as HTTP)
 */
static void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                      AwsEventType type, void* arg, uint8_t* data, size_t len) {
  // ESPAsyncWebServer callbacks run on the lwIP async TCP task, not the Arduino
  // main task. Use Serial.printf in addition to ESP_LOGI to ensure the message
  // is visible in the Arduino IDE Serial Monitor regardless of task context.
  if (type == WS_EVT_CONNECT) {
    ESP_LOGI(TAG_SRV, "[WS] Client #%u connected from %s", client->id(),
             client->remoteIP().toString().c_str());
    Serial.printf("[WS] Client #%u connected from %s\r\n", client->id(),
                  client->remoteIP().toString().c_str());
    client->text("{\"poc\":\"ESPAsyncWebServer WebSocket connected!\"}");
  } else if (type == WS_EVT_DISCONNECT) {
    ESP_LOGI(TAG_SRV, "[WS] Client #%u disconnected", client->id());
    Serial.printf("[WS] Client #%u disconnected\r\n", client->id());
  } else if (type == WS_EVT_ERROR) {
    ESP_LOGE(TAG_SRV, "[WS] Client #%u error", client->id());
    Serial.printf("[WS] Client #%u error\r\n", client->id());
  }
  // WS_EVT_DATA (incoming commands) handled in Step 6
}

/**
 * GET /voltage
 * Returns cached value from ConverterState; no live Modbus call.
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
 * Delegates to ConverterState::applyVoltageSetpoint which writes Modbus and
 * arms the anti-flicker settle window.
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

/**
 * GET /reset
 */
static void handleReset(AsyncWebServerRequest* request) {
  ESP_LOGW(TAG_SRV, "Request: GET /reset - CLEARING SETTINGS");
  // AsyncWebServer sends the response asynchronously; no manual flush needed.
  request->send(HTTP_CODE_OK, "text/plain",
                "WiFi settings cleared. ESP32 rebooting to Configuration Mode...");
  delay(200);
  wm.resetSettings();
  ESP.restart();
}

/**
 * GET /  — HTML landing page
 */
static void handleRoot(AsyncWebServerRequest* request) {
  ESP_LOGI(TAG_SRV, "Request: GET /");
  String ip = WiFi.localIP().toString();
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>SerialController API</title>";
  html += "<style>";
  html += "body{font-family:sans-serif;line-height:1.6;padding:20px;color:#333;max-width:800px;margin:auto}";
  html += "h1{border-bottom:2px solid #eee;padding-bottom:10px}";
  html += "code{background:#f4f4f4;padding:2px 5px;border-radius:3px;font-family:monospace}";
  html += "pre{background:#f4f4f4;padding:15px;border-radius:5px;overflow-x:auto;border-left:5px solid #007bff}";
  html += ".endpoint{margin-bottom:30px;border:1px solid #eee;padding:15px;border-radius:8px}";
  html += "a{color:#007bff;text-decoration:none}a:hover{text-decoration:underline}";
  html += ".warning{color:#856404;background:#fff3cd;padding:10px;border-radius:5px;border:1px solid #ffeeba}";
  html += "</style></head><body>";

  html += "<h1>SerialController REST API</h1>";
  html += "<p>Device IP: <strong>" + ip + "</strong> | SSID: <strong>" + WiFi.SSID() + "</strong></p>";
  html += "<p>WebSocket: <code>ws://" + ip + "/ws</code></p>";

  html += "<div class='endpoint'><h3>1. GET <a href='/voltage'>/voltage</a></h3>";
  html += "<p>Retrieves current output voltage from Riden.</p>";
  html += "<pre>curl http://" + ip + "/voltage</pre></div>";

  html += "<div class='endpoint'><h3>2. POST /setVoltage</h3>";
  html += "<p>Sets the target output voltage using the <code>v</code> parameter.</p>";
  html += "<pre>curl -X POST \"http://" + ip + "/setVoltage?v=5.0\"</pre></div>";

  html += "<div class='endpoint'><h3>3. GET <a href='/status'>/status</a></h3>";
  html += "<p>Retrieves full ESP32 hardware and WiFi diagnostics.</p>";
  html += "<pre>curl http://" + ip + "/status</pre></div>";

  html += "<div class='endpoint'><h3>4. GET <a href='/reset'>/reset</a></h3>";
  html += "<p class='warning'><strong>WARNING:</strong> Clears saved WiFi credentials and reboots into Configuration Mode.</p>";
  html += "<pre>curl http://" + ip + "/reset</pre></div>";

  html += "</body></html>";
  request->send(HTTP_CODE_OK, "text/html", html);
}

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

  // Attach WebSocket handler — endpoint: ws://<ip>/ws
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // HTTP routes — qualify with AsyncWebRequestMethod:: to avoid ambiguity with
  // the http_parser HTTP_GET/HTTP_POST macros pulled in via WiFiManager → WebServer.h
  server.on("/", AsyncWebRequestMethod::HTTP_GET, handleRoot);
  server.on("/voltage", AsyncWebRequestMethod::HTTP_GET, handleGetVoltage);
  server.on("/setVoltage", AsyncWebRequestMethod::HTTP_POST, handleSetVoltage);
  server.on("/status", AsyncWebRequestMethod::HTTP_GET, handleGetStatus);
  server.on("/reset", AsyncWebRequestMethod::HTTP_GET, handleReset);

  server.begin();
  ESP_LOGI(TAG_SRV, "AsyncWebServer started on port 80 (HTTP + WS on /ws).");
}

/**
 * No-op: ESPAsyncWebServer handles all requests on its own internal FreeRTOS task.
 * Kept for API compatibility with the call site in SerialController.ino loop().
 */
void handleServerRequests() {
  // intentionally empty
}
