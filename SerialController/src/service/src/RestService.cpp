#include "RestService.h"

#include <ArduinoJson.h>

#include "../../../src/SerialController/src/LogBuffer.h"

/**
 * RestService.cpp - RESTful HTTP API for the converter.
 *
 * Java equivalent: com.serial.service.RestService
 *
 * All handlers read state via _deviceService->getState() and delegate writes
 * to _deviceService validated methods - matching the Java layering exactly.
 */

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
  // GET handlers - no body, simple lambda captures this.
  _server->on("/api/state", AsyncWebRequestMethod::HTTP_GET,
              [this](AsyncWebServerRequest* request) { handleGetState(request); });
  _server->on("/api/limits", AsyncWebRequestMethod::HTTP_GET,
              [this](AsyncWebServerRequest* request) { handleGetLimits(request); });
  _server->on("/api/measurements", AsyncWebRequestMethod::HTTP_GET,
              [this](AsyncWebServerRequest* request) { handleGetMeasurements(request); });
  _server->on("/api/voltage", AsyncWebRequestMethod::HTTP_GET,
              [this](AsyncWebServerRequest* request) { handleGetVoltage(request); });
  _server->on("/api/current", AsyncWebRequestMethod::HTTP_GET,
              [this](AsyncWebServerRequest* request) { handleGetCurrent(request); });
  _server->on("/api/power", AsyncWebRequestMethod::HTTP_GET,
              [this](AsyncWebServerRequest* request) { handleGetPower(request); });

  // PUT/POST handlers - body delivered via onBody callback (three-arg form).
  _server->on(
      "/api/measurements", AsyncWebRequestMethod::HTTP_PUT,
      [](AsyncWebServerRequest* request) {},
      nullptr,
      [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
        handlePutMeasurements(request, data, len, index, total);
      });
  _server->on(
      "/api/voltage", AsyncWebRequestMethod::HTTP_PUT,
      [](AsyncWebServerRequest* request) {},
      nullptr,
      [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
        handlePutVoltage(request, data, len, index, total);
      });
  _server->on(
      "/api/voltage/verified", AsyncWebRequestMethod::HTTP_PUT,
      [](AsyncWebServerRequest* request) {},
      nullptr,
      [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
        handlePutVoltageVerified(request, data, len, index, total);
      });
  _server->on(
      "/api/current", AsyncWebRequestMethod::HTTP_PUT,
      [](AsyncWebServerRequest* request) {},
      nullptr,
      [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
        handlePutCurrent(request, data, len, index, total);
      });
  _server->on(
      "/api/current/verified", AsyncWebRequestMethod::HTTP_PUT,
      [](AsyncWebServerRequest* request) {},
      nullptr,
      [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
        handlePutCurrentVerified(request, data, len, index, total);
      });
  _server->on(
      "/api/output", AsyncWebRequestMethod::HTTP_PUT,
      [](AsyncWebServerRequest* request) {},
      nullptr,
      [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
        handlePutOutput(request, data, len, index, total);
      });
  _server->on(
      "/api/keypad", AsyncWebRequestMethod::HTTP_PUT,
      [](AsyncWebServerRequest* request) {},
      nullptr,
      [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
        handlePutKeypad(request, data, len, index, total);
      });
  _server->on("/api/protection/clear", AsyncWebRequestMethod::HTTP_POST,
              [this](AsyncWebServerRequest* request) { handlePostProtectionClear(request); });

  Log_info("REST routes registered.");
}

// ---- GET /api/state --------------------------------------------------------

/**
 * Java equivalent: RestService#getState
 */
void RestService::handleGetState(AsyncWebServerRequest* request) {
  Log_info("GET /api/state");
  const ConverterState* state = _deviceService->getState();
  JsonDocument doc;
  doc["deviceName"] = state->getDeviceName();
  doc["manufacturer"] = state->getManufacturer();
  doc["firmwareVersion"] = state->getFirmwareVersion();
  doc["deviceOnline"] = state->isDeviceOnline();
  doc["converterTopology"] = (int)state->getConverterTopology();
  doc["voltageOut"] = state->getVoltageOut();
  doc["currentOut"] = state->getCurrentOut();
  doc["powerOut"] = state->getPowerOut();
  doc["voltageIn"] = state->getVoltageIn();
  doc["temperatureCelsius"] = state->getTemperatureCelsius();
  doc["voltageSet"] = state->getVoltageSet();
  doc["currentSet"] = state->getCurrentSet();
  doc["outputEnabled"] = state->isOutputEnabled();
  doc["keypadLocked"] = state->isKeypadLocked();
  doc["cvMode"] = state->isCvMode();
  doc["protectionState"] = state->getProtectionState();
  doc["maxVoltage"] = state->getMaxVoltage();
  doc["minVoltage"] = state->getMinVoltage();
  doc["maxCurrent"] = state->getMaxCurrent();
  doc["minCurrent"] = state->getMinCurrent();
  doc["maxPower"] = state->getMaxPower();
  doc["configMaxVoltage"] = state->getConfigMaxVoltage();
  doc["configMaxCurrent"] = state->getConfigMaxCurrent();
  String json;
  serializeJson(doc, json);
  request->send(HTTP_OK, "application/json", json);
}

// ---- GET /api/limits -------------------------------------------------------

/**
 * Java equivalent: RestService#getLimits
 */
