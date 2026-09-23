#include "Server.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include "../../service/src/DeviceService.h"
#include "../../service/src/RestService.h"
#include "../../service/src/WebSocketService.h"
#include "ActiveDevice.h"
#include "ConverterStateGlobal.h"
#include "ESPInfo.h"
#include "esp_log.h"
#include "index_html.h"

/**
 * Server.cpp - WiFi initialisation, HTTP/WebSocket server wiring.
 *
 * Java equivalent: com.serial.SerialController#process - pure wiring/glue:
 *   - Connect to WiFi (replaces jSerialComm port selection on Java side)
 *   - Construct DeviceService, RestService, WebSocketService
 *   - Register all routes and start the server
 *
 * All /api/* handler logic lives in RestService (src/service/).
 * The polling loop and validated writes live in DeviceService (src/service/).
 * WebSocket broadcast + command-receive live in WebSocketService (src/service/).
 *
 * The ESP32-specific /status and /reset endpoints have no Java equivalent and
 * live here alongside the other platform-specific startup code.
 */

static const char* TAG_SRV = "SERVER";

// Onboard LED used for fault signalling (GPIO 2 on ESP32-WROOM-32)
#define FAULT_LED_PIN 2

/**
 * Blink the onboard LED in an SOS pattern (... --- ...) and halt.
 * Called when a fatal startup condition is detected (e.g. WiFi failure).
 * Never returns.
 *
 * No Java equivalent - on Java a fatal exception terminates the JVM.
 */
static void haltWithSOS() {
  pinMode(FAULT_LED_PIN, OUTPUT);
  const int dot = 150;
  const int dash = 450;
  const int gap = 150;
  const int word = 700;

  for (;;) {
    for (int i = 0; i < 3; i++) {
      digitalWrite(FAULT_LED_PIN, HIGH);
      delay(dot);
      digitalWrite(FAULT_LED_PIN, LOW);
      delay(gap);
    }
    delay(word);
    for (int i = 0; i < 3; i++) {
      digitalWrite(FAULT_LED_PIN, HIGH);
      delay(dash);
      digitalWrite(FAULT_LED_PIN, LOW);
      delay(gap);
    }
    delay(word);
    for (int i = 0; i < 3; i++) {
      digitalWrite(FAULT_LED_PIN, HIGH);
      delay(dot);
      digitalWrite(FAULT_LED_PIN, LOW);
      delay(gap);
    }
    delay(2000);
  }
}

// HTTP status codes
#define HTTP_CODE_OK 200
#define HTTP_CODE_SERVICE_UNAVAILABLE 503

// ---- Global server objects -------------------------------------------------

AsyncWebServer server(80);
AsyncWebSocket ws("/ws/data");
WebSocketService wsService(&ws, &converterState);

/**
 * DeviceService - owns the polling task and validated writes.
 * Constructed in setupServer() after activeDevice is available.
 * Declared extern so Application.cpp can access it for WS command dispatch.
 *
 * Java equivalent: DeviceService instance constructed in SerialController#process
 * and injected into RestService and WebSocketService.
 */
DeviceService* deviceServicePtr = nullptr;

WiFiManager wm;

// ---- Diagnostics endpoints (ESP32-specific, no Java equivalent) ------------

/**
 * GET /status - ESP32 hardware and WiFi diagnostics.
 * No Java equivalent (ESP32-specific endpoint).
 */
static void handleGetStatus(AsyncWebServerRequest* request) {
  ESP_LOGI(TAG_SRV, "GET /status");
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  fillESPInfo(root);
  String json;
  serializeJson(doc, json);
  request->send(HTTP_CODE_OK, "application/json", json);
}

/**
 * GET /reset - clears WiFi credentials and reboots.
 * No Java equivalent (ESP32-specific endpoint).
 */
static void handleReset(AsyncWebServerRequest* request) {
  ESP_LOGW(TAG_SRV, "GET /reset - CLEARING SETTINGS");
  request->send(HTTP_CODE_OK, "text/plain",
                "WiFi settings cleared. ESP32 rebooting to Configuration Mode...");
  delay(200);
  wm.resetSettings();
  ESP.restart();
}

// ---- Legacy / deprecated endpoints ----------------------------------------

/**
 * GET /voltage - legacy alias; prefer GET /api/voltage.
 */
static void handleGetVoltageLegacy(AsyncWebServerRequest* request) {
  ESP_LOGI(TAG_SRV, "GET /voltage (deprecated)");
  if (!converterState.isDeviceOnline()) {
    request->send(HTTP_CODE_SERVICE_UNAVAILABLE, "application/json", "{\"error\":\"Device offline\"}");
    return;
  }
  JsonDocument doc;
  doc["voltage"] = serialized(String(converterState.getVoltageOut(), 2));
  String json;
  serializeJson(doc, json);
  request->send(HTTP_CODE_OK, "application/json", json);
}

