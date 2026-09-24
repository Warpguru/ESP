#include "RidenRD60xx.h"

#include <Arduino.h>

#include "../../../src/SerialController/src/LogBuffer.h"
#include "../../modbus/src/ModbusConstants.h"

/**
 * RidenRD60xx.cpp - Driver for Riden RD60xx series DC/DC converters.
 *
 * Java equivalent: com.serial.devices.RidenRD60xx
 */

// ---- Static DeviceRegister descriptors -------------------------------------
// Java equivalent: public static final DeviceRegister fields in RidenRD60xx.java

const DeviceRegister RidenRD60xx::DEVICE_ID("Model Identification", nullptr, RidenRegistersRD60xx::REG_DEVICE_ID);
const DeviceRegister RidenRD60xx::FIRMWARE_VERSION("Firmware Version", nullptr, RidenRegistersRD60xx::REG_FIRMWARE, 100);
const DeviceRegister RidenRD60xx::TEMP_CELSIUS("Temperature Celsius", "°C", RidenRegistersRD60xx::REG_TEMP_CELSIUS);
const DeviceRegister RidenRD60xx::VSET("Voltage Setpoint", "V", RidenRegistersRD60xx::REG_VSET, 100);
const DeviceRegister RidenRD60xx::ISET("Current Setpoint", "A", RidenRegistersRD60xx::REG_ISET, 1000);
const DeviceRegister RidenRD60xx::VOUT("Output Voltage", "V", RidenRegistersRD60xx::REG_VOUT, 100);
const DeviceRegister RidenRD60xx::IOUT("Output Current", "A", RidenRegistersRD60xx::REG_IOUT, 1000);
const DeviceRegister RidenRD60xx::AH("Accumulated Amperehours", "Ah", RidenRegistersRD60xx::REG_AH);
const DeviceRegister RidenRD60xx::POUT("Output Power", "W", RidenRegistersRD60xx::REG_POUT, 100);
const DeviceRegister RidenRD60xx::VIN("Voltage Input", "V", RidenRegistersRD60xx::REG_VIN, 100);
const DeviceRegister RidenRD60xx::LOCK("Keypad Lock", nullptr, RidenRegistersRD60xx::REG_KEYPAD_LOCK);
const DeviceRegister RidenRD60xx::PROTECTION_STATE("Protection Status", nullptr, RidenRegistersRD60xx::REG_PROTECTION_STATE);
const DeviceRegister RidenRD60xx::MODE("CC/CV Mode", nullptr, RidenRegistersRD60xx::REG_MODE);
const DeviceRegister RidenRD60xx::OUTPUT_ENABLE("Output Enable", nullptr, RidenRegistersRD60xx::REG_OUTPUT_ENABLE);
const DeviceRegister RidenRD60xx::PRESET("Preset Selector", "Mx", RidenRegistersRD60xx::REG_PRESET);
const DeviceRegister RidenRD60xx::IRANGE("Current Range", "A", RidenRegistersRD60xx::REG_CURRENT_RANGE);

// ---- Lookup map from raw 5-digit model ID to retail model name -------------
// Java equivalent: private static final Map<Integer, String> KNOWN_DEVICE_IDS

struct DeviceIdEntry {
  int id;
  const char* name;
};

static const DeviceIdEntry KNOWN_DEVICE_IDS[] = {
    {60060, "RD6006"},
    {60061, "RD6006"},
    {60062, "RD6006"},
    {60063, "RD6006"},
    {60064, "RD6006"},
    {60065, "RD6006P"},
    {60066, "RK6006"},
    {60120, "RD6012"},
    {60121, "RD6012"},
    {60122, "RD6012"},
    {60123, "RD6012"},
    {60124, "RD6012"},
    {60125, "RD6012P"},
    {60180, "RD6018"},
    {60181, "RD6018"},
    {60182, "RD6018"},
    {60183, "RD6018"},
    {60184, "RD6018"},
    {60240, "RD6024"},
    {60241, "RD6024"},
    {60242, "RD6024"},
    {60243, "RD6024"},
    {60244, "RD6024"},
    {60300, "RD6030"},
    {60301, "RD6030"},
    {60302, "RD6030"},
    {60303, "RD6030"},
    {60304, "RD6030"},
};

static constexpr int KNOWN_DEVICE_IDS_COUNT =
    (int)(sizeof(KNOWN_DEVICE_IDS) / sizeof(KNOWN_DEVICE_IDS[0]));

/**
 * Looks up a raw device ID (0x0000 register value) in KNOWN_DEVICE_IDS.
 * Returns the model name string, or nullptr if not recognised.
 *
 * Java equivalent: KNOWN_DEVICE_IDS.get(deviceId)
 */
static const char* lookupDeviceId(int id) {
  for (int deviceIndex = 0; deviceIndex < KNOWN_DEVICE_IDS_COUNT; deviceIndex++) {
    if (KNOWN_DEVICE_IDS[deviceIndex].id == id) {
      return KNOWN_DEVICE_IDS[deviceIndex].name;
    }
  }
  return nullptr;
}

// ---- Constructor ------------------------------------------------------------

