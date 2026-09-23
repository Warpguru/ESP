#include "Sinilink.h"

#include <Arduino.h>

#include "../../../src/SerialController/src/LogBuffer.h"
#include "../../modbus/src/ModbusConstants.h"

/**
 * Sinilink.cpp - Driver for Sinilink XY-series DC/DC converters.
 *
 * Java equivalent: com.serial.devices.Sinilink
 */

// ---- DeviceRegister descriptors ---------------------------------------------
// Java equivalent: public static final DeviceRegister ... in Sinilink.java
// Scales: voltage ÷100, current ÷1000 (XY6008/XY6014 class), power ÷100, temp ÷10

static const DeviceRegister VSET("Voltage Setpoint", "V", SinilinkRegisters::REG_VSET, 100.0);
static const DeviceRegister ISET("Current Setpoint", "A", SinilinkRegisters::REG_ISET, 1000.0);
static const DeviceRegister VOUT("Output Voltage", "V", SinilinkRegisters::REG_VOUT, 100.0);
static const DeviceRegister IOUT("Output Current", "A", SinilinkRegisters::REG_IOUT, 1000.0);
static const DeviceRegister POUT("Output Power", "W", SinilinkRegisters::REG_POUT, 100.0);
static const DeviceRegister VIN("Input Voltage", "V", SinilinkRegisters::REG_VIN, 100.0);
static const DeviceRegister TEMP("Temperature Celsius", "°C", SinilinkRegisters::REG_TEMPERATURE_INTERNAL, 10.0);
static const DeviceRegister LOCK("Keypad Lock", nullptr, SinilinkRegisters::REG_KEYPAD_LOCK);
static const DeviceRegister PROT("Protection Status", nullptr, SinilinkRegisters::REG_PROTECTION_STATE);
static const DeviceRegister OUTP("Output Enable", nullptr, SinilinkRegisters::REG_OUTPUT_ENABLE);
static const DeviceRegister MODL("Model Version", nullptr, SinilinkRegisters::REG_MODEL);
static const DeviceRegister FWVR("Firmware Version", nullptr, SinilinkRegisters::REG_FIRMWARE, 100.0);

// ---- Device identification tables ------------------------------------------
// Java equivalent: private static final Map<Integer, String> KNOWN_MODELS / REPORTED_MODELS

struct SinilinkModelEntry {
  int value;
  const char* name;
};

/**
 * Authoritative lookup table: factory-confirmed product model register values.
 * Java equivalent: Sinilink.KNOWN_MODELS Map.ofEntries(...)
 */
static const SinilinkModelEntry KNOWN_MODELS[] = {
    {5008, "XY5008"},    // legacy flat integer
    {6008, "XY6008"},    // legacy flat integer
    {6014, "XY6014"},    // legacy flat integer
    {6020, "XY6020L"},   // legacy flat integer
    {3680, "XYH3680"},   // legacy flat integer
    {20488, "XY5008"},   // 0x5008 alternate encoding
    {24584, "XY6008"},   // 0x6008 alternate encoding
    {24832, "XY6020L"},  // 0x6100
    {13831, "XY3607F"},  // 0x3607
    {6149, "SK180S"},    // 0x1805
    {8713, "SK220S"},    // 0x2209
};
static constexpr int KNOWN_MODELS_COUNT = (int)(sizeof(KNOWN_MODELS) / sizeof(KNOWN_MODELS[0]));

/**
 * Community-reported lookup table: packed 0x59xx product model register values.
 * Java equivalent: Sinilink.REPORTED_MODELS
 * NOTE - community data, not factory-confirmed.
 */
static const SinilinkModelEntry REPORTED_MODELS[] = {
    {22792, "XY5008"},   // 0x5908 v0.8 variant
    {22802, "XY6008"},   // 0x5912 v1.8 layout
    {22804, "XY6008"},   // 0x5914 v2.0 layout
    {22798, "XY6014"},   // 0x590E v1.4 layout
    {22797, "XY6020L"},  // 0x590D v1.3 layout
    {22794, "XYH3680"},  // 0x590A v1.0 layout
};
static constexpr int REPORTED_MODELS_COUNT = (int)(sizeof(REPORTED_MODELS) / sizeof(REPORTED_MODELS[0]));