/**
 * POST /setVoltage?v=x - legacy alias; prefer PUT /api/voltage.
 */
static void handleSetVoltageLegacy(AsyncWebServerRequest* request) {
  if (!request->hasArg("v")) {
    request->send(400, "text/plain", "Bad Request: Missing 'v' parameter");
    return;
  }
  float v = request->arg("v").toFloat();
  ESP_LOGI(TAG_SRV, "POST /setVoltage?v=%.2f (deprecated)", v);
  if (activeDevice != nullptr && activeDevice->setVoltage(v)) {
    request->send(HTTP_CODE_OK, "text/plain", "Voltage set to: " + String(v, 2) + "V");
  } else {
    request->send(HTTP_CODE_SERVICE_UNAVAILABLE, "text/plain", "Riden Modbus Write Failed");
  }
}

// ---- Browser UI (Java reference implementation) ----------------------------

/**
 * GET / - Serial Controller live monitor UI.
 *
 * Serves the Java reference UI (Template/src/main/resources/public/index.html)
 * from PROGMEM via send_P() so the ~15 KB HTML never occupies DRAM at runtime.
 * The page self-contains all CSS and JS; no external assets are required.
 *
 * The embedded JS connects to ws://<host>/ws/data and drives all UI elements
 * directly from the ConverterState JSON broadcast (voltage, current, power,
 * setpoints, output toggle, keypad toggle, protection state, device identity).
 *
 * Java equivalent: Javalin serves Template/src/main/resources/public/index.html
 * as a classpath static file at GET /. Behaviour is identical.
 */
static void handleRoot(AsyncWebServerRequest* request) {
  ESP_LOGI(TAG_SRV, "GET /");
  request->send_P(HTTP_CODE_OK, "text/html", INDEX_HTML);
}

// ---- API reference documentation page -------------------------------------

/**
 * GET /doc - HTML reference page listing all REST endpoints.
 *
 * Kept alongside the browser UI so developers can inspect the API surface
 * without needing curl or an external tool. Not present in the Java original
 * (Java exposes Swagger UI at /openapi/ui from classpath static files).
 */
static void handleDoc(AsyncWebServerRequest* request) {
  ESP_LOGI(TAG_SRV, "GET /doc");
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
  html += "<p>Live monitor UI: <a href='/'><code>http://" + ip + "/</code></a></p>";
  html += "<p>WebSocket: <code>ws://" + ip + "/ws/data</code></p>";

  html += "<h2>State &amp; Limits</h2>";
  html += "<div class='ep'><span class='m GET'>GET</span><a href='/api/state'><code>/api/state</code></a>";
  html += "<p>Full ConverterState snapshot.</p>";
  html += "<pre>curl http://" + ip + "/api/state</pre></div>";
  html += "<div class='ep'><span class='m GET'>GET</span><a href='/api/limits'><code>/api/limits</code></a>";
  html += "<p>Device capability limits.</p>";
  html += "<pre>curl http://" + ip + "/api/limits</pre></div>";

  html += "<h2>Measurements (read-only)</h2>";
  html += "<div class='ep'><span class='m GET'>GET</span><a href='/api/measurements'><code>/api/measurements</code></a>";
  html += "<pre>curl http://" + ip + "/api/measurements</pre></div>";
  html += "<div class='ep'><span class='m GET'>GET</span><a href='/api/voltage'><code>/api/voltage</code></a>";
  html += "<pre>curl http://" + ip + "/api/voltage</pre></div>";
  html += "<div class='ep'><span class='m GET'>GET</span><a href='/api/current'><code>/api/current</code></a>";
  html += "<pre>curl http://" + ip + "/api/current</pre></div>";
  html += "<div class='ep'><span class='m GET'>GET</span><a href='/api/power'><code>/api/power</code></a>";
  html += "<pre>curl http://" + ip + "/api/power</pre></div>";

  html += "<h2>Setpoints (write)</h2>";
  html += "<div class='ep'><span class='m PUT'>PUT</span><code>/api/measurements</code>";
  html += "<pre>curl -X PUT http://" + ip + "/api/measurements -H 'Content-Type: application/json' -d '{\"voltage\":5.0,\"current\":1.0,\"power\":0}'</pre></div>";
  html += "<div class='ep'><span class='m PUT'>PUT</span><code>/api/voltage</code>";
  html += "<pre>curl -X PUT http://" + ip + "/api/voltage -H 'Content-Type: application/json' -d '{\"voltage\":5.0}'</pre></div>";
  html += "<div class='ep'><span class='m PUT'>PUT</span><code>/api/voltage/verified</code>";
  html += "<p>Returns 200 + <code>{\"voltageSet\":…}</code> on success, 409 on read-back mismatch.</p>";
  html += "<pre>curl -X PUT http://" + ip + "/api/voltage/verified -H 'Content-Type: application/json' -d '{\"voltage\":5.0}'</pre></div>";
  html += "<div class='ep'><span class='m PUT'>PUT</span><code>/api/current</code>";
  html += "<pre>curl -X PUT http://" + ip + "/api/current -H 'Content-Type: application/json' -d '{\"current\":1.0}'</pre></div>";
  html += "<div class='ep'><span class='m PUT'>PUT</span><code>/api/current/verified</code>";
  html += "<pre>curl -X PUT http://" + ip + "/api/current/verified -H 'Content-Type: application/json' -d '{\"current\":1.0}'</pre></div>";
  html += "<div class='ep'><span class='m PUT'>PUT</span><code>/api/output</code>";
  html += "<pre>curl -X PUT http://" + ip + "/api/output -H 'Content-Type: application/json' -d '{\"outputEnable\":true}'</pre></div>";
  html += "<div class='ep'><span class='m PUT'>PUT</span><code>/api/keypad</code>";
  html += "<pre>curl -X PUT http://" + ip + "/api/keypad -H 'Content-Type: application/json' -d '{\"keypadLock\":true}'</pre></div>";

  html += "<h2>Device control</h2>";
  html += "<div class='ep'><span class='m POST'>POST</span><code>/api/protection/clear</code>";
  html += "<pre>curl -X POST http://" + ip + "/api/protection/clear</pre></div>";

  html += "<h2>Diagnostics</h2>";
  html += "<div class='ep'><span class='m GET'>GET</span><a href='/status'><code>/status</code></a>";
  html += "<pre>curl http://" + ip + "/status</pre></div>";
  html += "<div class='ep'><span class='m GET'>GET</span><a href='/reset'><code>/reset</code></a>";
  html += "<p class='warn'><strong>WARNING:</strong> Clears WiFi credentials and reboots.</p></div>";

  html += "<h2>Deprecated</h2>";
  html += "<div class='ep'><span class='m GET'>GET</span><code class='dep'>/voltage</code> &nbsp;";
  html += "<span class='m POST'>POST</span><code class='dep'>/setVoltage?v=x</code>";
  html += "<p class='dep'>Kept for backward compatibility. Use <code>/api/voltage</code> and <code>PUT /api/voltage</code>.</p></div>";

  html += "</body></html>";
  request->send(HTTP_CODE_OK, "text/html", html);
}

