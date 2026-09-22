#pragma once

#include "../../devices/src/ifc/DC2DCConverter.h"
#include "ConverterState.h"

/**
 * DeviceService.h - Service layer for device detection, polling and control.
 *
 * Java equivalent: com.serial.service.DeviceService
 *
 * Coordinates device auto-detection, the background Modbus polling task,
 * and validated setpoint writes with ceiling enforcement for BUCK topology.
 */
class DeviceService {
 public:
  /**
   * Constructs the service with a shared state object and a converter driver.
   *
   * @param state     shared converter state (owned externally)
   * @param converter detected device driver (owned externally)
   */
  DeviceService(ConverterState* state, DC2DCConverter* converter);

  /**
   * Starts the background Modbus polling task (FreeRTOS task on Core 0).
   * Must be called once from applicationSetup() after WiFi is ready.
   */
  void begin();

  /**
   * Sets output voltage with topology-aware ceiling enforcement.
   * For BUCK topology: clamps setpoint to (voltageIn - 1V).
   *
   * @param volts requested voltage setpoint
   * @return true on success
   */
  bool setVoltage(double volts);

  /**
   * Sets output current.
   *
   * @param amperes requested current setpoint
   * @return true on success
   */
  bool setCurrent(double amperes);

  /**
   * Sets voltage and current atomically (single 0x10 frame).
   *
   * @param volts   voltage setpoint
   * @param amperes current setpoint
   * @return true on success
   */
  bool setVoltageCurrent(double volts, double amperes);

  /**
   * Enables or disables the converter output.
   *
   * @param on true to enable, false to disable
   * @return true on success
   */
  bool setOutput(bool on);

  /**
   * Returns a const pointer to the shared converter state for read access.
   */
  const ConverterState* getState() const;

 private:
  ConverterState* state;
  DC2DCConverter* converter;

  static void pollingTask(void* param);
};
