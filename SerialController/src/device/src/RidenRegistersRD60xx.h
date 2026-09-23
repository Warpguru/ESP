#pragma once

#include <stdint.h>

/**
 * RidenRegistersRD60xx.h - Modbus register address map for Riden RD60xx series.
 *
 * Java equivalent: com.serial.device.RidenRegistersRD60xx
 *
 * In Java this is a final class with only static final int constants.
 * In C++ the equivalent is a class with only static constexpr uint16_t constants
 * (no instances needed — same semantics as Java's private constructor pattern).
 *
 * Scaling factors for the RD60xx series:
 *   Voltage values: raw / 100  = volts
 *   Current values: raw / 1000 = amperes
 *   Power values:   raw / 100  = watts
 *
 * DeviceRegister descriptors (VSET, ISET, VOUT, etc.) are declared in
 * RidenRD60xx and use these addresses — same split as the Java code.
 */
class RidenRegistersRD60xx {
 public:
  /** Device model identification. Read only. Example: 60062 = RD6006 rev 2. */
  static constexpr uint16_t REG_DEVICE_ID = 0x0000;

  /** Serial number high word. Read only. */
  static constexpr uint16_t REG_SERIAL_HIGH = 0x0001;

  /** Serial number low word. Read only. */
  static constexpr uint16_t REG_SERIAL_LOW = 0x0002;

  /** Firmware version. Read only. Scaling: raw / 100 = version number. */
  static constexpr uint16_t REG_FIRMWARE = 0x0003;

  /**
   * Internal temperature sign (Celsius). Read only.
   * 0 = positive, 1 = negative.
   */
  static constexpr uint16_t REG_TEMP_SIGN_CELSIUS = 0x0004;

  /** Internal temperature magnitude in degrees Celsius. Read only. */
  static constexpr uint16_t REG_TEMP_CELSIUS = 0x0005;

  /** Temperature sign in Fahrenheit. Read only. 0 = positive, 1 = negative. */
  static constexpr uint16_t REG_TEMP_SIGN_FAHRENHEIT = 0x0006;

  /** Temperature value in Fahrenheit. Read only. */
  static constexpr uint16_t REG_TEMP_FAHRENHEIT = 0x0007;

  /**
   * Voltage setpoint. Read/Write.
   * Scaling: raw / 100 = volts. Example: 500 raw = 5.00 V.
   */
  static constexpr uint16_t REG_VSET = 0x0008;

  /**
   * Current setpoint. Read/Write.
   * Scaling: raw / 1000 = amperes.
   */
  static constexpr uint16_t REG_ISET = 0x0009;

  /**
   * Measured output voltage. Read only.
   * Scaling: raw / 100 = volts.
   */
  static constexpr uint16_t REG_VOUT = 0x000A;

  /**
   * Measured output current. Read only.
   * Scaling: raw / 1000 = amperes.
   */
  static constexpr uint16_t REG_IOUT = 0x000B;

  /** Accumulated ampere-hours. Read only. */
  static constexpr uint16_t REG_AH = 0x000C;

  /**
   * Measured output power. Read only.
   * Scaling: raw / 100 = watts.
   */
  static constexpr uint16_t REG_POUT = 0x000D;

  /**
   * Input voltage measurement. Read only.
   * Scaling: raw / 100 = volts.
   */
  static constexpr uint16_t REG_VIN = 0x000E;

  /**
   * Keypad lock state. Read/Write.
   * 0 = unlocked, 1 = locked.
   */
  static constexpr uint16_t REG_KEYPAD_LOCK = 0x000F;

  /**
   * Protection status. Read only.
   * 0 = normal, 1 = OVP, 2 = OCP.
   */
  static constexpr uint16_t REG_PROTECTION_STATE = 0x0010;

  /**
   * Regulation mode. Read only.
   * 0 = CV (constant voltage), 1 = CC (constant current).
   */
  static constexpr uint16_t REG_MODE = 0x0011;

  /**
   * Output enable control. Read/Write.
   * 0 = output OFF, 1 = output ON.
   */
  static constexpr uint16_t REG_OUTPUT_ENABLE = 0x0012;

  /**
   * Preset memory selector. Read/Write.
   * Values typically 1–9.
   */
  static constexpr uint16_t REG_PRESET = 0x0013;

  /**
   * Current range selection (model dependent). Read/Write.
   * 0 = low current range, 1 = high current range.
   */
  static constexpr uint16_t REG_CURRENT_RANGE = 0x0014;

 private:
  // Not instantiable — all members are static constants.
  // Java equivalent: private constructor in RidenRegistersRD60xx.java
  RidenRegistersRD60xx() = delete;
};
