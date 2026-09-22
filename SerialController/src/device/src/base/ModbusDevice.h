#pragma once

#include <stdint.h>

#include "../../../modbus/src/ModbusTransport.h"
#include "DeviceRegister.h"

/**
 * ModbusDevice.h - Abstract base class for Modbus-connected device drivers.
 *
 * Java equivalent: com.serial.device.base.ModbusDevice (abstract class)
 *
 * Provides register read/write operations via a ModbusTransport instance.
 * Concrete device drivers (RidenRD60xx, Sinilink, etc.) inherit from this
 * class and add device-specific register maps and domain methods.
 *
 * Note: This is not a C++ abstract class (no pure virtual methods here).
 * Subclasses inherit the concrete read/write implementation and add their
 * own interface by also implementing DC2DCConverter.
 */
class ModbusDevice {
 public:
  /** Maximum number of retries for verified write operations. */
  static constexpr int MAX_RETRY = 3;

  /**
   * Constructs a Modbus device instance.
   *
   * @param transport pointer to the shared transport (owned externally)
   * @param slave     Modbus slave address of the device
   */
  ModbusDevice(ModbusTransport* transport, uint8_t slave);

  /**
   * Returns the Modbus slave address.
   */
  uint8_t getSlave() const;

  /**
   * Returns the detected device name string, or nullptr if not yet detected.
   */
  const char* getDevice() const;

  /**
   * Returns the detected manufacturer string, or nullptr if not yet detected.
   */
  const char* getManufacturer() const;

  /**
   * Returns true if both manufacturer and device strings have been populated.
   */
  bool isDeviceDetected() const;

  /**
   * Closes and re-opens the serial transport. Called after consecutive poll failures.
   *
   * @return true on success
   */
  bool reconnect();

  /**
   * Reads a register using a DeviceRegister descriptor; returns engineering value.
   *
   * @param reg    register descriptor
   * @param value  output: decoded engineering value
   * @return true on success
   */
  bool read(const DeviceRegister& reg, double& value);

  /**
   * Reads a register using a DeviceRegister descriptor; returns raw integer value.
   *
   * @param reg   register descriptor
   * @param value output: raw register value
   * @return true on success
   */
  bool readInt(const DeviceRegister& reg, int& value);

  /**
   * Writes an engineering value to a register using its scale factor.
   *
   * @param reg   register descriptor
   * @param value engineering value to write
   * @return true on success
   */
  bool write(const DeviceRegister& reg, double value);

  /**
   * Writes a raw integer value to a register without scaling.
   *
   * @param reg   register descriptor
   * @param value raw integer value to write
   * @return true on success
   */
  bool writeInt(const DeviceRegister& reg, int value);

  /**
   * Writes a value and verifies it by reading back both setpoint and output registers.
   * Retries up to MAX_RETRY times with a 200 ms delay between attempts.
   *
   * @param regSet  register descriptor for the setpoint
   * @param regOut  register descriptor for the measured output
   * @param value   engineering value to write
   * @return true if write was verified within MAX_RETRY attempts
   */
  bool writeVerified(const DeviceRegister& regSet, const DeviceRegister& regOut, double value);

  /**
   * Reads a contiguous block of raw 16-bit registers in a single 0x03 frame.
   *
   * @param startAddress first register address
   * @param count        number of registers (1–125)
   * @param values       output buffer; must hold at least count elements
   * @return true on success
   */
  bool readBlock(uint16_t startAddress, uint8_t count, uint16_t* values);

  /**
   * Writes a contiguous block of raw 16-bit registers in a single 0x10 frame.
   *
   * @param startAddress first register address
   * @param values       raw values to write
   * @param count        number of registers (1–32)
   * @return true on success
   */
  bool writeBlock(uint16_t startAddress, const uint16_t* values, uint8_t count);

 protected:
  /** Shared transport instance (not owned by this class). */
  ModbusTransport* transport;

  /** Modbus slave address. */
  uint8_t slave;

  /** Detected manufacturer string (nullptr until detection succeeds). */
  const char* manufacturer = nullptr;

  /** Detected device string (nullptr until detection succeeds). */
  const char* device = nullptr;
};
