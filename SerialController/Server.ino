#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <esp_system.h>
#include <esp_chip_info.h>
#include "esp_log.h"

/**
 * Server.ino - WiFi Management and RESTful API
 */

static const char* TAG_SRV = "SERVER";

// HTTP Status Codes
#define HTTP_CODE_OK           200
#define HTTP_CODE_BAD_REQUEST  400
#define HTTP_CODE_NOT_FOUND    404
#define HTTP_CODE_SERVICE_UNAVAILABLE 503

// Global objects
WebServer server(80);
WiFiManager wm;

/**
 * Helper: Get Reset Reason as String
 */
const char* getResetReasonString(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_UNKNOWN:   return "Unknown";
    case ESP_RST_POWERON:   return "Power On";
    case ESP_RST_EXT:       return "External Pin";
    case ESP_RST_SW:        return "Software";
    case ESP_RST_PANIC:     return "Exception/Panic";
    case ESP_RST_INT_WDT:   return "Interrupt Watchdog";
    case ESP_RST_TASK_WDT:  return "Task Watchdog";
    case ESP_RST_WDT:       return "Other Watchdog";
    case ESP_RST_DEEPSLEEP: return "Deep Sleep Wakeup";
    case ESP_RST_BROWNOUT:  return "Brownout";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "Other";
  }
}

/**
 * GET /voltage
 */
void handleGetVoltage() {
  ESP_LOGI(TAG_SRV, "Request: GET /voltage");
  uint16_t vOutRaw;
  if (readModbusRegister(RIDEN_ID, REG_V_OUT, vOutRaw)) {
    float voltage = vOutRaw / 100.0;
    String json = "{\"voltage\": " + String(voltage, 2) + "}";
    server.send(HTTP_CODE_OK, "application/json", json);
  } else {
    ESP_LOGE(TAG_SRV, "Voltage Read Failed (Modbus Timeout)");
    server.send(HTTP_CODE_SERVICE_UNAVAILABLE, "application/json", "{\"error\": \"Riden Modbus Timeout\"}");
  }
}

/**
 * POST /setVoltage?v=5.0
 */
void handleSetVoltage() {
  if (server.hasArg("v")) {
    float voltageValue = server.arg("v").toFloat();
    ESP_LOGI(TAG_SRV, "Request: POST /setVoltage?v=%.2f", voltageValue);
    uint16_t rawValue = (uint16_t)(voltageValue * 100);
    
    if (writeModbusRegister(RIDEN_ID, REG_V_SET, rawValue)) {
      server.send(HTTP_CODE_OK, "text/plain", "Voltage set to: " + String(voltageValue, 2) + "V");
    } else {
      ESP_LOGE(TAG_SRV, "Voltage Write Failed (Modbus Error)");
      server.send(HTTP_CODE_SERVICE_UNAVAILABLE, "text/plain", "Riden Modbus Write Failed");
    }
  } else {
    ESP_LOGW(TAG_SRV, "Bad Request: Missing 'v' parameter");
    server.send(HTTP_CODE_BAD_REQUEST, "text/plain", "Bad Request: Missing 'v' parameter");
  }
}

/**
 * GET /status
 */
void handleGetStatus() {
  ESP_LOGI(TAG_SRV, "Request: GET /status");
  Serial.println("API Request: GET /status");
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);

  String json = "{";
  json += "\"chip\": {";
  json += "\"model\": \"" + String(ESP.getChipModel()) + "\",";
  json += "\"revision\": " + String(ESP.getChipRevision()) + ",";
  json += "\"cores\": " + String(chip_info.cores) + ",";
  json += "\"cpuFreqMHz\": " + String(ESP.getCpuFreqMHz()) + ",";
  json += "\"resetReason\": \"" + String(getResetReasonString(esp_reset_reason())) + "\"";
  json += "},";

  json += "\"memory\": {";
  json += "\"heapSize\": " + String(ESP.getHeapSize()) + ",";
  json += "\"freeHeap\": " + String(ESP.getFreeHeap()) + ",";
  json += "\"minFreeHeap\": " + String(ESP.getMinFreeHeap()) + ",";
  json += "\"flashSizeMB\": " + String(ESP.getFlashChipSize() / (1024 * 1024)) + ",";
  json += "\"psramSize\": " + String(ESP.getPsramSize());
  json += "},";

  json += "\"wifi\": {";
  json += "\"ip\": \"" + WiFi.localIP().toString() + "\",";
  json += "\"rssi\": " + String(WiFi.RSSI()) + ",";
  json += "\"mac\": \"" + WiFi.macAddress() + "\",";
  json += "\"ssid\": \"" + WiFi.SSID() + "\"";
  json += "}";
  json += "}";

  server.send(HTTP_CODE_OK, "application/json", json);
}

