#include "Wuzhi.h"

/**
 * Wuzhi.cpp - Driver for Wuzhi ZK-series DC/DC converters.
 *
 * Java equivalent: com.serial.devices.Wuzhi
 */

Wuzhi::Wuzhi(ModbusTransport* transport, uint8_t slave)
    : ModbusDevice(transport, slave) {
}

const char* Wuzhi::getDevice() {
  return ModbusDevice::getDevice();
}

bool Wuzhi::setVoltage(double volts) {
  // TODO: Port from com.serial.devices.Wuzhi#setVoltage
  return false;
}

double Wuzhi::getVoltage() {
  return cacheVoltageOut;
}

bool Wuzhi::setCurrent(double amperes) {
  // TODO: Port from com.serial.devices.Wuzhi#setCurrent
  return false;
}

double Wuzhi::getCurrent() {
  return cacheCurrentOut;
}

double Wuzhi::getPower() {
  return cachePowerOut;
}

double Wuzhi::getInputVoltage() {
  return cacheVoltageIn;
}

bool Wuzhi::setVoltageCurrent(double volts, double amperes) {
  // TODO: Port from com.serial.devices.Wuzhi#setVoltageCurrent
  return false;
}

bool Wuzhi::setOutput(bool outputEnabled) {
  // TODO: Port from com.serial.devices.Wuzhi#setOutput
  return false;
}

bool Wuzhi::getOutput() {
  return cacheOutput != 0;
}

double Wuzhi::getTemperatureCelsius() {
  return cacheTemperature;
}

int Wuzhi::getFirmwareVersion() {
  return cacheFirmwareRaw;
}

bool Wuzhi::setProtectionState(bool on) {
  // TODO: Port from com.serial.devices.Wuzhi#setProtectionState
  return false;
}

bool Wuzhi::getProtectionState() {
  return cacheProtection != 0;
}

bool Wuzhi::setKeypad(bool locked) {
  // TODO: Port from com.serial.devices.Wuzhi#setKeypad
  return false;
}

bool Wuzhi::getKeypad() {
  return cacheLock != 0;
}

bool Wuzhi::isCvMode() {
  return cacheMode != 0;
}

double Wuzhi::getVoltageSet() {
  return cacheVoltageSet;
}

double Wuzhi::getVoltageSetVerified() {
  // TODO: Port from com.serial.devices.Wuzhi#getVoltageSetVerified
  return 0.0;
}

double Wuzhi::getCurrentSet() {
  return cacheCurrentSet;
}

double Wuzhi::getCurrentSetVerified() {
  // TODO: Port from com.serial.devices.Wuzhi#getCurrentSetVerified
  return 0.0;
}

bool Wuzhi::pollAll() {
  // TODO: Port from com.serial.devices.Wuzhi#pollAll
  return false;
}

bool Wuzhi::reconnect() {
  return ModbusDevice::reconnect();
}
