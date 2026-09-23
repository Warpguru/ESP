#pragma once

#include "../../device/src/WuzhiRegisters.h"
#include "../../device/src/base/ModbusDevice.h"
#include "ifc/DC2DCConverter.h"

/**
 * Wuzhi.h - Device driver for Wuzhi ZK-series DC/DC converters.
 *
 * Java equivalent: com.serial.devices.Wuzhi
 *
 * The Wuzhi ZK-series uses a register map identical to Sinilink XY-series.
 * The critical difference: ISET/IOUT use scale 100 (not 1000 as on XY6008/XY6014).
 *
 * Wire connections — 4-pin XH2.54-4P TTL 3.3 V serial header:
 *   Pin 1 (GND)  → adapter GND
 *   Pin 2 (RxD)  → adapter TxD  (device receives)
 *   Pin 3 (TxD)  → adapter RxD  (device transmits)
 *   Pin 4 (VCC)  → NC - do not connect
 */
class Wuzhi : public ModbusDevice, public DC2DCConverter {
 public:
  Wuzhi(ModbusTransport* transport, uint8_t slave = 1);

  /**
   * Probes the device by reading the product model register (0x0016) and
   * applying the three-step identification strategy (KNOWN_MODELS exact match →
   * packed 0x59xx REPORTED_MODELS fallback → unknown 0x59 warn-and-skip).
   * Sets manufacturer and device strings on success.
   * Returns true if the device was identified.
   *
   * Java equivalent: Wuzhi#verifyDevicePresent(List<Integer> bauds) inner body.
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
  // Poll cache — names match Java field names (see Wuzhi.java cache* fields)
  // Block: 0x0000–0x0012 (19 registers), same layout as Sinilink
  double cacheVoltageSet = 0.0;  // offset  0 VSET   ÷100
  double cacheCurrentSet = 0.0;  // offset  1 ISET   ÷100  ← scale 100!
  double cacheVoltageOut = 0.0;  // offset  2 VOUT   ÷100
  double cacheCurrentOut = 0.0;  // offset  3 IOUT   ÷100  ← scale 100!
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
