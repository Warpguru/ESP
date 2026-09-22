#include "RidenRD50xx.h"

/**
 * RidenRD50xx.cpp - Driver for Riden DPS/RD50xx series DC/DC converters.
 *
 * Java equivalent: com.serial.devices.RidenRD50xx
 */

RidenRD50xx::RidenRD50xx(ModbusTransport* transport, uint8_t slave)
    : ModbusDevice(transport, slave) {
}

const char* RidenRD50xx::getDevice() {
  return ModbusDevice::getDevice();
}

bool RidenRD50xx::setVoltage(double volts) {
  // TODO: Port from com.serial.devices.RidenRD50xx#setVoltage
  return false;
}

double RidenRD50xx::getVoltage() {
  return cacheVoltageOut;
}

bool RidenRD50xx::setCurrent(double amperes) {
  // TODO: Port from com.serial.devices.RidenRD50xx#setCurrent
  return false;
}

double RidenRD50xx::getCurrent() {
  return cacheCurrentOut;
}

double RidenRD50xx::getPower() {
  return cachePowerOut;
}

double RidenRD50xx::getInputVoltage() {
  return cacheVoltageIn;
}

bool RidenRD50xx::setVoltageCurrent(double volts, double amperes) {
  // TODO: Port from com.serial.devices.RidenRD50xx#setVoltageCurrent
  return false;
}

bool RidenRD50xx::setOutput(bool outputEnabled) {
  // TODO: Port from com.serial.devices.RidenRD50xx#setOutput
  return false;
}

bool RidenRD50xx::getOutput() {
  return cacheOutput != 0;
}

double RidenRD50xx::getTemperatureCelsius() {
  return cacheTemperature;
}

int RidenRD50xx::getFirmwareVersion() {
  return cacheFirmwareRaw;
}

bool RidenRD50xx::setProtectionState(bool on) {
  // TODO: Port from com.serial.devices.RidenRD50xx#setProtectionState
  return false;
}

bool RidenRD50xx::getProtectionState() {
  return cacheProtection != 0;
}

bool RidenRD50xx::setKeypad(bool locked) {
  // TODO: Port from com.serial.devices.RidenRD50xx#setKeypad
  return false;
}

bool RidenRD50xx::getKeypad() {
  return cacheLock != 0;
}

bool RidenRD50xx::isCvMode() {
  return cacheMode != 0;
}

double RidenRD50xx::getVoltageSet() {
  return cacheVoltageSet;
}

double RidenRD50xx::getVoltageSetVerified() {
  // TODO: Port from com.serial.devices.RidenRD50xx#getVoltageSetVerified
  return 0.0;
}

double RidenRD50xx::getCurrentSet() {
  return cacheCurrentSet;
}

double RidenRD50xx::getCurrentSetVerified() {
  // TODO: Port from com.serial.devices.RidenRD50xx#getCurrentSetVerified
  return 0.0;
}

bool RidenRD50xx::pollAll() {
  // TODO: Port from com.serial.devices.RidenRD50xx#pollAll
  return false;
}

bool RidenRD50xx::reconnect() {
  return ModbusDevice::reconnect();
}
