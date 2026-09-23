#include "RestService.h"

#include <ArduinoJson.h>

#include "esp_log.h"

/**
 * RestService.cpp - RESTful HTTP API for the converter.
 *
 * Java equivalent: com.serial.service.RestService
 *
 * All handlers read state via _deviceService->getState() and delegate writes
 * to _deviceService validated methods — matching the Java layering exactly.
 */

static const char* TAG_RS = "REST";

// HTTP status code constants
static constexpr int HTTP_OK = 200;
static constexpr int HTTP_NO_CONTENT = 204;
static constexpr int HTTP_BAD_REQUEST = 400;
static constexpr int HTTP_CONFLICT = 409;
static constexpr int HTTP_INTERNAL_SERVER_ERROR = 500;
static constexpr int HTTP_SERVICE_UNAVAILABLE = 503;

// ---- Constructor -----------------------------------------------------------

RestService::RestService(AsyncWebServer* server, DeviceService* deviceService)
    : _server(server), _deviceService(deviceService) {
}

// ---- registerRoutes --------------------------------------------------------

/**
 * Registers all /api/* routes on the server.
 *
 * Java equivalent: RestService#registerRoutes
 */
void RestService::registerRoutes() {
  // GET handlers — no body, simple lambda captures this.
  _server->on("/api/state", AsyncWebRequestMethod::HTTP_GET,
              [this](AsyncWebServerRequest* r) { handleGetState(r); });
  _server->on("/api/limits", AsyncWebRequestMethod::HTTP_GET,
              [this](AsyncWebServerRequest* r) { handleGetLimits(r); });
  _server->on("/api/measurements", AsyncWebRequestMethod::HTTP_GET,
              [this](AsyncWebServerRequest* r) { handleGetMeasurements(r); });
  _server->on("/api/voltage", AsyncWebRequestMethod::HTTP_GET,
              [this](AsyncWebServerRequest* r) { handleGetVoltage(r); });
  _server->on("/api/current", AsyncWebRequestMethod::HTTP_GET,
              [this](AsyncWebServerRequest* r) { handleGetCurrent(r); });
  _server->on("/api/power", AsyncWebRequestMethod::HTTP_GET,
              [this](AsyncWebServerRequest* r) { handleGetPower(r); });

  // PUT/POST handlers — body delivered via onBody callback (three-arg form).
  _server->on(
      "/api/measurements", AsyncWebRequestMethod::HTTP_PUT,
      [](AsyncWebServerRequest* r) {},
      nullptr,
      [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t) {
        handlePutMeasurements(r, d, l, i, t);
      });
  _server->on(
      "/api/voltage", AsyncWebRequestMethod::HTTP_PUT,
      [](AsyncWebServerRequest* r) {},
      nullptr,
      [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t) {
        handlePutVoltage(r, d, l, i, t);
      });
  _server->on(
      "/api/voltage/verified", AsyncWebRequestMethod::HTTP_PUT,
      [](AsyncWebServerRequest* r) {},
      nullptr,
      [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t) {
        handlePutVoltageVerified(r, d, l, i, t);
      });
  _server->on(
      "/api/current", AsyncWebRequestMethod::HTTP_PUT,
      [](AsyncWebServerRequest* r) {},
      nullptr,
      [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t) {
        handlePutCurrent(r, d, l, i, t);
      });
  _server->on(
      "/api/current/verified", AsyncWebRequestMethod::HTTP_PUT,
      [](AsyncWebServerRequest* r) {},
      nullptr,
      [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t) {
        handlePutCurrentVerified(r, d, l, i, t);
      });
  _server->on(
      "/api/output", AsyncWebRequestMethod::HTTP_PUT,
      [](AsyncWebServerRequest* r) {},
      nullptr,
      [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t) {
        handlePutOutput(r, d, l, i, t);
      });
  _server->on(
      "/api/keypad", AsyncWebRequestMethod::HTTP_PUT,
      [](AsyncWebServerRequest* r) {},
      nullptr,
      [this](AsyncWebServerRequest* r, uint8_t* d, size_t l, size_t i, size_t t) {
        handlePutKeypad(r, d, l, i, t);
      });
  _server->on("/api/protection/clear", AsyncWebRequestMethod::HTTP_POST,
              [this](AsyncWebServerRequest* r) { handlePostProtectionClear(r); });

  ESP_LOGI(TAG_RS, "REST routes registered.");
}

