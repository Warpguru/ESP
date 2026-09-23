#pragma once

/**
 * DC2DCConverter.h - Pure abstract interface for all DC/DC converter drivers.
 *
 * Java equivalent: com.serial.devices.ifc.DC2DCConverter (interface)
 *
 * C++ does not have a separate interface keyword. The equivalent is a class
 * with only pure virtual methods (= 0) and a virtual destructor. This forces
 * every concrete driver (RidenRD60xx, Sinilink, etc.) to implement all methods,
 * exactly as a Java interface does. No templates required.
 */
class DC2DCConverter {
 public:
  virtual ~DC2DCConverter() = default;

  /** Returns the device identification string (e.g. "RD6006"). */
  virtual const char* getDevice() = 0;

  /** Returns the manufacturer string (e.g. "Riden"), or nullptr if not yet detected. */
  virtual const char* getManufacturer() = 0;

  /** Sets the output voltage setpoint (volts). */
  virtual bool setVoltage(double volts) = 0;

  /** Returns the cached measured output voltage (volts). */
  virtual double getVoltage() = 0;

  /** Sets the output current setpoint (amperes). */
  virtual bool setCurrent(double amperes) = 0;

  /** Returns the cached measured output current (amperes). */
  virtual double getCurrent() = 0;

  /** Returns the cached output power (watts). */
  virtual double getPower() = 0;

  /** Returns the cached input voltage (volts). */
  virtual double getInputVoltage() = 0;

  /**
   * Sets voltage and current setpoints atomically in a single 0x10 frame.
   * Prevents inter-frame gaps that some firmware (e.g. Sinilink) cannot tolerate.
   */
  virtual bool setVoltageCurrent(double volts, double amperes) = 0;

  /** Enables or disables the output. */
  virtual bool setOutput(bool on) = 0;

  /** Returns the cached output state. */
  virtual bool getOutput() = 0;

  /** Returns the cached temperature (degrees Celsius). */
  virtual double getTemperatureCelsius() = 0;

  /** Returns the firmware version integer. */
  virtual int getFirmwareVersion() = 0;

  /** Enables or disables the protection state. */
  virtual bool setProtectionState(bool on) = 0;

  /** Returns the cached protection state. */
  virtual bool getProtectionState() = 0;

  /** Locks or unlocks the physical keypad. */
  virtual bool setKeypad(bool locked) = 0;

  /** Returns the cached keypad (child lock) state. */
  virtual bool getKeypad() = 0;

  /** Returns true if the device is in CV (constant voltage) mode, false for CC. */
  virtual bool isCvMode() = 0;

  /** Returns the cached voltage setpoint (VSET). */
  virtual double getVoltageSet() = 0;

  /** Reads VSET directly from the device, bypassing the poll cache. */
  virtual double getVoltageSetVerified() = 0;

  /** Returns the cached current setpoint (ISET). */
  virtual double getCurrentSet() = 0;

  /** Reads ISET directly from the device, bypassing the poll cache. */
  virtual double getCurrentSetVerified() = 0;

  /**
   * Reads all registers needed for a full poll cycle in a single bulk 0x03 frame
   * and stores decoded values in internal cache fields.
   * All getter methods return cached values after this call.
   *
   * @return true on success; cache is not modified on failure
   */
  virtual bool pollAll() = 0;

  /**
   * Closes and re-opens the serial transport.
   * Called after consecutive poll failures to recover from a disconnected adapter.
   *
   * @return true on success
   */
  virtual bool reconnect() = 0;
};
