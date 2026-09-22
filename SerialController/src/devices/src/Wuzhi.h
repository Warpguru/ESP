#pragma once

#include "../../device/src/WuzhiRegisters.h"
#include "../../device/src/base/ModbusDevice.h"
#include "ifc/DC2DCConverter.h"

/**
 * Wuzhi.h - Device driver for Wuzhi ZK-series DC/DC converters.
 *
 * Java equivalent: com.serial.devices.Wuzhi
 */
class Wuzhi : public ModbusDevice, public DC2DCConverter {
 public:
  Wuzhi(ModbusTransport* transport, uint8_t slave = 1);

  const char* getDevice() override;
  bool setVoltage(double volts) override;
  double getVoltage() override;
  bool setCurrent(double amperes) override;
  double getCurrent() override;
  double getPower() override;
  double getInputVoltage() override;
  bool setVoltageCurrent(double volts, double amperes) override;
  bool setOutput(bool outputEnabled) override;
  bool getOutput() override;
  double getTemperatureCelsius() override;
  int getFirmwareVersion() override;
  bool setProtectionState(bool on) override;
  bool getProtectionState() override;
  bool setKeypad(bool locked) override;
  bool getKeypad() override;
  bool isCvMode() override;
  double getVoltageSet() override;
  double getVoltageSetVerified() override;
  double getCurrentSet() override;
  double getCurrentSetVerified() override;
  bool pollAll() override;
  bool reconnect() override;

 private:
  // Poll cache — names match Java field names (see Wuzhi.java cache* fields)
  double cacheVoltageOut = 0.0;
  double cacheCurrentOut = 0.0;
  double cachePowerOut = 0.0;
  double cacheVoltageIn = 0.0;
  double cacheTemperature = 0.0;
  double cacheVoltageSet = 0.0;
  double cacheCurrentSet = 0.0;
  int cacheOutput = 0;
  int cacheMode = 0;
  int cacheProtection = 0;
  int cacheLock = 0;
  int cacheFirmwareRaw = 0;
};
