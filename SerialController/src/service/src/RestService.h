#pragma once

#include <ESPAsyncWebServer.h>

#include "DeviceService.h"

/**
 * RestService.h - RESTful HTTP API for the converter.
 *
 * Java equivalent: com.serial.service.RestService
 *
 * Registers all HTTP routes on the provided AsyncWebServer instance.
 * Reads converter state from DeviceService and delegates setpoint writes
 * back to it.
 */
class RestService {
 public:
  /**
   * Constructs the REST service.
   *
   * @param server        AsyncWebServer instance (owned externally)
   * @param deviceService service layer (owned externally)
   */
  RestService(AsyncWebServer* server, DeviceService* deviceService);

  /**
   * Registers all HTTP routes on the server.
   * Must be called before server.begin().
   */
  void registerRoutes();

 private:
  AsyncWebServer* server;
  DeviceService* deviceService;

  // Route handlers
  void handleGetStatus(AsyncWebServerRequest* request);
  void handleGetVoltage(AsyncWebServerRequest* request);
  void handleSetVoltage(AsyncWebServerRequest* request);
  void handleGetCurrent(AsyncWebServerRequest* request);
  void handleSetCurrent(AsyncWebServerRequest* request);
  void handleSetOutput(AsyncWebServerRequest* request);
  void handleRoot(AsyncWebServerRequest* request);
};