RidenRD60xx::RidenRD60xx(ModbusTransport* transport, uint8_t slave)
    : ModbusDevice(transport, slave) {
}

// ---- verifyDevicePresent ----------------------------------------------------

/**
 * Reads the model ID register (0x0000) and validates against KNOWN_DEVICE_IDS.
 * Sets manufacturer = "Riden" and device = model name on success.
 * Returns true if identified.
 *
 * Java equivalent: RidenRD60xx#verifyDevicePresent(List<Integer> bauds) inner probe body.
 */
bool RidenRD60xx::verifyDevicePresent() {
  uint16_t rawId = 0;
  if (!transport->readRegister(slave, RidenRegistersRD60xx::REG_DEVICE_ID, rawId)) {
    return false;
  }

  int deviceId = (int)rawId;
  Log_debug("Device ID register (0x0000) raw value: %d", deviceId);

  const char* modelName = lookupDeviceId(deviceId);
  if (modelName == nullptr) {
    return false;
  }

  manufacturer = "Riden";
  device = modelName;

  Log_info("Detected Riden RD60xx (Model: %s, ID: %d) at slave %d.",
           modelName, deviceId, slave);
  return true;
}

// ---- DC2DCConverter interface - getDevice ----------------------------------

/**
 * Returns the detected model name string (e.g. "RD6006"), or nullptr if not
 * yet detected. Delegates to the ModbusDevice base class field.
 *
 * Java equivalent: RidenRD60xx inherits getDevice() from ModbusDevice
 */
const char* RidenRD60xx::getDevice() {
  return ModbusDevice::getDevice();
}

/**
 * Returns the detected manufacturer string (e.g. "Riden"), or nullptr if not
 * yet detected. Delegates to the ModbusDevice base class field.
 *
 * Java equivalent: ModbusDevice#getManufacturer (inherited by RidenRD60xx)
 */
const char* RidenRD60xx::getManufacturer() {
  return ModbusDevice::getManufacturer();
}

// ---- pollAll ----------------------------------------------------------------

/**
 * Reads the full register block (0x0000–0x0012, 19 registers) in a single
 * Modbus 0x03 frame and populates all poll-cache fields.
 *
 * Offset map (address − REG_DEVICE_ID):
 *  [0]  DEVICE_ID   [1]  SERIAL_HIGH [2]  SERIAL_LOW   [3]  FIRMWARE
 *  [4]  TEMP_SIGN_C [5]  TEMP_C      [6]  TEMP_SIGN_F  [7]  TEMP_F
 *  [8]  VSET        [9]  ISET        [10] VOUT         [11] IOUT
 * [12]  AH          [13] POUT        [14] VIN          [15] LOCK
 * [16]  PROTECTION  [17] MODE        [18] OUTPUT
 *
 * On first successful poll, sets manufacturer and device strings from the
 * model ID in register 0x0000.
 *
 * Java equivalent: RidenRD60xx#pollAll
 * Deviation: bool return instead of void / throws.
 */
bool RidenRD60xx::pollAll() {
  uint16_t r[19];
  if (!readBlock(RidenRegistersRD60xx::REG_DEVICE_ID, 19, r)) {
    return false;
  }

  cacheDeviceId = r[0];
  cacheSerialHigh = r[1];
  cacheSerialLow = r[2];
  cacheFirmwareRaw = r[3];
  cacheTempSignCelsius = r[4];
  cacheTemperature = TEMP_CELSIUS.decode(r[5]);
  cacheTempSignFahrenheit = r[6];
  cacheTemperatureFahr = r[7];
  cacheVoltageSet = VSET.decode(r[8]);
  cacheCurrentSet = ISET.decode(r[9]);
  cacheVoltageOut = VOUT.decode(r[10]);
  cacheCurrentOut = IOUT.decode(r[11]);
  cacheAh = r[12];
  cachePowerOut = POUT.decode(r[13]);
  cacheVoltageIn = VIN.decode(r[14]);
  cacheLock = r[15];
  cacheProtection = r[16];
  cacheMode = r[17];
  cacheOutput = r[18];

  // On first successful poll, identify the device from register 0x0000.
  if (device == nullptr) {
    const char* modelName = lookupDeviceId(cacheDeviceId);
    if (modelName != nullptr) {
      manufacturer = "Riden";
      device = modelName;
      Log_info("Detected Riden RD60xx (Model: %s, ID: %d, FW: %d) at slave %d.",
               device, cacheDeviceId, cacheFirmwareRaw, slave);
    }
  }

  return true;
}

// ---- Voltage setters / getters ---------------------------------------------

/**
 * Java equivalent: RidenRD60xx#setVoltage
 */
bool RidenRD60xx::setVoltage(double volts) {
  return write(VSET, volts);
}

/**
 * Java equivalent: RidenRD60xx#getVoltage
 */
double RidenRD60xx::getVoltage() {
  return cacheVoltageOut;
}

/**
 * Java equivalent: RidenRD60xx#getVoltageSet
 */
double RidenRD60xx::getVoltageSet() {
  return cacheVoltageSet;
}

/**
 * Reads VSET directly from the device, bypassing the poll cache.
 *
 * Java equivalent: RidenRD60xx#getVoltageSetVerified
 */