// ---- GET /api/state --------------------------------------------------------

/**
 * Java equivalent: RestService#getState
 */
void RestService::handleGetState(AsyncWebServerRequest* request) {
  ESP_LOGI(TAG_RS, "GET /api/state");
  const ConverterState* s = _deviceService->getState();
  JsonDocument doc;
  doc["deviceName"] = s->getDeviceName();
  doc["manufacturer"] = s->getManufacturer();
  doc["firmwareVersion"] = s->getFirmwareVersion();
  doc["deviceOnline"] = s->isDeviceOnline();
  doc["converterTopology"] = (int)s->getConverterTopology();
  doc["voltageOut"] = s->getVoltageOut();
  doc["currentOut"] = s->getCurrentOut();
  doc["powerOut"] = s->getPowerOut();
  doc["voltageIn"] = s->getVoltageIn();
  doc["temperatureCelsius"] = s->getTemperatureCelsius();
  doc["voltageSet"] = s->getVoltageSet();
  doc["currentSet"] = s->getCurrentSet();
  doc["outputEnabled"] = s->isOutputEnabled();
  doc["keypadLocked"] = s->isKeypadLocked();
  doc["cvMode"] = s->isCvMode();
  doc["protectionState"] = s->getProtectionState();
  doc["maxVoltage"] = s->getMaxVoltage();
  doc["minVoltage"] = s->getMinVoltage();
  doc["maxCurrent"] = s->getMaxCurrent();
  doc["minCurrent"] = s->getMinCurrent();
  doc["maxPower"] = s->getMaxPower();
  doc["configMaxVoltage"] = s->getConfigMaxVoltage();
  doc["configMaxCurrent"] = s->getConfigMaxCurrent();
  String json;
  serializeJson(doc, json);
  request->send(HTTP_OK, "application/json", json);
}

// ---- GET /api/limits -------------------------------------------------------

/**
 * Java equivalent: RestService#getLimits
 */
void RestService::handleGetLimits(AsyncWebServerRequest* request) {
  ESP_LOGI(TAG_RS, "GET /api/limits");
  const ConverterState* s = _deviceService->getState();
  JsonDocument doc;
  doc["manufacturer"] = s->getManufacturer();
  doc["deviceName"] = s->getDeviceName();
  doc["minVoltage"] = s->getMinVoltage();
  doc["maxVoltage"] = s->getMaxVoltage();
  doc["minCurrent"] = s->getMinCurrent();
  doc["maxCurrent"] = s->getMaxCurrent();
  doc["maxPower"] = s->getMaxPower();
  String json;
  serializeJson(doc, json);
  request->send(HTTP_OK, "application/json", json);
}

// ---- GET /api/measurements -------------------------------------------------

/**
 * Java equivalent: RestService#getMeasurements
 */
void RestService::handleGetMeasurements(AsyncWebServerRequest* request) {
  ESP_LOGI(TAG_RS, "GET /api/measurements");
  const ConverterState* s = _deviceService->getState();
  JsonDocument doc;
  doc["voltage"] = s->getVoltageOut();
  doc["current"] = s->getCurrentOut();
  doc["power"] = s->getPowerOut();
  String json;
  serializeJson(doc, json);
  request->send(HTTP_OK, "application/json", json);
}

// ---- GET /api/voltage ------------------------------------------------------

/**
 * Java equivalent: RestService#getVoltage
 */
void RestService::handleGetVoltage(AsyncWebServerRequest* request) {
  ESP_LOGI(TAG_RS, "GET /api/voltage");
  JsonDocument doc;
  doc["voltage"] = _deviceService->getState()->getVoltageOut();
  String json;
  serializeJson(doc, json);
  request->send(HTTP_OK, "application/json", json);
}

// ---- GET /api/current ------------------------------------------------------

/**
 * Java equivalent: RestService#getCurrent
 */
void RestService::handleGetCurrent(AsyncWebServerRequest* request) {
  ESP_LOGI(TAG_RS, "GET /api/current");
  JsonDocument doc;
  doc["current"] = _deviceService->getState()->getCurrentOut();
  String json;
  serializeJson(doc, json);
  request->send(HTTP_OK, "application/json", json);
}

