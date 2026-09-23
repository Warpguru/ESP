#pragma once

#include "../../device/src/RidenRegistersRD60xx.h"
#include "../../device/src/base/DeviceRegister.h"
#include "../../device/src/base/ModbusDevice.h"
#include "ifc/DC2DCConverter.h"

/**
 * RidenRD60xx.h - Device driver for Riden RD60xx series DC/DC converters.
 *
 * Java equivalent: com.serial.devices.RidenRD60xx
 *
 * Wire connections - 4-pin TTL 3.3 V serial header on the back of the display board:
 *   Pin 1 - Black (GND) → adapter GND
 *   Pin 2 - White (RxD) → adapter TxD  (device receives)
 *   Pin 3 - Green (TxD) → adapter RxD  (device transmits)
 *   Pin 4 - Red   (VCC) → NC - do not connect
 */
class RidenRD60xx : public ModbusDevice, public DC2DCConverter {
 public:
  // ---- DeviceRegister descriptors ----------------------------------------
  // Java equivalent: public static final DeviceRegister ... in RidenRD60xx.java
  // Block: 0x0000–0x0012 (19 registers) read by pollAll() in one 0x03 frame.

  static const DeviceRegister DEVICE_ID;         // offset  0 — model ID, raw
  static const DeviceRegister FIRMWARE_VERSION;  // offset  3 — firmware, scale 100
  static const DeviceRegister TEMP_CELSIUS;      // offset  5 — temperature °C, scale 1
  static const DeviceRegister VSET;              // offset  8 — voltage setpoint V, scale 100
  static const DeviceRegister ISET;              // offset  9 — current setpoint A, scale 1000
  static const DeviceRegister VOUT;              // offset 10 — output voltage V, scale 100
  static const DeviceRegister IOUT;              // offset 11 — output current A, scale 1000
  static const DeviceRegister AH;                // offset 12 — amp-hours, raw
  static const DeviceRegister POUT;              // offset 13 — output power W, scale 100
  static const DeviceRegister VIN;               // offset 14 — input voltage V, scale 100
  static const DeviceRegister LOCK;              // offset 15 — keypad lock, raw
  static const DeviceRegister PROTECTION_STATE;  // offset 16 — protection status, raw
  static const DeviceRegister MODE;              // offset 17 — CV/CC mode, raw
  static const DeviceRegister OUTPUT_ENABLE;     // offset 18 — output on/off, raw
  static const DeviceRegister PRESET;            // preset selector, raw
  static const DeviceRegister IRANGE;            // current range, raw

  RidenRD60xx(ModbusTransport* transport, uint8_t slave = 1);

  /**
   * Probes the device by reading the model ID register (0x0000) and validating
   * against KNOWN_DEVICE_IDS. Sets manufacturer and device strings on success.
   * Returns true if the device was identified.
   *
   * Java equivalent: RidenRD60xx#verifyDevicePresent(List<Integer> bauds) inner body.
   */
  bool verifyDevicePresent();

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
  // Poll cache — populated by pollAll(), returned by all getters.
  // Block: 0x0000–0x0012 (19 registers). Offset map:
  //  [0]  DEVICE_ID   [1]  SERIAL_HIGH [2]  SERIAL_LOW   [3]  FIRMWARE
  //  [4]  TEMP_SIGN_C [5]  TEMP_C      [6]  TEMP_SIGN_F  [7]  TEMP_F
  //  [8]  VSET        [9]  ISET        [10] VOUT         [11] IOUT
  // [12]  AH          [13] POUT        [14] VIN          [15] LOCK
  // [16]  PROTECTION  [17] MODE        [18] OUTPUT
  //
  // Java equivalent: private volatile fields in RidenRD60xx.java
  int cacheDeviceId = 0;            // offset  0
  int cacheSerialHigh = 0;          // offset  1
  int cacheSerialLow = 0;           // offset  2
  int cacheFirmwareRaw = 0;         // offset  3
  int cacheTempSignCelsius = 0;     // offset  4
  double cacheTemperature = 0.0;    // offset  5
  int cacheTempSignFahrenheit = 0;  // offset  6
  int cacheTemperatureFahr = 0;     // offset  7
  double cacheVoltageSet = 0.0;     // offset  8
  double cacheCurrentSet = 0.0;     // offset  9
  double cacheVoltageOut = 0.0;     // offset 10
  double cacheCurrentOut = 0.0;     // offset 11
  int cacheAh = 0;                  // offset 12
  double cachePowerOut = 0.0;       // offset 13
  double cacheVoltageIn = 0.0;      // offset 14
  int cacheLock = 0;                // offset 15
  int cacheProtection = 0;          // offset 16
  int cacheMode = 0;                // offset 17
  int cacheOutput = 0;              // offset 18
};