double RidenRD60xx::getVoltageSetVerified() {
  double v = 0.0;
  read(VSET, v);
  return v;
}

// ---- Current setters / getters ---------------------------------------------

/**
 * Java equivalent: RidenRD60xx#setCurrent
 */
bool RidenRD60xx::setCurrent(double amperes) {
  return write(ISET, amperes);
}

/**
 * Java equivalent: RidenRD60xx#getCurrent
 */
double RidenRD60xx::getCurrent() {
  return cacheCurrentOut;
}

/**
 * Java equivalent: RidenRD60xx#getCurrentSet
 */
double RidenRD60xx::getCurrentSet() {
  return cacheCurrentSet;
}

/**
 * Reads ISET directly from the device, bypassing the poll cache.
 *
 * Java equivalent: RidenRD60xx#getCurrentSetVerified
 */
double RidenRD60xx::getCurrentSetVerified() {
  double v = 0.0;
  read(ISET, v);
  return v;
}

// ---- Atomic voltage + current write ----------------------------------------

/**
 * Sets voltage and current setpoints atomically in a single 0x10 frame.
 * Prevents inter-frame gaps that some firmware cannot tolerate.
 *
 * Java equivalent: RidenRD60xx#setVoltageCurrent
 */
bool RidenRD60xx::setVoltageCurrent(double volts, double amperes) {
  uint16_t values[2];
  values[0] = (uint16_t)VSET.encode(volts);
  values[1] = (uint16_t)ISET.encode(amperes);
  return writeBlock(RidenRegistersRD60xx::REG_VSET, values, 2);
}

// ---- Power / input voltage -------------------------------------------------

/**
 * Java equivalent: RidenRD60xx#getPower
 */
double RidenRD60xx::getPower() {
  return cachePowerOut;
}

/**
 * Java equivalent: RidenRD60xx#getInputVoltage
 */
double RidenRD60xx::getInputVoltage() {
  return cacheVoltageIn;
}

// ---- Output enable ---------------------------------------------------------

/**
 * Java equivalent: RidenRD60xx#setOutput
 */
bool RidenRD60xx::setOutput(bool outputEnabled) {
  return writeInt(OUTPUT_ENABLE,
                  outputEnabled ? ModbusConstants::STATE_ON : ModbusConstants::STATE_OFF);
}

/**
 * Java equivalent: RidenRD60xx#getOutput
 */
bool RidenRD60xx::getOutput() {
  return (cacheOutput == ModbusConstants::STATE_ON);
}

// ---- Temperature -----------------------------------------------------------

/**
 * Returns the internal temperature in degrees Celsius from the poll cache.
 *
 * The RD60xx represents temperature as a magnitude in REG_TEMP_CELSIUS and
 * a separate sign in REG_TEMP_SIGN_CELSIUS (0 = positive, 1 = negative).
 * Both are populated by pollAll(); the sign is applied here.
 *
 * Java equivalent: RidenRD60xx#getTemperatureCelsius
 */
double RidenRD60xx::getTemperatureCelsius() {
  return (cacheTempSignCelsius == ModbusConstants::STATE_ON) ? -cacheTemperature : cacheTemperature;
}

// ---- Firmware --------------------------------------------------------------

/**
 * Java equivalent: RidenRD60xx#getFirmwareVersion
 */
int RidenRD60xx::getFirmwareVersion() {
  return (int)FIRMWARE_VERSION.decode(cacheFirmwareRaw);
}

// ---- Protection state ------------------------------------------------------

/**
 * Java equivalent: RidenRD60xx#setProtectionState
 */
bool RidenRD60xx::setProtectionState(bool on) {
  return writeInt(PROTECTION_STATE,
                  on ? ModbusConstants::STATE_ON : ModbusConstants::STATE_OFF);
}

/**
 * Java equivalent: RidenRD60xx#getProtectionState
 */
bool RidenRD60xx::getProtectionState() {
  return (cacheProtection == ModbusConstants::STATE_ON);
}

// ---- Keypad lock -----------------------------------------------------------

/**
 * Java equivalent: RidenRD60xx#setKeypad
 */
bool RidenRD60xx::setKeypad(bool locked) {
  return writeInt(LOCK, locked ? ModbusConstants::STATE_ON : ModbusConstants::STATE_OFF);
}

/**
 * Java equivalent: RidenRD60xx#getKeypad
 */
bool RidenRD60xx::getKeypad() {
  return (cacheLock == ModbusConstants::STATE_ON);
}

// ---- CV/CC mode ------------------------------------------------------------

/**
 * Returns true if the device is in CV (constant voltage) mode.
 * Register REG_MODE: 0 = CV, 1 = CC.
 *
 * Java equivalent: RidenRD60xx#isCvMode
 */
bool RidenRD60xx::isCvMode() {
  return (cacheMode == 0);
}

// ---- Reconnect -------------------------------------------------------------

/**
 * Java equivalent: RidenRD60xx#reconnect (delegates to ModbusDevice)
 */
bool RidenRD60xx::reconnect() {
  return ModbusDevice::reconnect();
}
