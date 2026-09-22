#include "DeviceService.h"

#include <Arduino.h>

/**
 * DeviceService.cpp - Service layer for device detection, polling and control.
 *
 * Java equivalent: com.serial.service.DeviceService
 */

DeviceService::DeviceService(ConverterState* state, DC2DCConverter* converter)
    : state(state), converter(converter) {
}

void DeviceService::begin() {
  // TODO: Port from com.serial.service.DeviceService
  // Start polling FreeRTOS task on Core 0
  xTaskCreatePinnedToCore(pollingTask, "DeviceService_Poll", 4096, this, 2, NULL, 0);
}

bool DeviceService::setVoltage(double volts) {
  // TODO: Port from com.serial.service.DeviceService#setVoltage (with BUCK ceiling)
  return false;
}

bool DeviceService::setCurrent(double amperes) {
  // TODO: Port from com.serial.service.DeviceService#setCurrent
  return false;
}

bool DeviceService::setVoltageCurrent(double volts, double amperes) {
  // TODO: Port from com.serial.service.DeviceService#setVoltageCurrent
  return false;
}

bool DeviceService::setOutput(bool on) {
  // TODO: Port from com.serial.service.DeviceService#setOutput
  return false;
}

const ConverterState* DeviceService::getState() const {
  return state;
}

void DeviceService::pollingTask(void* param) {
  // TODO: Port polling loop from com.serial.service.DeviceService
  DeviceService* self = static_cast<DeviceService*>(param);
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
