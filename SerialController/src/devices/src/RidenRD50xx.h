#pragma once

#include "../../device/src/RidenRegistersRD50xx.h"
#include "../../device/src/base/ModbusDevice.h"
#include "ifc/DC2DCConverter.h"

/**
 * RidenRD50xx.h - Device driver for Ruideng DPS/RD50xx series DC/DC converters.
 *
 * Java equivalent: com.serial.devices.RidenRD50xx
 *
 * Default baud rate 9600 (differs from RD60xx which defaults to 115200).
 * No temperature register - getTemperatureCelsius() returns -999.0.
 *
 * Wire connections - 4-pin TTL 3.3 V serial header (front-panel cutout or rear connector):
 *   Black  (GND)  → adapter GND
 *   Blue   (RxD)  → adapter TxD  (device receives)
 *   Yellow (TxD)  → adapter RxD  (device transmits)
 *   Red    (VCC)  → NC - do not connect
 */
class RidenRD50xx : public ModbusDevice, public DC2DCConverter {
 public:
  RidenRD50xx(ModbusTransport* transport, uint8_t slave = 1);

  /**
   * Probes the device by reading the model register (0x000B) and validating
   * against KNOWN_DEVICE_IDS. Sets manufacturer and device strings on success.
   * Returns true if the device was identified.
   *
   * Java equivalent: RidenRD50xx#verifyDevicePresent(List<Integer> bauds) inner body.
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
  // Poll cache - names match Java field names (see RidenRD50xx.java cache* fields)
  // Block: 0x0000–0x000C (13 registers)
  double cacheVoltageSet = 0.0;  // offset  0 VSET      ÷100
  double cacheCurrentSet = 0.0;  // offset  1 ISET      ÷100
  double cacheVoltageOut = 0.0;  // offset  2 VOUT      ÷100
  double cacheCurrentOut = 0.0;  // offset  3 IOUT      ÷100
  double cachePowerOut = 0.0;    // offset  4 POUT      ÷100
  double cacheVoltageIn = 0.0;   // offset  5 VIN       ÷100
  int cacheLock = 0;             // offset  6 LOCK      raw
  int cacheProtection = 0;       // offset  7 PROTECT   raw
  int cacheMode = 0;             // offset  8 MODE      raw
  int cacheOutput = 0;           // offset  9 OUTPUT    raw
  // offset 10: backlight - not used
  // offset 11: device ID - not used after detection
  int cacheFirmwareRaw = 0;  // offset 12 FIRMWARE  raw (÷10 in getter)
};