/**
 * GET /reset
 */
void handleReset() {
  ESP_LOGW(TAG_SRV, "Request: GET /reset - CLEARING SETTINGS");
  server.send(HTTP_CODE_OK, "text/plain", "WiFi settings cleared. ESP32 rebooting to Configuration Mode...");
  delay(1000);
  wm.resetSettings();
  ESP.restart();
}

/**
 * Root Handler
 */
void handleRoot() {
  ESP_LOGI(TAG_SRV, "Request: GET /");
  String ip = WiFi.localIP().toString();
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>SerialController API</title>";
  html += "<style>";
  html += "body { font-family: sans-serif; line-height: 1.6; padding: 20px; color: #333; max-width: 800px; margin: auto; }";
  html += "h1 { border-bottom: 2px solid #eee; padding-bottom: 10px; }";
  html += "code { background: #f4f4f4; padding: 2px 5px; border-radius: 3px; font-family: monospace; }";
  html += "pre { background: #f4f4f4; padding: 15px; border-radius: 5px; overflow-x: auto; border-left: 5px solid #007bff; }";
  html += ".endpoint { margin-bottom: 30px; border: 1px solid #eee; padding: 15px; border-radius: 8px; }";
  html += "a { color: #007bff; text-decoration: none; }";
  html += "a:hover { text-decoration: underline; }";
  html += ".warning { color: #856404; background-color: #fff3cd; padding: 10px; border-radius: 5px; border: 1px solid #ffeeba; }";
  html += "</style></head><body>";
  
  html += "<h1>SerialController REST API</h1>";
  html += "<p>Device IP: <strong>" + ip + "</strong> | SSID: <strong>" + WiFi.SSID() + "</strong></p>";

  html += "<div class='endpoint'>";
  html += "<h3>1. GET <a href='/voltage'>/voltage</a></h3>";
  html += "<p><strong>Description:</strong> Retrieves current output voltage from Riden.</p>";
  html += "<strong>Example:</strong><pre>curl http://" + ip + "/voltage</pre>";
  html += "</div>";

  html += "<div class='endpoint'>";
  html += "<h3>2. POST /setVoltage</h3>";
  html += "<p><strong>Description:</strong> Sets the target output voltage using the <code>v</code> parameter.</p>";
  html += "<strong>Example:</strong><pre>curl -X POST \"http://" + ip + "/setVoltage?v=5.0\"</pre>";
  html += "</div>";

  html += "<div class='endpoint'>";
  html += "<h3>3. GET <a href='/status'>/status</a></h3>";
  html += "<p><strong>Description:</strong> Retrieves ESP32 hardware and WiFi status.</p>";
  html += "<strong>Example:</strong><pre>curl http://" + ip + "/status</pre>";
  html += "</div>";

  html += "<div class='endpoint'>";
  html += "<h3>4. GET <a href='/reset'>/reset</a></h3>";
  html += "<p class='warning'><strong>WARNING:</strong> This will clear your saved WiFi credentials and reboot the device into Configuration Mode.</p>";
  html += "<strong>Example:</strong><pre>curl http://" + ip + "/reset</pre>";
  html += "</div>";

  html += "</body></html>";
  
  server.send(HTTP_CODE_OK, "text/html", html);
}

/**
 * Initialize WiFi using WiFiManager.
 */
void setupServer() {
  ESP_LOGI(TAG_SRV, "Initializing WiFiManager...");
  bool res = wm.autoConnect("SerialController");

  if (!res) {
    ESP_LOGE(TAG_SRV, "WiFi Connection Failed!");
  } else {
    ESP_LOGI(TAG_SRV, "WiFi Connected! IP: %s", WiFi.localIP().toString().c_str());
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/voltage", HTTP_GET, handleGetVoltage);
  server.on("/setVoltage", HTTP_POST, handleSetVoltage);
  server.on("/status", HTTP_GET, handleGetStatus);
  server.on("/reset", HTTP_GET, handleReset);
  
  server.begin();
  ESP_LOGI(TAG_SRV, "HTTP Server started on port 80.");
}

/**
 * Process incoming HTTP requests.
 */
void handleServerRequests() {
  server.handleClient();
}
