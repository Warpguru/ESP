#include "RestService.h"

/**
 * RestService.cpp - RESTful HTTP API for the converter.
 *
 * Java equivalent: com.serial.service.RestService
 */

RestService::RestService(AsyncWebServer* server, DeviceService* deviceService)
    : server(server), deviceService(deviceService) {
}

void RestService::registerRoutes() {
  // TODO: Port route registrations from com.serial.service.RestService
}

void RestService::handleGetStatus(AsyncWebServerRequest* request) {
  // TODO: Port from com.serial.service.RestService
}

void RestService::handleGetVoltage(AsyncWebServerRequest* request) {
  // TODO: Port from com.serial.service.RestService
}

void RestService::handleSetVoltage(AsyncWebServerRequest* request) {
  // TODO: Port from com.serial.service.RestService
}

void RestService::handleGetCurrent(AsyncWebServerRequest* request) {
  // TODO: Port from com.serial.service.RestService
}

void RestService::handleSetCurrent(AsyncWebServerRequest* request) {
  // TODO: Port from com.serial.service.RestService
}

void RestService::handleSetOutput(AsyncWebServerRequest* request) {
  // TODO: Port from com.serial.service.RestService
}

void RestService::handleRoot(AsyncWebServerRequest* request) {
  // TODO: Port from com.serial.service.RestService
}
