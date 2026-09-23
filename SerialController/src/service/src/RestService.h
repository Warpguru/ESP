#pragma once

#include <ESPAsyncWebServer.h>

#include "DeviceService.h"

/**
 * RestService.h - RESTful HTTP API for the converter.
 *
 * Java equivalent: com.serial.service.RestService
 *
 * Registers all /api/* routes on the provided AsyncWebServer instance.
 * All reads go through DeviceService#getState(); all writes delegate to
 * DeviceService validated write methods — matching the Java layering exactly.
 *
 * Route table (matches Java RestService#registerRoutes):
 *   GET  /api/state
 *   GET  /api/limits
 *   GET  /api/measurements
 *   GET  /api/voltage
 *   GET  /api/current
 *   GET  /api/power
 *   PUT  /api/measurements
 *   PUT  /api/voltage
 *   PUT  /api/voltage/verified
 *   PUT  /api/current
 *   PUT  /api/current/verified
 *   PUT  /api/output
 *   PUT  /api/keypad
 *   POST /api/protection/clear
 */
class RestService {
 public:
  /**
   * Constructs the REST service.
   *
   * @param server        AsyncWebServer instance (owned externally)
   * @param deviceService service layer (owned externally)
   *
   * Java equivalent: RestService constructor
   */
  RestService(AsyncWebServer* server, DeviceService* deviceService);

  /**
   * Registers all /api/* HTTP routes on the server.
   * Must be called before server.begin().
   *
   * Java equivalent: RestService#registerRoutes
   */
  void registerRoutes();

 private:
  AsyncWebServer* _server;
  DeviceService* _deviceService;

  // ---- GET handlers (no body) ----------------------------------------------
  void handleGetState(AsyncWebServerRequest* request);
  void handleGetLimits(AsyncWebServerRequest* request);
  void handleGetMeasurements(AsyncWebServerRequest* request);
  void handleGetVoltage(AsyncWebServerRequest* request);
  void handleGetCurrent(AsyncWebServerRequest* request);
  void handleGetPower(AsyncWebServerRequest* request);

  // ---- PUT/POST handlers (JSON body) ---------------------------------------
  void handlePutMeasurements(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
  void handlePutVoltage(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
  void handlePutVoltageVerified(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
  void handlePutCurrent(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
  void handlePutCurrentVerified(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
  void handlePutOutput(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
  void handlePutKeypad(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
  void handlePostProtectionClear(AsyncWebServerRequest* request);
};
