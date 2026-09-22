#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "ConverterTopology.h"

// ConverterState.cpp includes ModBus.h and RidenConfig.h for the applySetpoint
// methods. Callers that need the full Modbus API include ModBus.h directly.

/**
 * ConverterState.h - Thread-safe holder of all DC/DC converter state.
 *
 * Java equivalent: com.serial.service.ConverterState
 *
 * Single source of truth shared between the Modbus polling task (Core 0)
 * and the HTTP/WebSocket handlers (async server task).
 *
 * Java volatile fields map to regular fields protected by a FreeRTOS mutex.
 * Java synchronized compound operations map to mutex take/give pairs in
 * DeviceService.
 *
 * Private field names match the Java field names exactly. Where a setter
 * parameter would shadow the field of the same name, the implementation uses
 * this->fieldName = fieldName (matching the Java this.fieldName = fieldName
 * pattern).
 */
class ConverterState {
 public:
  ConverterState();
  ~ConverterState();

  // ---- Measured values (updated by polling task) --------------------------

  void setVoltageOut(double voltageOut);
  double getVoltageOut() const;

  void setCurrentOut(double currentOut);
  double getCurrentOut() const;

  void setPowerOut(double powerOut);
  double getPowerOut() const;

  void setVoltageIn(double voltageIn);
  double getVoltageIn() const;

  void setTemperatureCelsius(double temperatureCelsius);
  double getTemperatureCelsius() const;

  // ---- Setpoints (polled every second to catch front-panel changes) --------

  void setVoltageSet(double voltageSet);
  double getVoltageSet() const;

  void setCurrentSet(double currentSet);
  double getCurrentSet() const;

  // ---- Output & mode state ------------------------------------------------

  void setOutputEnabled(bool outputEnabled);
  bool isOutputEnabled() const;

  void setKeypadLocked(bool keypadLocked);
  bool isKeypadLocked() const;

  void setProtectionState(int protectionState);
  int getProtectionState() const;

  void setCvMode(bool cvMode);
  bool isCvMode() const;

  // ---- Device limits (set once after detection by DeviceService) ----------

  void setMaxVoltage(double maxVoltage);
  double getMaxVoltage() const;

  void setMinVoltage(double minVoltage);
  double getMinVoltage() const;

  void setMaxCurrent(double maxCurrent);
  double getMaxCurrent() const;

  void setMinCurrent(double minCurrent);
  double getMinCurrent() const;

  void setMaxPower(double maxPower);
  double getMaxPower() const;

  /** Operator-configured voltage ceiling; 0.0 = no limit. */
  void setConfigMaxVoltage(double configMaxVoltage);
  double getConfigMaxVoltage() const;

  /** Operator-configured current ceiling; 0.0 = no limit. */
  void setConfigMaxCurrent(double configMaxCurrent);
  double getConfigMaxCurrent() const;

  void setConverterTopology(ConverterTopology converterTopology);
  ConverterTopology getConverterTopology() const;

  // ---- Device presence & identity -----------------------------------------

  void setDeviceOnline(bool deviceOnline);
  bool isDeviceOnline() const;

  void setDeviceName(const char* deviceName);
  const char* getDeviceName() const;

  void setManufacturer(const char* manufacturer);
  const char* getManufacturer() const;

  /** Firmware version string, e.g. "v1.7"; empty string if unknown. */
  void setFirmwareVersion(const char* firmwareVersion);
  const char* getFirmwareVersion() const;

  // ---- Setpoint apply with settle-window (anti-flicker) -------------------
  //
  // These methods write the value to the device via Modbus, update the cached
  // setpoint, and arm a 2-second suppress window so the polling task does not
  // overwrite the displayed value with a transient device read-back.
  // (ESP32-specific — no Java equivalent in ConverterState.java; the settle
  // window logic lives in DeviceService.java on the Java side.)

  /**
   * Write voltage setpoint to device, update cache, arm settle window.
   * Returns true on successful Modbus write.
   */
  bool applyVoltageSetpoint(double volts);

  /**
   * Write current setpoint to device, update cache, arm settle window.
   * Returns true on successful Modbus write.
   */
  bool applyCurrentSetpoint(double amps);

  /**
   * Returns true if the voltage settle window is still active.
   * The polling task should skip updating voltageSet while this is true.
   */
  bool isVoltagePending() const;

  /**
   * Returns true if the current settle window is still active.
   * The polling task should skip updating currentSet while this is true.
   */
  bool isCurrentPending() const;

 private:
  SemaphoreHandle_t mutex;

  // Measured values — names match Java field names exactly
  double voltageOut = 0.0;
  double currentOut = 0.0;
  double powerOut = 0.0;
  double voltageIn = 0.0;
  double temperatureCelsius = 0.0;

  // Setpoints
  double voltageSet = 0.0;
  double currentSet = 0.0;

  // Settle-window timestamps (millis). While millis() < timestamp, the
  // corresponding setpoint read-back from the poller is suppressed.
  // (ESP32-specific — not present in Java ConverterState)
  uint32_t voltagePendingUntil = 0;
  uint32_t currentPendingUntil = 0;

  // Output & mode
  bool outputEnabled = false;
  bool keypadLocked = false;
  int protectionState = 0;
  bool cvMode = false;

  // Device presence & identity
  bool deviceOnline = false;
  const char* deviceName = "Unknown";
  const char* manufacturer = "Unknown";
  const char* firmwareVersion = "";

  // Device limits (set once after detection)
  double maxVoltage = 0.0;
  double minVoltage = 0.0;
  double maxCurrent = 0.0;
  double minCurrent = 0.0;
  double maxPower = 0.0;
  double configMaxVoltage = 0.0;
  double configMaxCurrent = 0.0;
  ConverterTopology converterTopology = ConverterTopology::BUCK_BOOST;
};
