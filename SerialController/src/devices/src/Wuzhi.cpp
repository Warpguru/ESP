#include "Wuzhi.h"

#include <Arduino.h>

#include "../../modbus/src/ModbusConstants.h"
#include "esp_log.h"

/**
 * Wuzhi.cpp - Driver for Wuzhi ZK-series DC/DC converters.
 *
 * Java equivalent: com.serial.devices.Wuzhi
 *
 * Register map is address-for-address identical to Sinilink XY-series.
 * The single critical difference: ISET/IOUT use scale 100 (10 mA resolution),
 * not scale 1000 as on the Sinilink XY6008/XY6014 class.
 */

static const char* TAG_WZ = "WUZHI";

// ---- DeviceRegister descriptors ---------------------------------------------
// Java equivalent: public static final DeviceRegister ... in Wuzhi.java
// Key difference from Sinilink: ISET / IOUT scale = 100 (not 1000)

static const DeviceRegister VSET("Voltage Setpoint", "V", WuzhiRegisters::REG_VSET, 100.0);
static const DeviceRegister ISET("Current Setpoint", "A", WuzhiRegisters::REG_ISET, 100.0);  // ← 100, not 1000
static const DeviceRegister VOUT("Output Voltage", "V", WuzhiRegisters::REG_VOUT, 100.0);
static const DeviceRegister IOUT("Output Current", "A", WuzhiRegisters::REG_IOUT, 100.0);  // ← 100, not 1000
static const DeviceRegister POUT("Output Power", "W", WuzhiRegisters::REG_POUT, 100.0);
static const DeviceRegister VIN("Input Voltage", "V", WuzhiRegisters::REG_VIN, 100.0);
static const DeviceRegister TEMP("Temperature Celsius", "°C", WuzhiRegisters::REG_TEMPERATURE_INTERNAL, 10.0);
static const DeviceRegister LOCK("Keypad Lock", nullptr, WuzhiRegisters::REG_KEYPAD_LOCK);
static const DeviceRegister PROT("Protection Status", nullptr, WuzhiRegisters::REG_PROTECTION_STATE);
static const DeviceRegister OUTP("Output Enable", nullptr, WuzhiRegisters::REG_OUTPUT_ENABLE);
static const DeviceRegister MODL("Model Version", nullptr, WuzhiRegisters::REG_MODEL);
static const DeviceRegister FWVR("Firmware Version", nullptr, WuzhiRegisters::REG_FIRMWARE, 100.0);

// ---- Device identification tables ------------------------------------------
// Java equivalent: private static final Map<Integer, String> KNOWN_MODELS / REPORTED_MODELS

struct WuzhiModelEntry {
  int value;
  const char* name;
};

/**
 * Authoritative lookup table: factory-confirmed product model register values.
 * Java equivalent: Wuzhi.KNOWN_MODELS
 */
static const WuzhiModelEntry KNOWN_MODELS[] = {
    {6522, "ZK-6522C"},    // legacy flat integer
    {10022, "ZK-10022C"},  // unverified
    {150, "ZK-SK150C"},    // unverified
    {3605, "WZ3605E"},     // unverified
    {5005, "WZ5005E"},     // unverified
    {6008, "WZ-6008"},     // unverified
};
static constexpr int KNOWN_MODELS_COUNT = (int)(sizeof(KNOWN_MODELS) / sizeof(KNOWN_MODELS[0]));

/**
 * Community-reported lookup table: packed 0x59xx product model register values.
 * Java equivalent: Wuzhi.REPORTED_MODELS (currently empty in Java, same here)
 * NOTE - community data, not factory-confirmed. Populate when ZK-6522C raw value is observed.
 */
static const WuzhiModelEntry REPORTED_MODELS[] = {
    // TODO: populate with community-reported ZK-series model register values as they are discovered.
    // Example: { XXXXX, "ZK-6522C" }
};
static constexpr int REPORTED_MODELS_COUNT = (int)(sizeof(REPORTED_MODELS) / sizeof(REPORTED_MODELS[0]));

/** High byte of modern packed model register ('Y' = 0x59). Java equivalent: WUZHI_MODEL_HIGH_BYTE */
static constexpr int WUZHI_MODEL_HIGH_BYTE = 0x59;

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

Wuzhi::Wuzhi(ModbusTransport* transport, uint8_t slave)
    : ModbusDevice(transport, slave) {
}

// ---- verifyDevicePresent ----------------------------------------------------

/**
 * Reads the product model register (0x0016) and identifies the device using a
 * three-step strategy identical to Sinilink#verifyDevicePresent.
 *
 * Java equivalent: Wuzhi#verifyDevicePresent(List<Integer> bauds) inner probe body.
 */
