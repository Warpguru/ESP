#pragma once

#include "../../device/src/SinilinkRegisters.h"
#include "../../device/src/base/ModbusDevice.h"
#include "ifc/DC2DCConverter.h"

/**
 * Sinilink.h - Device driver for Sinilink XY-series DC/DC converters.
 *
 * Java equivalent: com.serial.devices.Sinilink
 *
 * Wire connections — 4-pin TTL 3.3 V serial header on the underside of the control board:
 *   Black  (GND)  → adapter GND
 *   Green  (RxD)  → adapter TxD  (device receives)
 *   Yellow (TxD)  → adapter RxD  (device transmits)
 *   Red    (VCC)  → NC - do not connect
 */
class Sinilink : public ModbusDevice, public DC2DCConverter {
 public:
  Sinilink(ModbusTransport* transport, uint8_t slave = 1);

  /**
   * Probes the device by reading the product model register (0x0016) and
   * applying the three-step identification strategy (KNOWN_MODELS exact match →
   * packed 0x59xx REPORTED_MODELS fallback → unknown 0x59 warn-and-skip).
   * Sets manufacturer and device strings on success.
   * Returns true if the device was identified.
   *
   * Java equivalent: Sinilink#verifyDevicePresent(List<Integer> bauds) — the inner
   * probe body called once per baud rate. DeviceDetection drives the baud iteration.
   */
  bool verifyDevicePresent();

  // ---- DC2DCConverter interface -------------------------------------------

  const char* getDevice() override;
  const char* getManufacturer() override;
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
  // Poll cache — names match Java field names (see Sinilink.java cache* fields)
  // Block: 0x0000–0x0012 (19 registers)
  double cacheVoltageSet = 0.0;  // offset  0 VSET   ÷100
  double cacheCurrentSet = 0.0;  // offset  1 ISET   ÷1000
  double cacheVoltageOut = 0.0;  // offset  2 VOUT   ÷100
  double cacheCurrentOut = 0.0;  // offset  3 IOUT   ÷1000
  double cachePowerOut = 0.0;    // offset  4 POUT   ÷100
  double cacheVoltageIn = 0.0;   // offset  5 VIN    ÷100
  // offsets 6-12: AH/WH/timer counters — not used
  double cacheTemperature = 0.0;  // offset 13 TEMP   ÷10
  // offset 14: external temp — not used
  int cacheLock = 0;        // offset 15 LOCK   raw
  int cacheProtection = 0;  // offset 16 PROT   raw
  int cacheMode = 0;        // offset 17 MODE   raw
  int cacheOutput = 0;      // offset 18 OUTPUT raw
};