// ---- GET /api/power --------------------------------------------------------

/**
 * Java equivalent: RestService#getPower
 */
void RestService::handleGetPower(AsyncWebServerRequest* request) {
  ESP_LOGI(TAG_RS, "GET /api/power");
  JsonDocument doc;
  doc["power"] = _deviceService->getState()->getPowerOut();
  String json;
  serializeJson(doc, json);
  request->send(HTTP_OK, "application/json", json);
}

// ---- PUT /api/measurements -------------------------------------------------

/**
 * Java equivalent: RestService#setMeasurements → DeviceService#setMeasurements
 */
void RestService::handlePutMeasurements(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
  ESP_LOGI(TAG_RS, "PUT /api/measurements");
  if (!_deviceService->isDeviceDetected()) {
    request->send(HTTP_SERVICE_UNAVAILABLE, "text/plain", "No device connected");
    return;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, data, len);
  if (err || !doc.containsKey("voltage") || !doc.containsKey("current")) {
    request->send(HTTP_BAD_REQUEST, "text/plain", "Missing or malformed 'voltage'/'current' field");
    return;
  }
  double volts = doc["voltage"].as<double>();
  double amperes = doc["current"].as<double>();
  if (!_deviceService->setVoltageCurrent(volts, amperes)) {
    request->send(HTTP_BAD_REQUEST, "text/plain", "Value out of range or device write failed");
    return;
  }
  request->send(HTTP_NO_CONTENT);
}

// ---- PUT /api/voltage ------------------------------------------------------

/**
 * Java equivalent: RestService#setVoltage → DeviceService#setVoltage
 */
void RestService::handlePutVoltage(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
  ESP_LOGI(TAG_RS, "PUT /api/voltage");
  if (!_deviceService->isDeviceDetected()) {
    request->send(HTTP_SERVICE_UNAVAILABLE, "text/plain", "No device connected");
    return;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, data, len);
  if (err || !doc.containsKey("voltage")) {
    request->send(HTTP_BAD_REQUEST, "text/plain", "Missing or malformed 'voltage' field");
    return;
  }
  double volts = doc["voltage"].as<double>();
  if (!_deviceService->setVoltage(volts)) {
    request->send(HTTP_BAD_REQUEST, "text/plain", "Value out of range or device write failed");
    return;
  }
  request->send(HTTP_NO_CONTENT);
}

// ---- PUT /api/voltage/verified ---------------------------------------------

/**
 * Java equivalent: RestService#setVoltageVerified → DeviceService#setVoltageVerified
 */
void RestService::handlePutVoltageVerified(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
  ESP_LOGI(TAG_RS, "PUT /api/voltage/verified");
  if (!_deviceService->isDeviceDetected()) {
    request->send(HTTP_SERVICE_UNAVAILABLE, "text/plain", "No device connected");
    return;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, data, len);
  if (err || !doc.containsKey("voltage")) {
    request->send(HTTP_BAD_REQUEST, "text/plain", "Missing or malformed 'voltage' field");
    return;
  }
  double volts = doc["voltage"].as<double>();
  double confirmed = 0.0;
  bool conflict = false;
  if (!_deviceService->setVoltageVerified(volts, confirmed, conflict)) {
    if (conflict) {
      request->send(HTTP_CONFLICT, "text/plain", "Voltage setpoint not accepted by device");
    } else {
      request->send(HTTP_BAD_REQUEST, "text/plain", "Value out of range or device write failed");
    }
    return;
  }
  JsonDocument resp;
  resp["voltageSet"] = confirmed;
  String json;
  serializeJson(resp, json);
  request->send(HTTP_OK, "application/json", json);
}

// ---- PUT /api/current ------------------------------------------------------

/**
 * Java equivalent: RestService#setCurrent → DeviceService#setCurrent
 */
