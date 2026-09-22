#include "Sinilink.h"

/**
 * Sinilink.cpp - Driver for Sinilink XY-series DC/DC converters.
 *
 * Java equivalent: com.serial.devices.Sinilink
 */

Sinilink::Sinilink(ModbusTransport* transport, uint8_t slave)
    : ModbusDevice(transport, slave) {
}

const char* Sinilink::getDevice() {
  return ModbusDevice::getDevice();
}

bool Sinilink::setVoltage(double volts) {
  // TODO: Port from com.serial.devices.Sinilink#setVoltage
  return false;
}

double Sinilink::getVoltage() {
  return cacheVoltageOut;
}

bool Sinilink::setCurrent(double amperes) {
  // TODO: Port from com.serial.devices.Sinilink#setCurrent
  return false;
}

double Sinilink::getCurrent() {
  return cacheCurrentOut;
}

double Sinilink::getPower() {
  return cachePowerOut;
}

double Sinilink::getInputVoltage() {
  return cacheVoltageIn;
}

bool Sinilink::setVoltageCurrent(double volts, double amperes) {
  // TODO: Port from com.serial.devices.Sinilink#setVoltageCurrent
  return false;
}

bool Sinilink::setOutput(bool outputEnabled) {
  // TODO: Port from com.serial.devices.Sinilink#setOutput
  return false;
}

bool Sinilink::getOutput() {
  return cacheOutput != 0;
}

double Sinilink::getTemperatureCelsius() {
  return cacheTemperature;
}

int Sinilink::getFirmwareVersion() {
  return cacheFirmwareRaw;
}

bool Sinilink::setProtectionState(bool on) {
  // TODO: Port from com.serial.devices.Sinilink#setProtectionState
  return false;
}

bool Sinilink::getProtectionState() {
  return cacheProtection != 0;
}

bool Sinilink::setKeypad(bool locked) {
  // TODO: Port from com.serial.devices.Sinilink#setKeypad
  return false;
}

bool Sinilink::getKeypad() {
  return cacheLock != 0;
}

bool Sinilink::isCvMode() {
  return cacheMode != 0;
}

double Sinilink::getVoltageSet() {
  return cacheVoltageSet;
}

double Sinilink::getVoltageSetVerified() {
  // TODO: Port from com.serial.devices.Sinilink#getVoltageSetVerified
  return 0.0;
}

double Sinilink::getCurrentSet() {
  return cacheCurrentSet;
}

double Sinilink::getCurrentSetVerified() {
  // TODO: Port from com.serial.devices.Sinilink#getCurrentSetVerified
  return 0.0;
}

bool Sinilink::pollAll() {
  // TODO: Port from com.serial.devices.Sinilink#pollAll
  return false;
}

bool Sinilink::reconnect() {
  return ModbusDevice::reconnect();
}
