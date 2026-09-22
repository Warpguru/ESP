#include "ConverterState.h"

#include "../../../ModBus.h"
#include "../../../RidenConfig.h"

/**
 * ConverterState.cpp - Thread-safe DC/DC converter state holder.
 *
 * Java equivalent: com.serial.service.ConverterState
 *
 * Where a setter parameter has the same name as its field (matching Java's
 * convention), this->fieldName = fieldName is used to disambiguate, exactly
 * as Java uses this.fieldName = fieldName.
 */

// Settle window duration in milliseconds (matches Java DeviceService constant).
static constexpr uint32_t SETTLE_MS = 2000;

ConverterState::ConverterState() {
  mutex = xSemaphoreCreateMutex();
}

ConverterState::~ConverterState() {
  if (mutex) {
    vSemaphoreDelete(mutex);
  }
}

// ---- Measured values --------------------------------------------------------

void ConverterState::setVoltageOut(double voltageOut) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  this->voltageOut = voltageOut;
  xSemaphoreGive(mutex);
}

double ConverterState::getVoltageOut() const {
  return voltageOut;
}

void ConverterState::setCurrentOut(double currentOut) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  this->currentOut = currentOut;
  xSemaphoreGive(mutex);
}

double ConverterState::getCurrentOut() const {
  return currentOut;
}

void ConverterState::setPowerOut(double powerOut) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  this->powerOut = powerOut;
  xSemaphoreGive(mutex);
}

double ConverterState::getPowerOut() const {
  return powerOut;
}

void ConverterState::setVoltageIn(double voltageIn) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  this->voltageIn = voltageIn;
  xSemaphoreGive(mutex);
}

double ConverterState::getVoltageIn() const {
  return voltageIn;
}

void ConverterState::setTemperatureCelsius(double temperatureCelsius) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  this->temperatureCelsius = temperatureCelsius;
  xSemaphoreGive(mutex);
}

double ConverterState::getTemperatureCelsius() const {
  return temperatureCelsius;
}

// ---- Setpoints --------------------------------------------------------------

void ConverterState::setVoltageSet(double voltageSet) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  this->voltageSet = voltageSet;
  xSemaphoreGive(mutex);
}

double ConverterState::getVoltageSet() const {
  return voltageSet;
}

void ConverterState::setCurrentSet(double currentSet) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  this->currentSet = currentSet;
  xSemaphoreGive(mutex);
}

double ConverterState::getCurrentSet() const {
  return currentSet;
}

// ---- Output & mode ----------------------------------------------------------

void ConverterState::setOutputEnabled(bool outputEnabled) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  this->outputEnabled = outputEnabled;
  xSemaphoreGive(mutex);
}

bool ConverterState::isOutputEnabled() const {
  return outputEnabled;
}

void ConverterState::setKeypadLocked(bool keypadLocked) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  this->keypadLocked = keypadLocked;
  xSemaphoreGive(mutex);
}

bool ConverterState::isKeypadLocked() const {
  return keypadLocked;
}

void ConverterState::setProtectionState(int protectionState) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  this->protectionState = protectionState;
  xSemaphoreGive(mutex);
}

int ConverterState::getProtectionState() const {
  return protectionState;
}

void ConverterState::setCvMode(bool cvMode) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  this->cvMode = cvMode;
  xSemaphoreGive(mutex);
}

bool ConverterState::isCvMode() const {
  return cvMode;
}

// ---- Device limits ----------------------------------------------------------

void ConverterState::setMaxVoltage(double maxVoltage) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  this->maxVoltage = maxVoltage;
  xSemaphoreGive(mutex);
}

double ConverterState::getMaxVoltage() const {
  return maxVoltage;
}

void ConverterState::setMinVoltage(double minVoltage) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  this->minVoltage = minVoltage;
  xSemaphoreGive(mutex);
}

double ConverterState::getMinVoltage() const {
  return minVoltage;
}

void ConverterState::setMaxCurrent(double maxCurrent) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  this->maxCurrent = maxCurrent;
  xSemaphoreGive(mutex);
}

double ConverterState::getMaxCurrent() const {
  return maxCurrent;
}

void ConverterState::setMinCurrent(double minCurrent) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  this->minCurrent = minCurrent;
  xSemaphoreGive(mutex);
}

double ConverterState::getMinCurrent() const {
  return minCurrent;
}

void ConverterState::setMaxPower(double maxPower) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  this->maxPower = maxPower;
  xSemaphoreGive(mutex);
}

double ConverterState::getMaxPower() const {
  return maxPower;
}

void ConverterState::setConfigMaxVoltage(double configMaxVoltage) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  this->configMaxVoltage = configMaxVoltage;
  xSemaphoreGive(mutex);
}

double ConverterState::getConfigMaxVoltage() const {
  return configMaxVoltage;
}

void ConverterState::setConfigMaxCurrent(double configMaxCurrent) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  this->configMaxCurrent = configMaxCurrent;
  xSemaphoreGive(mutex);
}

double ConverterState::getConfigMaxCurrent() const {
  return configMaxCurrent;
}

void ConverterState::setConverterTopology(ConverterTopology converterTopology) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  this->converterTopology = converterTopology;
  xSemaphoreGive(mutex);
}

ConverterTopology ConverterState::getConverterTopology() const {
  return converterTopology;
}

// ---- Device presence & identity ---------------------------------------------

void ConverterState::setDeviceOnline(bool deviceOnline) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  this->deviceOnline = deviceOnline;
  xSemaphoreGive(mutex);
}

bool ConverterState::isDeviceOnline() const {
  return deviceOnline;
}

void ConverterState::setDeviceName(const char* deviceName) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  this->deviceName = deviceName;
  xSemaphoreGive(mutex);
}

const char* ConverterState::getDeviceName() const {
  return deviceName;
}

void ConverterState::setManufacturer(const char* manufacturer) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  this->manufacturer = manufacturer;
  xSemaphoreGive(mutex);
}

const char* ConverterState::getManufacturer() const {
  return manufacturer;
}

void ConverterState::setFirmwareVersion(const char* firmwareVersion) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  this->firmwareVersion = firmwareVersion;
  xSemaphoreGive(mutex);
}

const char* ConverterState::getFirmwareVersion() const {
  return firmwareVersion;
}

// ---- Settle-window helpers --------------------------------------------------

bool ConverterState::isVoltagePending() const {
  return millis() < voltagePendingUntil;
}

bool ConverterState::isCurrentPending() const {
  return millis() < currentPendingUntil;
}

// ---- Setpoint apply ---------------------------------------------------------

bool ConverterState::applyVoltageSetpoint(double volts) {
  uint16_t raw = (uint16_t)(volts * 100.0);
  bool ok = writeModbusRegister(RIDEN_ID, REG_V_SET, raw);
  if (ok) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    voltageSet = volts;
    voltagePendingUntil = millis() + SETTLE_MS;
    xSemaphoreGive(mutex);
  }
  return ok;
}

bool ConverterState::applyCurrentSetpoint(double amps) {
  uint16_t raw = (uint16_t)(amps * 1000.0);
  bool ok = writeModbusRegister(RIDEN_ID, REG_I_SET, raw);
  if (ok) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    currentSet = amps;
    currentPendingUntil = millis() + SETTLE_MS;
    xSemaphoreGive(mutex);
  }
  return ok;
}