void RestService::handlePutCurrent(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
  ESP_LOGI(TAG_RS, "PUT /api/current");
  if (!_deviceService->isDeviceDetected()) {
    request->send(HTTP_SERVICE_UNAVAILABLE, "text/plain", "No device connected");
    return;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, data, len);
  if (err || !doc.containsKey("current")) {
    request->send(HTTP_BAD_REQUEST, "text/plain", "Missing or malformed 'current' field");
    return;
  }
  double amperes = doc["current"].as<double>();
  if (!_deviceService->setCurrent(amperes)) {
    request->send(HTTP_BAD_REQUEST, "text/plain", "Value out of range or device write failed");
    return;
  }
  request->send(HTTP_NO_CONTENT);
}

// ---- PUT /api/current/verified ---------------------------------------------

/**
 * Java equivalent: RestService#setCurrentVerified → DeviceService#setCurrentVerified
 */
void RestService::handlePutCurrentVerified(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
  ESP_LOGI(TAG_RS, "PUT /api/current/verified");
  if (!_deviceService->isDeviceDetected()) {
    request->send(HTTP_SERVICE_UNAVAILABLE, "text/plain", "No device connected");
    return;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, data, len);
  if (err || !doc.containsKey("current")) {
    request->send(HTTP_BAD_REQUEST, "text/plain", "Missing or malformed 'current' field");
    return;
  }
  double amperes = doc["current"].as<double>();
  double confirmed = 0.0;
  bool conflict = false;
  if (!_deviceService->setCurrentVerified(amperes, confirmed, conflict)) {
    if (conflict) {
      request->send(HTTP_CONFLICT, "text/plain", "Current setpoint not accepted by device");
    } else {
      request->send(HTTP_BAD_REQUEST, "text/plain", "Value out of range or device write failed");
    }
    return;
  }
  JsonDocument resp;
  resp["currentSet"] = confirmed;
  String json;
  serializeJson(resp, json);
  request->send(HTTP_OK, "application/json", json);
}

// ---- PUT /api/output -------------------------------------------------------

/**
 * Java equivalent: RestService#setOutput → DeviceService#setOutput
 */
void RestService::handlePutOutput(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
  ESP_LOGI(TAG_RS, "PUT /api/output");
  if (!_deviceService->isDeviceDetected()) {
    request->send(HTTP_SERVICE_UNAVAILABLE, "text/plain", "No device connected");
    return;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, data, len);
  if (err || !doc.containsKey("outputEnable")) {
    request->send(HTTP_BAD_REQUEST, "text/plain", "Missing or malformed 'outputEnable' field");
    return;
  }
  bool on = doc["outputEnable"].as<bool>();
  if (!_deviceService->setOutput(on)) {
    request->send(HTTP_INTERNAL_SERVER_ERROR, "text/plain", "Device write failed");
    return;
  }
  request->send(HTTP_NO_CONTENT);
}

// ---- PUT /api/keypad -------------------------------------------------------

/**
 * Java equivalent: RestService#setKeypad → DeviceService#setKeypad
 */
void RestService::handlePutKeypad(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
  ESP_LOGI(TAG_RS, "PUT /api/keypad");
  if (!_deviceService->isDeviceDetected()) {
    request->send(HTTP_SERVICE_UNAVAILABLE, "text/plain", "No device connected");
    return;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, data, len);
  if (err || !doc.containsKey("keypadLock")) {
    request->send(HTTP_BAD_REQUEST, "text/plain", "Missing or malformed 'keypadLock' field");
    return;
  }
  bool locked = doc["keypadLock"].as<bool>();
  if (!_deviceService->setKeypad(locked)) {
    request->send(HTTP_INTERNAL_SERVER_ERROR, "text/plain", "Device write failed");
    return;
  }
  request->send(HTTP_NO_CONTENT);
}

// ---- POST /api/protection/clear --------------------------------------------

/**
 * Java equivalent: RestService#clearProtection → DeviceService#clearProtection
 */
void RestService::handlePostProtectionClear(AsyncWebServerRequest* request) {
  ESP_LOGI(TAG_RS, "POST /api/protection/clear");
  if (!_deviceService->isDeviceDetected()) {
    request->send(HTTP_SERVICE_UNAVAILABLE, "text/plain", "No device connected");
    return;
  }
  if (!_deviceService->clearProtection()) {
    request->send(HTTP_INTERNAL_SERVER_ERROR, "text/plain", "Device write failed");
    return;
  }
  request->send(HTTP_NO_CONTENT);
}
