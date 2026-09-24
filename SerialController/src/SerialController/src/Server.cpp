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
#include "LogBuffer.h"
#include "StatusLed.h"
#include "index_html.h"
#include "openapi_json.h"

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

// ---- Global server objects -------------------------------------------------

AsyncWebServer server(SERVER_PORT);
AsyncWebSocket ws(WS_PATH);
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
  Log_info("GET /status");
  JsonDocument doc;
  JsonObject statusObject = doc.to<JsonObject>();
  fillESPInfo(statusObject);
  String json;
  serializeJson(doc, json);
  request->send(HTTP_CODE_OK, "application/json", json);
}

/**
 * GET /reset - clears WiFi credentials and reboots.
 * No Java equivalent (ESP32-specific endpoint).
 */
static void handleReset(AsyncWebServerRequest* request) {
  Log_warn("GET /reset - CLEARING SETTINGS");
  request->send(HTTP_CODE_OK, "text/plain",
                "WiFi settings cleared. ESP32 rebooting to Configuration Mode...");
  delay(RESET_REBOOT_DELAY_MS);
  wm.resetSettings();
  ESP.restart();
}

// ---- Legacy / deprecated endpoints ----------------------------------------

/**
 * GET /voltage - legacy alias; prefer GET /api/voltage.
 */