/** High byte of modern packed model register ('Y' = 0x59). Java equivalent: SINILINK_MODEL_HIGH_BYTE */
static constexpr int SINILINK_MODEL_HIGH_BYTE = 0x59;

static const char* lookupKnownModel(int value) {
  for (int i = 0; i < KNOWN_MODELS_COUNT; i++) {
    if (KNOWN_MODELS[i].value == value) {
      return KNOWN_MODELS[i].name;
    }
  }
  return nullptr;
}

static const char* lookupReportedModel(int value) {
  for (int i = 0; i < REPORTED_MODELS_COUNT; i++) {
    if (REPORTED_MODELS[i].value == value) {
      return REPORTED_MODELS[i].name;
    }
  }
  return nullptr;
}

// ---- Constructor ------------------------------------------------------------

Sinilink::Sinilink(ModbusTransport* transport, uint8_t slave)
    : ModbusDevice(transport, slave) {
}

// ---- verifyDevicePresent ----------------------------------------------------

/**
 * Reads the product model register (0x0016) and identifies the device using a
 * three-step strategy:
 *   1. KNOWN_MODELS exact match (factory-confirmed, no warning).
 *   2. If high byte = 0x59 ('Y'): consult REPORTED_MODELS (community data, WARN).
 *   3. If high byte = 0x59 but no match: log WARN and skip - do not guess.
 *
 * Sets manufacturer = "Sinilink" and device = model name on success.
 * Returns true if identified.
 *
 * Java equivalent: Sinilink#verifyDevicePresent(List<Integer> bauds) inner probe body.
 */
bool Sinilink::verifyDevicePresent() {
  int modelValue = 0;
  if (!readInt(MODL, modelValue)) {
    return false;
  }

  char hexBuf[5];
  snprintf(hexBuf, sizeof(hexBuf), "%04X", (unsigned)modelValue);
  Log_info("Product model register (0x0016) raw value: %d (0x%s)", modelValue, hexBuf);

  // Step 1: exact match in KNOWN_MODELS
  const char* modelName = lookupKnownModel(modelValue);

  // Step 2: if high byte = 0x59 ('Y'), consult REPORTED_MODELS
  if (modelName == nullptr && (modelValue >> 8) == SINILINK_MODEL_HIGH_BYTE) {
    modelName = lookupReportedModel(modelValue);
    if (modelName != nullptr) {
      Log_warn(
          "Product model register 0x%s matched community-reported data as %s"
          " -- not factory-confirmed; promote to KNOWN_MODELS once verified on hardware.",
          hexBuf, modelName);
    } else {
      Log_warn(
          "Product model register 0x%s has Sinilink 'Y' high byte"
          " but is not in KNOWN_MODELS or REPORTED_MODELS -- device not identified.",
          hexBuf);
    }
  }

  if (modelName == nullptr) {
    return false;
  }

  manufacturer = "Sinilink";
  device = modelName;

  int fw = 0;
  readInt(FWVR, fw);
  Log_info("Detected Sinilink %s (product model: 0x%s, FW raw: %d).",
           modelName, hexBuf, fw);
  return true;
}

// ---- DC2DCConverter interface - getDevice / getManufacturer -----------------

const char* Sinilink::getDevice() {
  return ModbusDevice::getDevice();
}

const char* Sinilink::getManufacturer() {
  return ModbusDevice::getManufacturer();
}

// ---- pollAll ----------------------------------------------------------------

/**
 * Reads the full register block (0x0000–0x0012, 19 registers) in a single
 * Modbus 0x03 frame and populates all poll-cache fields.
 *
 * Offset map (address − REG_VSET):
 *  [0]  VSET   [1]  ISET    [2]  VOUT     [3]  IOUT    [4]  POUT   [5]  VIN
 *  [6]  AH_LOW [7]  AH_HIGH [8]  WH_LOW   [9]  WH_HIGH [10] OUT_H  [11] OUT_M
 * [12]  OUT_S  [13] TEMP    [14] TEMP_EXT [15] LOCK   [16] PROTECT [17] MODE
 * [18]  OUTPUT
 *
 * Java equivalent: Sinilink#pollAll
 */