bool Wuzhi::verifyDevicePresent() {
  int modelValue = 0;
  if (!readInt(MODL, modelValue)) {
    return false;
  }

  char hexBuf[5];
  snprintf(hexBuf, sizeof(hexBuf), "%04X", (unsigned)modelValue);
  ESP_LOGI(TAG_WZ, "Product model register (0x0016) raw value: %d (0x%s)", modelValue, hexBuf);

  // Step 1: exact match in KNOWN_MODELS
  const char* modelName = lookupKnownModel(modelValue);

  // Step 2: if high byte = 0x59 ('Y'), consult REPORTED_MODELS
  if (modelName == nullptr && (modelValue >> 8) == WUZHI_MODEL_HIGH_BYTE) {
    modelName = lookupReportedModel(modelValue);
    if (modelName != nullptr) {
      ESP_LOGW(TAG_WZ,
               "Product model register 0x%s matched community-reported Wuzhi data as %s"
               " -- not factory-confirmed; promote to KNOWN_MODELS once verified on hardware.",
               hexBuf, modelName);
    } else {
      ESP_LOGW(TAG_WZ,
               "Product model register 0x%s has Wuzhi/Sinilink 'Y' high byte"
               " but is not in KNOWN_MODELS or REPORTED_MODELS -- device not identified."
               " Add Map.entry(%d, \"ZK?????\") to Wuzhi.KNOWN_MODELS once the model is confirmed.",
               hexBuf, modelValue);
    }
  }

  if (modelName == nullptr) {
    return false;
  }

  manufacturer = "Wuzhi";
  device = modelName;

  int fw = 0;
  readInt(FWVR, fw);
  ESP_LOGI(TAG_WZ, "Detected Wuzhi ZK-series %s (product model: 0x%s, FW raw: %d).",
           modelName, hexBuf, fw);
  return true;
}

// ---- DC2DCConverter interface - getDevice / getManufacturer -----------------

const char* Wuzhi::getDevice() {
  return ModbusDevice::getDevice();
}

const char* Wuzhi::getManufacturer() {
  return ModbusDevice::getManufacturer();
}

// ---- pollAll ----------------------------------------------------------------

/**
 * Reads the full register block (0x0000–0x0012, 19 registers) in a single
 * Modbus 0x03 frame and populates all poll-cache fields.
 *
 * Offset map identical to Sinilink (address − REG_VSET).
 *
 * Java equivalent: Wuzhi#pollAll
 */
bool Wuzhi::pollAll() {
  uint16_t r[19];
  if (!readBlock((uint16_t)WuzhiRegisters::REG_VSET, 19, r)) {
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

bool Wuzhi::setVoltage(double volts) {
  return write(VSET, volts);
}

double Wuzhi::getVoltage() {
  return cacheVoltageOut;
}

double Wuzhi::getVoltageSet() {
  return cacheVoltageSet;
}

double Wuzhi::getVoltageSetVerified() {
  double v = 0.0;
  read(VSET, v);
  return v;
}

// ---- Current ----------------------------------------------------------------

bool Wuzhi::setCurrent(double amperes) {
  return write(ISET, amperes);
}

double Wuzhi::getCurrent() {
  return cacheCurrentOut;
}

double Wuzhi::getCurrentSet() {
  return cacheCurrentSet;
}

double Wuzhi::getCurrentSetVerified() {
  double v = 0.0;
  read(ISET, v);
  return v;
}

// ---- Atomic voltage + current write -----------------------------------------

/**
 * Java equivalent: Wuzhi#setVoltageCurrent
 */
bool Wuzhi::setVoltageCurrent(double volts, double amperes) {
  uint16_t values[2];
  values[0] = (uint16_t)VSET.encode(volts);
  values[1] = (uint16_t)ISET.encode(amperes);
  return writeBlock((uint16_t)WuzhiRegisters::REG_VSET, values, 2);
}

// ---- Power / input voltage --------------------------------------------------

double Wuzhi::getPower() {
  return cachePowerOut;
}

double Wuzhi::getInputVoltage() {
  return cacheVoltageIn;
}

// ---- Output enable ----------------------------------------------------------

bool Wuzhi::setOutput(bool outputEnabled) {
  return writeInt(OUTP, outputEnabled ? ModbusConstants::STATE_ON : ModbusConstants::STATE_OFF);
}

bool Wuzhi::getOutput() {
  return (cacheOutput == ModbusConstants::STATE_ON);
}

// ---- Temperature ------------------------------------------------------------

double Wuzhi::getTemperatureCelsius() {
  return cacheTemperature;
}

// ---- Firmware ---------------------------------------------------------------

int Wuzhi::getFirmwareVersion() {
  int raw = 0;
  readInt(FWVR, raw);
  return raw;
}

// ---- Protection state -------------------------------------------------------

bool Wuzhi::setProtectionState(bool on) {
  return writeInt(PROT, on ? ModbusConstants::STATE_ON : ModbusConstants::STATE_OFF);
}

bool Wuzhi::getProtectionState() {
  return (cacheProtection != ModbusConstants::STATE_OFF);
}

// ---- Keypad lock ------------------------------------------------------------

bool Wuzhi::setKeypad(bool locked) {
  return writeInt(LOCK, locked ? ModbusConstants::STATE_ON : ModbusConstants::STATE_OFF);
}

bool Wuzhi::getKeypad() {
  return (cacheLock == ModbusConstants::STATE_ON);
}

// ---- CV/CC mode -------------------------------------------------------------

/**
 * Register REG_MODE: 0 = CV, 1 = CC.
 * Java equivalent: Wuzhi#isCvMode
 */
bool Wuzhi::isCvMode() {
  return (cacheMode == 0);
}

// ---- Reconnect --------------------------------------------------------------

bool Wuzhi::reconnect() {
  return ModbusDevice::reconnect();
}
