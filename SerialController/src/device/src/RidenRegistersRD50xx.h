#pragma once

/**
 * RidenRegistersRD50xx.h - Modbus register address map for Ruideng DPS/RD50xx series.
 *
 * Java equivalent: com.serial.device.RidenRegistersRD50xx
 *
 * Source: Hangzhou Rui Deng Technology Co., Ltd — DPS5020 Digital power
 * communication protocol V1.2 (doc/DPS5020 communication protocol V1.2.pdf).
 *
 * The DPS50xx register layout is distinct from both Sinilink and RD60xx:
 *   - Register 0x0000 is U-SET (voltage setpoint), NOT a model ID.
 *   - Register 0x000B is MODEL (product number), NOT an energy counter.
 *   - Register 0x000C is VERSON (firmware version), NOT an energy counter.
 *   - No temperature registers (getTemperatureCelsius() returns -999.0).
 *
 * Scaling factors (2 decimal places throughout):
 *   Voltage : raw / 100 = volts
 *   Current : raw / 100 = amperes
 *   Power   : raw / 100 = watts
 */
namespace RidenRegistersRD50xx {

/** U-SET - Voltage setpoint. R/W. Scale: raw / 100 = volts. */
constexpr int REG_VSET = 0x0000;

/** I-SET - Current setpoint. R/W. Scale: raw / 100 = amperes. */
constexpr int REG_ISET = 0x0001;

/** UOUT - Measured output voltage. R. Scale: raw / 100 = volts. */
constexpr int REG_VOUT = 0x0002;

/** IOUT - Measured output current. R. Scale: raw / 100 = amperes. */
constexpr int REG_IOUT = 0x0003;

/** POWER - Measured output power. R. Scale: raw / 100 = watts. */
constexpr int REG_POUT = 0x0004;

/** UIN - Input voltage. R. Scale: raw / 100 = volts. */
constexpr int REG_VIN = 0x0005;

/** LOCK - Keypad lock. R/W. 0 = unlocked, 1 = locked. */
constexpr int REG_KEYPAD_LOCK = 0x0006;

/**
 * PROTECT - Protection state. R.
 * 0 = normal, 1 = OVP (over-voltage), 2 = OCP (over-current), 3 = OPP (over-power).
 */
constexpr int REG_PROTECTION_STATE = 0x0007;

/** CVCC - Regulation mode. R. 0 = CV (constant voltage), 1 = CC (constant current). */
constexpr int REG_MODE = 0x0008;

/** ONOFF - Output switch. R/W. 0 = OFF, 1 = ON. */
constexpr int REG_OUTPUT_ENABLE = 0x0009;

/** B_LED - Backlight brightness level. R/W. 0 = darkest, 5 = brightest. */
constexpr int REG_BACKLIGHT = 0x000A;

/**
 * MODEL - Product model number. R.
 * Returns a 4-digit integer identifying the model (e.g. 5020 for DPS5020).
 * Used by verifyDevicePresent() for device identification.
 * Confirmed on real hardware: DPS5020 returns 5020 at 9600 baud.
 */
constexpr int REG_DEVICE_ID = 0x000B;

/**
 * VERSON - Firmware version. R.
 * Raw register / 10.0 = version (e.g. 17 = v1.7).
 * Several DPS5020 factory batches always return 0 — known hardware limitation.
 */
constexpr int REG_FIRMWARE = 0x000C;

/**
 * EXTRACT_M - Data-set recall. W.
 * Writing 0–9 recalls the corresponding preset (M0–M9) into working registers.
 */
constexpr int REG_EXTRACT_M = 0x0023;

}  // namespace RidenRegistersRD50xx