static void handleGetVoltageLegacy(AsyncWebServerRequest* request) {
  Log_info("GET /voltage (deprecated)");
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
 * POST /setVoltage?voltage=x - legacy alias; prefer PUT /api/voltage.
 */
static void handleSetVoltageLegacy(AsyncWebServerRequest* request) {
  if (!request->hasArg("voltage")) {
    request->send(HTTP_CODE_BAD_REQUEST, "text/plain", "Bad Request: Missing 'voltage' parameter");
    return;
  }
  float voltage = request->arg("voltage").toFloat();
  Log_info("POST /setVoltage?voltage=%.2f (deprecated)", voltage);
  if (activeDevice != nullptr && activeDevice->setVoltage(voltage)) {
    request->send(HTTP_CODE_OK, "text/plain", "Voltage set to: " + String(voltage, 2) + "V");
  } else {
    request->send(HTTP_CODE_SERVICE_UNAVAILABLE, "text/plain", "Riden Modbus Write Failed");
  }
}

// ---- Log retrieval (ESP32-specific, no Java equivalent) --------------------

/**
 * GET /api/log        - returns the last N log lines as a JSON array.
 * GET /api/log?clear=1 - clears the buffer, then returns an empty array.
 *
 * No Java equivalent - ESP32-specific remote diagnostics endpoint.
 * Allows log retrieval when no USB serial connection is available.
 */
static void handleGetLog(AsyncWebServerRequest* request) {
  Log_info("GET /api/log");
  if (request->hasArg("clear") && request->arg("clear") == "1") {
    Log.clear();
  }
  String json;
  Log.getJson(json);
  request->send(HTTP_CODE_OK, "application/json", json);
}

/**
 * PUT /api/log/level - change the active log level at runtime.
 *
 * Body: {"level":"DEBUG"}
 * Valid values: ERROR, WARN, INFO, DEBUG, TRACE (case-insensitive).
 * Returns 200 {"level":"DEBUG"} on success, 400 on unrecognised level.
 *
 * No Java equivalent - ESP32-specific remote diagnostics endpoint.
 */
static void handlePutLogLevel(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
  JsonDocument doc;
  DeserializationError deserializationError = deserializeJson(doc, data, len);
  if (deserializationError || !doc["level"].is<const char*>()) {
    request->send(HTTP_CODE_BAD_REQUEST, "application/json", "{\"error\":\"Expected {\\\"level\\\":\\\"INFO\\\"}\"}");
    return;
  }
  const char* levelName = doc["level"].as<const char*>();
  if (!Log.setLevelFromString(levelName)) {
    request->send(HTTP_CODE_BAD_REQUEST, "application/json", "{\"error\":\"Unknown level\"}");
    return;
  }
  // Persist to NVS so the new level survives a reboot.
  Log.saveLevel();
  Log_info("Log level changed to %s", Log.getLevelName());
  String json = "{\"level\":\"";
  json += Log.getLevelName();
  json += "\"}";
  request->send(HTTP_CODE_OK, "application/json", json);
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
  Log_info("GET /");
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
  Log_info("GET /doc");
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
  html += "<p>IP: <strong>" + ip + "</strong> | SSID: <strong>" + WiFi.SSID() +
          "</strong>"
          " | Built: <strong>" __DATE__ " " __TIME__ "</strong></p>";
  html += "<p>Live monitor UI: <a href='/'><code>http://" + ip + "/</code></a></p>";
  html += "<p>WebSocket: <code>ws://" + ip + "/ws/data</code></p>";
  html +=
      "<p>OpenAPI spec: <a href='/openapi.json'><code>/openapi.json</code></a>"
      " &nbsp;|&nbsp; Interactive explorer: <a href='/openapi/ui'><code>/openapi/ui</code></a></p>";

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

  html += "<h2>Device control</h2>";
  html += "<div class='ep'><span class='m PUT'>PUT</span><code>/api/output</code>";
  html += "<pre>curl -X PUT http://" + ip + "/api/output -H 'Content-Type: application/json' -d '{\"outputEnable\":true}'</pre></div>";
  html += "<div class='ep'><span class='m PUT'>PUT</span><code>/api/keypad</code>";
  html += "<pre>curl -X PUT http://" + ip + "/api/keypad -H 'Content-Type: application/json' -d '{\"keypadLock\":true}'</pre></div>";
  html += "<div class='ep'><span class='m POST'>POST</span><code>/api/protection/clear</code>";
  html += "<pre>curl -X POST http://" + ip + "/api/protection/clear</pre></div>";

  html += "<h2>Diagnostics</h2>";
  html += "<div class='ep'><span class='m GET'>GET</span><a href='/api/log'><code>/api/log</code></a>";
  html += "<p>Last " + String(LOG_BUFFER_LINES) + " log lines as JSON. Active level: <strong>" + String(Log.getLevelName()) + "</strong>. Add <code>?clear=1</code> to flush.</p>";
  html += "<pre>curl http://" + ip + "/api/log</pre></div>";
  html += "<div class='ep'><span class='m PUT'>PUT</span><code>/api/log/level</code>";
  html += "<p>Change active log level at runtime. Levels: ERROR, WARN, INFO, DEBUG, TRACE.</p>";
  html += "<pre>curl -X PUT http://" + ip + "/api/log/level -H 'Content-Type: application/json' -d '{\"level\":\"DEBUG\"}'</pre></div>";
  html += "<div class='ep'><span class='m GET'>GET</span><a href='/status'><code>/status</code></a>";
  html += "<pre>curl http://" + ip + "/status</pre></div>";
  html += "<div class='ep'><span class='m GET'>GET</span><a href='/reset'><code>/reset</code></a>";
  html += "<p class='warn'><strong>WARNING:</strong> Clears WiFi credentials and reboots.</p></div>";

  html += "<h2>Deprecated</h2>";
  html += "<div class='ep'><span class='m GET'>GET</span><code class='dep'>/voltage</code> &nbsp;";
  html += "<span class='m POST'>POST</span><code class='dep'>/setVoltage?voltage=x</code>";
  html += "<p class='dep'>Kept for backward compatibility. Use <code>/api/voltage</code> and <code>PUT /api/voltage</code>.</p></div>";

  html += "</body></html>";
  request->send(HTTP_CODE_OK, "text/html", html);
}

// ---- OpenAPI spec + Swagger UI ---------------------------------------------

/**
 * GET /openapi.json - OpenAPI 3.0.0 specification served from PROGMEM.
 *
 * Java equivalent: Javalin auto-generates /openapi from Javalin-OpenAPI annotations.
 * On ESP32 the spec is hand-authored and stored in flash (openapi_json.h).
 */
static void handleGetOpenApiJson(AsyncWebServerRequest* request) {
  Log_info("GET /openapi.json");
  request->send_P(HTTP_CODE_OK, "application/json", OPENAPI_JSON);
}

/**
 * GET /openapi/ui - Swagger UI loaded from unpkg CDN (swagger-ui-dist@5.33.0).
 *
 * The HTML stub is ~700 bytes. The browser fetches Swagger UI JS/CSS from
 * unpkg.com and then fetches /openapi.json from this device. Requires the
 * developer's browser to have internet access; the ESP32 itself does not.
 *
 * Java equivalent: Javalin serves Swagger UI at /openapi/ui from classpath statics.
 */
static void handleGetOpenApiUi(AsyncWebServerRequest* request) {
  Log_info("GET /openapi/ui");
  String ip = WiFi.localIP().toString();
  String html =
      "<!DOCTYPE html>"
      "<html lang=\"en\"><head>"
      "<meta charset=\"utf-8\"/>"
      "<title>SerialController API</title>"
      "<link rel=\"stylesheet\" href=\"https://unpkg.com/swagger-ui-dist@5.33.0/swagger-ui.css\"/>"
      "<style>"
      ".build-banner{background:#f8f9fa;border-bottom:1px solid #e9ecef;padding:8px 16px;font-family:sans-serif;font-size:13px;color:#495057}"
      ".build-banner strong{color:#212529}"
      "</style>"
      "</head><body>"
      "<div class=\"build-banner\">IP: <strong>" + ip + "</strong> | SSID: <strong>" + WiFi.SSID() +
      "</strong> | Built: <strong>" __DATE__ " " __TIME__ "</strong></div>"
      "<div id=\"swagger-ui\"></div>"
      "<script src=\"https://unpkg.com/swagger-ui-dist@5.33.0/swagger-ui-bundle.js\"></script>"
      "<script>"
      "window.onload=()=>{"
      "window.ui=SwaggerUIBundle({"
      "url:'/openapi.json',"
      "dom_id:'#swagger-ui',"
      "presets:[SwaggerUIBundle.presets.apis,SwaggerUIBundle.SwaggerUIStandalonePreset],"
      "layout:'BaseLayout'"
      "});"
      "};"
      "</script>"
      "</body></html>";
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
  Log_info("Initializing WiFiManager...");

  // Inject a log-level <select> into the WiFiManager captive-portal form.
  // The custom HTML is rendered verbatim inside the portal page by WiFiManager.
  // The <select> name "logLevel" is used to retrieve the chosen value after
  // autoConnect() returns.  The default option is whichever level is currently
  // active (loaded from NVS by LogBuffer::begin() or the compile-time default).
  //
  // Built by iterating LogLevel::values() - no hardcoded strings, no per-level
  // local variables.  strncat appends directly into the fixed buffer; remaining
  // tracks how many bytes are still free to prevent overflow.
  char portalHtml[300];
  size_t remaining = sizeof(portalHtml);
  portalHtml[0] = '\0';
  strncat(portalHtml, "<br/><label for='logLevel'>Log Level</label>"
                      "<select id='logLevel' name='logLevel'>", remaining - 1);
  remaining -= strlen(portalHtml);

  size_t levelCount;
  const LogLevel* const* levels = LogLevel::values(levelCount);
  const char* currentLevel = Log.getLevelName();
  for (size_t i = 0; i < levelCount; i++) {
    const char* levelName = levels[i]->name();
    const char* selected = (strcmp(currentLevel, levelName) == 0) ? " selected" : "";
    char option[60];
    snprintf(option, sizeof(option), "<option value='%s'%s>%s</option>",
             levelName, selected, levelName);
    strncat(portalHtml, option, remaining - 1);
    remaining -= strlen(option);
  }
  strncat(portalHtml, "</select>", remaining - 1);

  // WiFiManagerParameter with an empty id/label injects raw HTML into the form.
  WiFiManagerParameter logLevelParam(portalHtml);
  wm.addParameter(&logLevelParam);

  // Read the submitted log level inside the save-params callback, which fires
  // during form submission while WiFiManager's internal web server is still
  // alive and its request arguments are still valid.  Reading wm.server->arg()
  // AFTER autoConnect() returns is too late - the server has already shut down
  // and the args are gone, which is why the level appeared not to be saved.
  //
  // setSaveConfigCallback fires whenever WiFiManager saves ANY configuration
  // (WiFi credentials or custom params).  It is registered in addition to
  // setSaveParamsCallback because on some WiFiManager versions the params
  // callback is only triggered when the custom-params form is submitted
  // separately - it does not fire on a plain first-time credential save.
  // Registering both callbacks ensures the level is always persisted regardless
  // of which form the user submitted.
  auto saveLevelCallback = [&logLevelParam]() {
    const char* chosen = logLevelParam.getValue();
    if (chosen != nullptr && chosen[0] != '\0' && Log.setLevelFromString(chosen)) {
      Log.saveLevel();
      Log_info("Log level set from portal to %s", Log.getLevelName());
    }
  };
  wm.setSaveParamsCallback(saveLevelCallback);
  wm.setSaveConfigCallback(saveLevelCallback);

  if (!wm.autoConnect("SerialController")) {
    Log_error("WiFi Connection Failed! Halting with SOS signal.");
    statusLed.setState(LedState::FAULT);
    vTaskSuspend(NULL);  // suspend the calling task; ledTask drives SOS forever
  }

  Log_info("WiFi Connected! IP: %s", WiFi.localIP().toString().c_str());

  // Construct DeviceService now that activeDevice is available.
  // Java equivalent: new DeviceService(portName, appConfig)
  static DeviceService deviceServiceInstance(&converterState, activeDevice);
  deviceServicePtr = &deviceServiceInstance;

  // Load device capability limits from the compile-time catalogue into
  // ConverterState. Must run before begin() starts the polling task so
  // that effectiveMaxVoltage() / effectiveMaxCurrent() have valid bounds
  // before the first setpoint write can arrive over REST or WebSocket.
  // Java equivalent: DeviceService constructor → loadLimits()
  deviceServicePtr->loadLimits();

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
  server.on("/openapi.json", AsyncWebRequestMethod::HTTP_GET, handleGetOpenApiJson);
  server.on("/openapi/ui", AsyncWebRequestMethod::HTTP_GET, handleGetOpenApiUi);
  server.on("/api/log", AsyncWebRequestMethod::HTTP_GET, handleGetLog);
  server.on(
      "/api/log/level", AsyncWebRequestMethod::HTTP_PUT,
      [](AsyncWebServerRequest* request) {},
      nullptr,
      handlePutLogLevel);
  server.on("/status", AsyncWebRequestMethod::HTTP_GET, handleGetStatus);
  server.on("/reset", AsyncWebRequestMethod::HTTP_GET, handleReset);

  // Deprecated legacy endpoints
  server.on("/voltage", AsyncWebRequestMethod::HTTP_GET, handleGetVoltageLegacy);
  server.on("/setVoltage", AsyncWebRequestMethod::HTTP_POST, handleSetVoltageLegacy);

  server.begin();
  Log_info("AsyncWebServer started on port 80 (HTTP + WS on /ws/data).");

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
