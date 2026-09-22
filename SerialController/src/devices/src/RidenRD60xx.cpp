#include "RidenRD60xx.h"

/**
 * RidenRD60xx.cpp - Driver for Riden RD60xx series DC/DC converters.
 *
 * Java equivalent: com.serial.devices.RidenRD60xx
 */

RidenRD60xx::RidenRD60xx(ModbusTransport* transport, uint8_t slave)
    : ModbusDevice(transport, slave) {
}

const char* RidenRD60xx::getDevice() {
  return ModbusDevice::getDevice();
}

bool RidenRD60xx::setVoltage(double volts) {
  // TODO: Port from com.serial.devices.RidenRD60xx#setVoltage
  return false;
}

double RidenRD60xx::getVoltage() {
  return cacheVoltageOut;
}

bool RidenRD60xx::setCurrent(double amperes) {
  // TODO: Port from com.serial.devices.RidenRD60xx#setCurrent
  return false;
}

double RidenRD60xx::getCurrent() {
  return cacheCurrentOut;
}

double RidenRD60xx::getPower() {
  return cachePowerOut;
}

double RidenRD60xx::getInputVoltage() {
  return cacheVoltageIn;
}

bool RidenRD60xx::setVoltageCurrent(double volts, double amperes) {
  // TODO: Port from com.serial.devices.RidenRD60xx#setVoltageCurrent
  return false;
}

bool RidenRD60xx::setOutput(bool outputEnabled) {
  // TODO: Port from com.serial.devices.RidenRD60xx#setOutput
  return false;
}

bool RidenRD60xx::getOutput() {
  return cacheOutput != 0;
}

double RidenRD60xx::getTemperatureCelsius() {
  return cacheTemperature;
}

int RidenRD60xx::getFirmwareVersion() {
  return cacheFirmwareRaw;
}

bool RidenRD60xx::setProtectionState(bool on) {
  // TODO: Port from com.serial.devices.RidenRD60xx#setProtectionState
  return false;
}

bool RidenRD60xx::getProtectionState() {
  return cacheProtection != 0;
}

bool RidenRD60xx::setKeypad(bool locked) {
  // TODO: Port from com.serial.devices.RidenRD60xx#setKeypad
  return false;
}

bool RidenRD60xx::getKeypad() {
  return cacheLock != 0;
}

bool RidenRD60xx::isCvMode() {
  return cacheMode != 0;
}

double RidenRD60xx::getVoltageSet() {
  return cacheVoltageSet;
}

double RidenRD60xx::getVoltageSetVerified() {
  // TODO: Port from com.serial.devices.RidenRD60xx#getVoltageSetVerified
  return 0.0;
}

double RidenRD60xx::getCurrentSet() {
  return cacheCurrentSet;
}

double RidenRD60xx::getCurrentSetVerified() {
  // TODO: Port from com.serial.devices.RidenRD60xx#getCurrentSetVerified
  return 0.0;
}

bool RidenRD60xx::pollAll() {
  // TODO: Port from com.serial.devices.RidenRD60xx#pollAll
  return false;
}

bool RidenRD60xx::reconnect() {
  return ModbusDevice::reconnect();
}
