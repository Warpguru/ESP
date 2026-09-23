#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "../../devices/src/ifc/DC2DCConverter.h"
#include "ConverterState.h"

/**
 * DeviceService.h - Service layer for device polling and validated setpoint writes.
 *
 * Java equivalent: com.serial.service.DeviceService
 *
 * Owns the background Modbus polling FreeRTOS task and all validated write
 * operations. All write methods are serialised with a FreeRTOS mutex that
 * also gates the poll task, matching the Java synchronized keyword.
 *
 * RestService calls these methods instead of touching activeDevice or
 * converterState directly - matching the Java layering exactly.
 */
class DeviceService {
 public:
  /**
   * Constructs the service.
   *
   * @param state     shared converter state (owned externally - the global converterState)
   * @param converter detected device driver (owned externally - the global activeDevice)
   *
   * Java equivalent: DeviceService constructor minus port detection (detection is
   * done in applicationSetup() before DeviceService is constructed here).
   */
  DeviceService(ConverterState* state, DC2DCConverter* converter);

  /**
   * Starts the background Modbus polling FreeRTOS task on Core 1.
   * Must be called from applicationSetup() after WiFi is up.
   *
   * Java equivalent: DeviceService#start
   */
  void begin();

  /**
   * Returns true if a converter has been detected (device name populated).
   *
   * Java equivalent: DeviceService#isDeviceDetected
   */
  bool isDeviceDetected() const;

  /**
   * Returns a const pointer to the shared ConverterState for read access.
   *
   * Java equivalent: DeviceService#getState
   */
  const ConverterState* getState() const;

  // ---- Validated write operations -------------------------------------------
  // Java equivalent: the synchronized write methods in DeviceService.
  // Each acquires _mutex before calling the driver, preventing concurrent
  // serial access between REST/WS handlers and the poll task.

  /**
   * Sets the voltage setpoint with range validation and settle-window suppression.
   * Returns false if out of range or the Modbus write fails.
   *
   * Java equivalent: DeviceService#setVoltage
   */
  bool setVoltage(double volts);

  /**
   * Sets the voltage setpoint and verifies via Modbus read-back.
   * Returns confirmed value via out-param; sets outConflict=true on read-back mismatch.
   *
   * Java equivalent: DeviceService#setVoltageVerified
   */
  bool setVoltageVerified(double volts, double& confirmedOut, bool& outConflict);

  /**
   * Sets the current setpoint with range validation and settle-window suppression.
   *
   * Java equivalent: DeviceService#setCurrent
   */
  bool setCurrent(double amperes);

  /**
   * Sets the current setpoint and verifies via Modbus read-back.
   *
   * Java equivalent: DeviceService#setCurrentVerified
   */
  bool setCurrentVerified(double amperes, double& confirmedOut, bool& outConflict);

  /**
   * Sets voltage and current atomically in a single 0x10 frame.
   *
   * Java equivalent: DeviceService#setMeasurements
   */
  bool setVoltageCurrent(double volts, double amperes);

  /**
   * Enables or disables the converter output.
   *
   * Java equivalent: DeviceService#setOutput
   */
  bool setOutput(bool on);

  /**
   * Locks or unlocks the keypad (child lock).
   *
   * Java equivalent: DeviceService#setKeypad
   */
  bool setKeypad(bool locked);

  /**
   * Clears a tripped protection condition.
   *
   * Java equivalent: DeviceService#clearProtection
   */
  bool clearProtection();

 private:
  ConverterState* _state;
  DC2DCConverter* _converter;

  /** Serialises poll task and write operations - Java equivalent: synchronized. */
  SemaphoreHandle_t _mutex;

  /** millis() deadline before which poll must not overwrite voltageSet. */
  volatile uint32_t _voltagePendingUntil = 0;

  /** millis() deadline before which poll must not overwrite currentSet. */
  volatile uint32_t _currentPendingUntil = 0;

  static void pollingTask(void* param);
  void poll();
  void attemptReconnect();
  double effectiveMaxVoltage() const;
  double effectiveMaxCurrent() const;
  bool validateRange(const char* name, double value, double min, double max);
};