// ---- setupServer -----------------------------------------------------------

/**
 * Connects to WiFi, constructs service objects, registers all routes, starts server.
 *
 * Java equivalent: SerialController#process - the wiring block that constructs
 * DeviceService, RestService, WebSocketService, registers Javalin routes, and
 * calls javalin.start(). WiFiManager replaces jSerialComm port selection.
 */
void setupServer() {
  ESP_LOGI(TAG_SRV, "Initializing WiFiManager...");
  if (!wm.autoConnect("SerialController")) {
    ESP_LOGE(TAG_SRV, "WiFi Connection Failed! Halting with SOS signal.");
    haltWithSOS();
  }
  ESP_LOGI(TAG_SRV, "WiFi Connected! IP: %s", WiFi.localIP().toString().c_str());

  // Construct DeviceService now that activeDevice is available.
  // Java equivalent: new DeviceService(portName, appConfig)
  static DeviceService deviceServiceInstance(&converterState, activeDevice);
  deviceServicePtr = &deviceServiceInstance;

  // Construct RestService with a pointer to the server and DeviceService.
  // Java equivalent: new RestService(deviceService, appConfig)
  static RestService restService(&server, deviceServicePtr);
  restService.registerRoutes();

  // Initialise WebSocketService.
  // Java equivalent: webSocketService.start() after javalin.start()
  wsService.begin();
  server.addHandler(&ws);

  // Browser UI (Java reference) and API reference doc (ESP32-specific)
  server.on("/", AsyncWebRequestMethod::HTTP_GET, handleRoot);
  server.on("/doc", AsyncWebRequestMethod::HTTP_GET, handleDoc);
  server.on("/status", AsyncWebRequestMethod::HTTP_GET, handleGetStatus);
  server.on("/reset", AsyncWebRequestMethod::HTTP_GET, handleReset);

  // Deprecated legacy endpoints
  server.on("/voltage", AsyncWebRequestMethod::HTTP_GET, handleGetVoltageLegacy);
  server.on("/setVoltage", AsyncWebRequestMethod::HTTP_POST, handleSetVoltageLegacy);

  server.begin();
  ESP_LOGI(TAG_SRV, "AsyncWebServer started on port 80 (HTTP + WS on /ws/data).");

  // Start the DeviceService polling task.
  // Java equivalent: deviceService.start() after javalin.start()
  deviceServicePtr->begin();
}

/**
 * No-op: ESPAsyncWebServer handles all requests on its own FreeRTOS task.
 *
 * Java equivalent: not needed - Javalin's Jetty handles requests on its own
 * thread pool without any manual pump call from main().
 */
void handleServerRequests() {
  // intentionally empty
}
