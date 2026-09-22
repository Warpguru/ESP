#pragma once

#include <stdint.h>

/**
 * DeviceRegister.h - Describes a Modbus register with scaling metadata.
 *
 * Java equivalent: com.serial.device.base.DeviceRegister
 *
 * Encapsulates the address, name, unit and scale factor needed to convert
 * between raw Modbus register values and engineering units (V, A, W, etc.).
 *
 * Java's ConcurrentHashMap REGISTRY is replaced by a simple static array
 * registry, sufficient for the ESP32's single-threaded device driver context.
 */
class DeviceRegister {
 public:
  /** Human-readable register name (e.g. "Voltage Setpoint"). */
  const char* name;

  /** Engineering unit string (e.g. "V", "A", "W"). */
  const char* unit;

  /** Modbus register address. */
  uint16_t address;

  /**
   * Scaling factor: raw = round(engineering * scale), engineering = raw / scale.
   * scale=1 means no conversion. scale=100 means 2 decimal places (e.g. volts).
   */
  double scale;

  /**
   * Constructs a register descriptor and registers it in the global registry.
   *
   * @param name    human-readable name
   * @param unit    engineering unit string
   * @param address Modbus register address
   * @param scale   scaling factor (default 1.0)
   */
  DeviceRegister(const char* name, const char* unit, uint16_t address, double scale = 1.0);

  /**
   * Encodes an engineering value to the raw integer value expected by the device.
   * raw = round(value * scale)
   *
   * @param value engineering value
   * @return raw register value
   */
  int encode(double value) const;

  /**
   * Decodes a raw register value to an engineering value.
   * value = raw / scale
   *
   * @param raw raw register value
   * @return engineering value
   */
  double decode(int raw) const;
};
