#include "RidenRD50xx.h"

#include <Arduino.h>

#include "../../../src/SerialController/src/LogBuffer.h"
#include "../../modbus/src/ModbusConstants.h"

/**
 * RidenRD50xx.cpp - Driver for Ruideng DPS/RD50xx series DC/DC converters.
 *
 * Java equivalent: com.serial.devices.RidenRD50xx
 *
 * Different register layout from RD60xx:
 *   - 0x0000 = U-SET (voltage setpoint), not model ID.
 *   - 0x000B = MODEL (product number), detection register.
 *   - No temperature register - getTemperatureCelsius() returns -999.0.
 */

// ---- DeviceRegister descriptors ---------------------------------------------
// Java equivalent: public static final DeviceRegister ... in RidenRD50xx.java

static const DeviceRegister VSET("Voltage Setpoint", "V", RidenRegistersRD50xx::REG_VSET, 100.0);
static const DeviceRegister ISET("Current Setpoint", "A", RidenRegistersRD50xx::REG_ISET, 100.0);
static const DeviceRegister VOUT("Output Voltage", "V", RidenRegistersRD50xx::REG_VOUT, 100.0);
static const DeviceRegister IOUT("Output Current", "A", RidenRegistersRD50xx::REG_IOUT, 100.0);
static const DeviceRegister POUT("Output Power", "W", RidenRegistersRD50xx::REG_POUT, 100.0);
static const DeviceRegister VIN("Input Voltage", "V", RidenRegistersRD50xx::REG_VIN, 100.0);
static const DeviceRegister LOCK("Keypad Lock", nullptr, RidenRegistersRD50xx::REG_KEYPAD_LOCK);
static const DeviceRegister PROT("Protection", nullptr, RidenRegistersRD50xx::REG_PROTECTION_STATE);
static const DeviceRegister MODE("CC/CV Mode", nullptr, RidenRegistersRD50xx::REG_MODE);
static const DeviceRegister OUTP("Output Enable", nullptr, RidenRegistersRD50xx::REG_OUTPUT_ENABLE);
static const DeviceRegister DEVD("Device ID", nullptr, RidenRegistersRD50xx::REG_DEVICE_ID);
static const DeviceRegister FWVR("Firmware", nullptr, RidenRegistersRD50xx::REG_FIRMWARE, 10.0);

// ---- Device identification table -------------------------------------------
// Java equivalent: private static final Map<Integer, String> KNOWN_DEVICE_IDS

struct RD50xxDeviceIdEntry {
  int id;
  const char* name;
};

/**
 * Lookup table: 4-digit model code → retail model name.
 * Register 0x000B returns a short integer (e.g. 5020 for DPS5020).
 * Device name uses "DPS" prefix; manufacturer = "Ruideng".
 *
 * Java equivalent: RidenRD50xx.KNOWN_DEVICE_IDS
 */
static const RD50xxDeviceIdEntry KNOWN_DEVICE_IDS[] = {
    {5005, "DPS5005"},
    {5010, "DPS5010"},
    {5020, "DPS5020"},
};
static constexpr int KNOWN_DEVICE_IDS_COUNT =
    (int)(sizeof(KNOWN_DEVICE_IDS) / sizeof(KNOWN_DEVICE_IDS[0]));

static const char* lookupDeviceId(int id) {
  for (int i = 0; i < KNOWN_DEVICE_IDS_COUNT; i++) {
    if (KNOWN_DEVICE_IDS[i].id == id) {
      return KNOWN_DEVICE_IDS[i].name;
    }
  }
  return nullptr;
}

// ---- Constructor ------------------------------------------------------------

RidenRD50xx::RidenRD50xx(ModbusTransport* transport, uint8_t slave)
    : ModbusDevice(transport, slave) {
}

// ---- verifyDevicePresent ----------------------------------------------------

/**
 * Probes the model register (0x000B) and validates against KNOWN_DEVICE_IDS.
 * Sets manufacturer = "Ruideng" and device = model name on success.
 * Returns true if identified.
 *
 * Java equivalent: RidenRD50xx#verifyDevicePresent(List<Integer> bauds) inner probe body.
 */
bool RidenRD50xx::verifyDevicePresent() {
  int deviceId = 0;
  if (!readInt(DEVD, deviceId)) {
    return false;
  }

  Log_debug("Device ID register (0x000B) raw value: %d", deviceId);

  const char* modelName = lookupDeviceId(deviceId);
  if (modelName == nullptr) {
    return false;
  }

  manufacturer = "Ruideng";
  device = modelName;

  int fwRaw = 0;
  readInt(FWVR, fwRaw);
  // fw == 0 is normal on DPS5020 factory batches - not a read error.
  if (fwRaw == 0) {
    Log_info("Detected Ruideng DPS/RD50xx (Model: %s, FW: unknown) at slave %d.",
             modelName, slave);
  } else {
    Log_info("Detected Ruideng DPS/RD50xx (Model: %s, FW: v%.1f) at slave %d.",
             modelName, fwRaw / 10.0, slave);
  }
  return true;
}

// ---- DC2DCConverter interface - getDevice / getManufacturer -----------------

const char* RidenRD50xx::getDevice() {
  return ModbusDevice::getDevice();
}

const char* RidenRD50xx::getManufacturer() {
  return ModbusDevice::getManufacturer();
}