bool Sinilink::pollAll() {
  uint16_t r[19];
  if (!readBlock((uint16_t)SinilinkRegisters::REG_VSET, 19, r)) {
    return false;
  }
  cacheVoltageSet = VSET.decode(r[0]);
  cacheCurrentSet = ISET.decode(r[1]);
  cacheVoltageOut = VOUT.decode(r[2]);
  cacheCurrentOut = IOUT.decode(r[3]);
  cachePowerOut = POUT.decode(r[4]);
  cacheVoltageIn = VIN.decode(r[5]);
  // r[6..12]: AH/WH/timer counters - not used
  cacheTemperature = TEMP.decode(r[13]);
  // r[14]: external temp - not used
  cacheLock = r[15];
  cacheProtection = r[16];
  cacheMode = r[17];
  cacheOutput = r[18];
  return true;
}

// ---- Voltage ----------------------------------------------------------------

bool Sinilink::setVoltage(double volts) {
  return write(VSET, volts);
}

double Sinilink::getVoltage() {
  return cacheVoltageOut;
}

double Sinilink::getVoltageSet() {
  return cacheVoltageSet;
}

/**
 * Reads VSET directly from the device, bypassing the poll cache.
 * Java equivalent: Sinilink#getVoltageSetVerified
 */
double Sinilink::getVoltageSetVerified() {
  double v = 0.0;
  read(VSET, v);
  return v;
}

// ---- Current ----------------------------------------------------------------

bool Sinilink::setCurrent(double amperes) {
  return write(ISET, amperes);
}

double Sinilink::getCurrent() {
  return cacheCurrentOut;
}

double Sinilink::getCurrentSet() {
  return cacheCurrentSet;
}

/**
 * Reads ISET directly from the device, bypassing the poll cache.
 * Java equivalent: Sinilink#getCurrentSetVerified
 */
double Sinilink::getCurrentSetVerified() {
  double v = 0.0;
  read(ISET, v);
  return v;
}

// ---- Atomic voltage + current write -----------------------------------------

/**
 * Sets voltage and current setpoints atomically in a single 0x10 frame.
 * Required: Sinilink firmware at 115200 baud cannot tolerate two back-to-back 0x06 frames.
 *
 * Java equivalent: Sinilink#setVoltageCurrent
 */
bool Sinilink::setVoltageCurrent(double volts, double amperes) {
  uint16_t values[2];
  values[0] = (uint16_t)VSET.encode(volts);
  values[1] = (uint16_t)ISET.encode(amperes);
  return writeBlock((uint16_t)SinilinkRegisters::REG_VSET, values, 2);
}

// ---- Power / input voltage --------------------------------------------------

double Sinilink::getPower() {
  return cachePowerOut;
}

double Sinilink::getInputVoltage() {
  return cacheVoltageIn;
}

// ---- Output enable ----------------------------------------------------------

bool Sinilink::setOutput(bool outputEnabled) {
  return writeInt(OUTP, outputEnabled ? ModbusConstants::STATE_ON : ModbusConstants::STATE_OFF);
}

bool Sinilink::getOutput() {
  return (cacheOutput == ModbusConstants::STATE_ON);
}

// ---- Temperature ------------------------------------------------------------

double Sinilink::getTemperatureCelsius() {
  return cacheTemperature;
}

// ---- Firmware ---------------------------------------------------------------

int Sinilink::getFirmwareVersion() {
  int raw = 0;
  readInt(FWVR, raw);
  return raw;
}

// ---- Protection state -------------------------------------------------------

bool Sinilink::setProtectionState(bool on) {
  return writeInt(PROT, on ? ModbusConstants::STATE_ON : ModbusConstants::STATE_OFF);
}

bool Sinilink::getProtectionState() {
  return (cacheProtection != ModbusConstants::STATE_OFF);
}

// ---- Keypad lock ------------------------------------------------------------

bool Sinilink::setKeypad(bool locked) {
  return writeInt(LOCK, locked ? ModbusConstants::STATE_ON : ModbusConstants::STATE_OFF);
}

bool Sinilink::getKeypad() {
  return (cacheLock == ModbusConstants::STATE_ON);
}

// ---- CV/CC mode -------------------------------------------------------------

/**
 * Register REG_MODE: 0 = CV, 1 = CC.
 * Java equivalent: Sinilink#isCvMode
 */
bool Sinilink::isCvMode() {
  return (cacheMode == 0);
}

// ---- Reconnect --------------------------------------------------------------

bool Sinilink::reconnect() {
  return ModbusDevice::reconnect();
}