void RestService::handleGetLimits(AsyncWebServerRequest* request) {
  Log_info("GET /api/limits");
  const ConverterState* state = _deviceService->getState();
  JsonDocument doc;
  doc["manufacturer"] = state->getManufacturer();
  doc["deviceName"] = state->getDeviceName();
  doc["minVoltage"] = state->getMinVoltage();
  doc["maxVoltage"] = state->getMaxVoltage();
  doc["minCurrent"] = state->getMinCurrent();
  doc["maxCurrent"] = state->getMaxCurrent();
  doc["maxPower"] = state->getMaxPower();
  String json;
  serializeJson(doc, json);
  request->send(HTTP_OK, "application/json", json);
}

// ---- GET /api/measurements -------------------------------------------------

/**
 * Java equivalent: RestService#getMeasurements
 */
void RestService::handleGetMeasurements(AsyncWebServerRequest* request) {
  Log_info("GET /api/measurements");
  const ConverterState* state = _deviceService->getState();
  JsonDocument doc;
  doc["voltage"] = state->getVoltageOut();
  doc["current"] = state->getCurrentOut();
  doc["power"] = state->getPowerOut();
  String json;
  serializeJson(doc, json);
  request->send(HTTP_OK, "application/json", json);
}

// ---- GET /api/voltage ------------------------------------------------------

/**
 * Java equivalent: RestService#getVoltage
 */
void RestService::handleGetVoltage(AsyncWebServerRequest* request) {
  Log_info("GET /api/voltage");
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
  Log_info("GET /api/current");
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
  Log_info("GET /api/power");
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
  Log_info("PUT /api/measurements");
  if (!_deviceService->isDeviceDetected()) {
    request->send(HTTP_SERVICE_UNAVAILABLE, "text/plain", "No device connected");
    return;
  }
  JsonDocument doc;
  DeserializationError deserializationError = deserializeJson(doc, data, len);
  if (deserializationError || !doc.containsKey("voltage") || !doc.containsKey("current")) {
    request->send(HTTP_BAD_REQUEST, "text/plain", "Missing or malformed 'voltage'/'current' field");
    return;
  }
  double voltage = doc["voltage"].as<double>();
  double amperes = doc["current"].as<double>();
  if (!_deviceService->setVoltageCurrent(voltage, amperes)) {
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
  Log_info("PUT /api/voltage");
  if (!_deviceService->isDeviceDetected()) {
    request->send(HTTP_SERVICE_UNAVAILABLE, "text/plain", "No device connected");
    return;
  }
  JsonDocument doc;
  DeserializationError deserializationError = deserializeJson(doc, data, len);
  if (deserializationError || !doc.containsKey("voltage")) {
    request->send(HTTP_BAD_REQUEST, "text/plain", "Missing or malformed 'voltage' field");
    return;
  }
  double voltage = doc["voltage"].as<double>();
  if (!_deviceService->setVoltage(voltage)) {
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
  Log_info("PUT /api/voltage/verified");
  if (!_deviceService->isDeviceDetected()) {
    request->send(HTTP_SERVICE_UNAVAILABLE, "text/plain", "No device connected");
    return;
  }
  JsonDocument doc;
  DeserializationError deserializationError = deserializeJson(doc, data, len);
  if (deserializationError || !doc.containsKey("voltage")) {
    request->send(HTTP_BAD_REQUEST, "text/plain", "Missing or malformed 'voltage' field");
    return;
  }
  double voltage = doc["voltage"].as<double>();
  double confirmed = 0.0;
  bool conflict = false;
  if (!_deviceService->setVoltageVerified(voltage, confirmed, conflict)) {
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
  Log_info("PUT /api/current");
  if (!_deviceService->isDeviceDetected()) {
    request->send(HTTP_SERVICE_UNAVAILABLE, "text/plain", "No device connected");
    return;
  }
  JsonDocument doc;
  DeserializationError deserializationError = deserializeJson(doc, data, len);
  if (deserializationError || !doc.containsKey("current")) {
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
  Log_info("PUT /api/current/verified");
  if (!_deviceService->isDeviceDetected()) {
    request->send(HTTP_SERVICE_UNAVAILABLE, "text/plain", "No device connected");
    return;
  }
  JsonDocument doc;
  DeserializationError deserializationError = deserializeJson(doc, data, len);
  if (deserializationError || !doc.containsKey("current")) {
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
  Log_info("PUT /api/output");
  if (!_deviceService->isDeviceDetected()) {
    request->send(HTTP_SERVICE_UNAVAILABLE, "text/plain", "No device connected");
    return;
  }
  JsonDocument doc;
  DeserializationError deserializationError = deserializeJson(doc, data, len);
  if (deserializationError || !doc.containsKey("outputEnable")) {
    request->send(HTTP_BAD_REQUEST, "text/plain", "Missing or malformed 'outputEnable' field");
    return;
  }
  bool outputEnabled = doc["outputEnable"].as<bool>();
  if (!_deviceService->setOutput(outputEnabled)) {
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
  Log_info("PUT /api/keypad");
  if (!_deviceService->isDeviceDetected()) {
    request->send(HTTP_SERVICE_UNAVAILABLE, "text/plain", "No device connected");
    return;
  }
  JsonDocument doc;
  DeserializationError deserializationError = deserializeJson(doc, data, len);
  if (deserializationError || !doc.containsKey("keypadLock")) {
    request->send(HTTP_BAD_REQUEST, "text/plain", "Missing or malformed 'keypadLock' field");
    return;
  }
  bool keypadLocked = doc["keypadLock"].as<bool>();
  if (!_deviceService->setKeypad(keypadLocked)) {
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
  Log_info("POST /api/protection/clear");
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