// ---- pollAll ----------------------------------------------------------------

/**
 * Reads the full register block (0x0000–0x000C, 13 registers) in a single
 * Modbus 0x03 frame and populates all poll-cache fields.
 *
 * Offset map (address − REG_VSET):
 *  [0] VSET  [1] ISET  [2] VOUT  [3] IOUT  [4] POUT  [5] VIN
 *  [6] LOCK  [7] PROT  [8] MODE  [9] OUTPUT [10] BACKLIGHT
 * [11] DEVICE_ID  [12] FIRMWARE
 *
 * Java equivalent: RidenRD50xx#pollAll
 */
bool RidenRD50xx::pollAll() {
  uint16_t r[13];
  if (!readBlock((uint16_t)RidenRegistersRD50xx::REG_VSET, 13, r)) {
    return false;
  }
  cacheVoltageSet = VSET.decode(r[0]);
  cacheCurrentSet = ISET.decode(r[1]);
  cacheVoltageOut = VOUT.decode(r[2]);
  cacheCurrentOut = IOUT.decode(r[3]);
  cachePowerOut = POUT.decode(r[4]);
  cacheVoltageIn = VIN.decode(r[5]);
  cacheLock = r[6];
  cacheProtection = r[7];
  cacheMode = r[8];
  cacheOutput = r[9];
  // r[10]: backlight - not used
  // r[11]: device ID - not used after detection
  cacheFirmwareRaw = r[12];
  return true;
}

// ---- Voltage ----------------------------------------------------------------

bool RidenRD50xx::setVoltage(double volts) {
  return write(VSET, volts);
}

double RidenRD50xx::getVoltage() {
  return cacheVoltageOut;
}

double RidenRD50xx::getVoltageSet() {
  return cacheVoltageSet;
}

double RidenRD50xx::getVoltageSetVerified() {
  double v = 0.0;
  read(VSET, v);
  return v;
}

// ---- Current ----------------------------------------------------------------

bool RidenRD50xx::setCurrent(double amperes) {
  return write(ISET, amperes);
}

double RidenRD50xx::getCurrent() {
  return cacheCurrentOut;
}

double RidenRD50xx::getCurrentSet() {
  return cacheCurrentSet;
}

double RidenRD50xx::getCurrentSetVerified() {
  double v = 0.0;
  read(ISET, v);
  return v;
}

// ---- Atomic voltage + current write -----------------------------------------

/**
 * Java equivalent: RidenRD50xx#setVoltageCurrent
 */
bool RidenRD50xx::setVoltageCurrent(double volts, double amperes) {
  uint16_t values[2];
  values[0] = (uint16_t)VSET.encode(volts);
  values[1] = (uint16_t)ISET.encode(amperes);
  return writeBlock((uint16_t)RidenRegistersRD50xx::REG_VSET, values, 2);
}

// ---- Power / input voltage --------------------------------------------------

double RidenRD50xx::getPower() {
  return cachePowerOut;
}

double RidenRD50xx::getInputVoltage() {
  return cacheVoltageIn;
}

// ---- Output enable ----------------------------------------------------------

bool RidenRD50xx::setOutput(bool outputEnabled) {
  return writeInt(OUTP, outputEnabled ? ModbusConstants::STATE_ON : ModbusConstants::STATE_OFF);
}

bool RidenRD50xx::getOutput() {
  return (cacheOutput == ModbusConstants::STATE_ON);
}

// ---- Temperature ------------------------------------------------------------

/**
 * The DPS50xx series does not expose a temperature register.
 * Returns -999.0 as an unambiguous sentinel (physically impossible reading).
 *
 * Java equivalent: RidenRD50xx#getTemperatureCelsius
 */
double RidenRD50xx::getTemperatureCelsius() {
  return -999.0;
}

// ---- Firmware ---------------------------------------------------------------

/**
 * Raw register / 10.0 = version (e.g. 17 = v1.7).
 * Several DPS5020 factory batches always return 0 - known hardware limitation.
 *
 * Java equivalent: RidenRD50xx#getFirmwareVersion
 */
int RidenRD50xx::getFirmwareVersion() {
  return cacheFirmwareRaw;
}

// ---- Protection state -------------------------------------------------------

bool RidenRD50xx::setProtectionState(bool on) {
  return writeInt(PROT, on ? ModbusConstants::STATE_ON : ModbusConstants::STATE_OFF);
}

bool RidenRD50xx::getProtectionState() {
  return (cacheProtection != ModbusConstants::STATE_OFF);
}

// ---- Keypad lock ------------------------------------------------------------

bool RidenRD50xx::setKeypad(bool locked) {
  return writeInt(LOCK, locked ? ModbusConstants::STATE_ON : ModbusConstants::STATE_OFF);
}

bool RidenRD50xx::getKeypad() {
  return (cacheLock == ModbusConstants::STATE_ON);
}

// ---- CV/CC mode -------------------------------------------------------------

/**
 * Register REG_MODE: 0 = CV, 1 = CC.
 * Java equivalent: RidenRD50xx#isCvMode
 */
bool RidenRD50xx::isCvMode() {
  return (cacheMode == 0);
}

// ---- Reconnect --------------------------------------------------------------

bool RidenRD50xx::reconnect() {
  return ModbusDevice::reconnect();
}
