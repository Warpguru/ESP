#pragma once

/**
 * WuzhiRegisters.h - Modbus register address map for Wuzhi ZK-series.
 *
 * Java equivalent: com.serial.device.WuzhiRegisters
 *
 * The Wuzhi ZK-series register map is address-for-address identical to
 * SinilinkRegisters. The single critical difference is current scaling:
 *   ISET / IOUT use scale 100 (10 mA resolution) - NOT scale 1000 as on
 *   the Sinilink XY6008/XY6014 class. Using the wrong scale produces a 10×
 *   current error.
 *
 * Scaling factors (see doc/SerialController.md - Register Scaling table):
 *   Voltage : raw / 100 = volts
 *   Current : raw / 100 = amperes  ← DIFFERENT from Sinilink XY6008/XY6014
 *   Power   : raw / 100 = watts
 *   Temp    : raw / 10  = °C
 *
 * Memory model:
 *   M0 (0x0050) = active working set; M1–M10 = stored presets (11 groups,
 *   one more than Sinilink/Riden). Registers 0x0000/0x0001 mirror 0x0050/0x0051.
 */
namespace WuzhiRegisters {

/** VSET - Voltage setpoint. R/W. Scale: raw / 100 = volts. */
constexpr int REG_VSET = 0x0000;

/**
 * ISET - Current setpoint. R/W. Scale: raw / 100 = amperes (10 mA resolution).
 * NOTE: scale 100, not 1000. Using 1000 produces a 10× current error.
 */
constexpr int REG_ISET = 0x0001;

/** VOUT - Measured output voltage. R. Scale: raw / 100 = volts. */
constexpr int REG_VOUT = 0x0002;

/**
 * IOUT - Measured output current. R. Scale: raw / 100 = amperes.
 * NOTE: scale 100, same as ISET.
 */
constexpr int REG_IOUT = 0x0003;

/** POUT - Measured output power. R. Scale: raw / 100 = watts. */
constexpr int REG_POUT = 0x0004;

/** VIN - Input voltage. R. Scale: raw / 100 = volts. */
constexpr int REG_VIN = 0x0005;

/** AH_LOW - Accumulated capacity low 16 bits (mAh). R. */
constexpr int REG_AH_LOW = 0x0006;

/** AH_HIGH - Accumulated capacity high 16 bits (mAh). R. */
constexpr int REG_AH_HIGH = 0x0007;

/** WH_LOW - Accumulated energy low 16 bits (mWh). R. */
constexpr int REG_WH_LOW = 0x0008;

/** WH_HIGH - Accumulated energy high 16 bits (mWh). R. */
constexpr int REG_WH_HIGH = 0x0009;

/** OUT_HOURS - Output ON time hours. R. */
constexpr int REG_OUT_HOURS = 0x000A;

/** OUT_MINUTES - Output ON time minutes. R. */
constexpr int REG_OUT_MINUTES = 0x000B;

/** OUT_SECONDS - Output ON time seconds. R. */
constexpr int REG_OUT_SECONDS = 0x000C;

/** TEMP - Internal temperature. R. Scale: raw / 10 = °C. */
constexpr int REG_TEMPERATURE_INTERNAL = 0x000D;

/** TEMP_EXT - External temperature. R. Scale: raw / 10 = °C. */
constexpr int REG_TEMPERATURE_EXTERNAL = 0x000E;

/** LOCK - Keypad lock. R/W. 0 = unlocked, 1 = locked. */
constexpr int REG_KEYPAD_LOCK = 0x000F;

/**
 * PROTECT - Protection state. R/W. Write 0 to clear.
 * 0=normal, 1=OVP, 2=OCP, 3=OPP, 4=LVP, 5=OAH, 6=OHP, 7=OTP, 8=OEP, 9=OWH, 10=ICP.
 */
constexpr int REG_PROTECTION_STATE = 0x0010;

/** MODE - Regulation mode. R. 0 = CV (constant voltage), 1 = CC (constant current). */
constexpr int REG_MODE = 0x0011;

/** OUTPUT - Output enable. R/W. 0 = OFF, 1 = ON. */
constexpr int REG_OUTPUT_ENABLE = 0x0012;

/** TEMP_UNIT - Temperature unit. R/W. 0 = °C, 1 = °F. */
constexpr int REG_TEMP_UNIT = 0x0013;

/** BACKLIGHT - Backlight brightness. R/W. Range: 0–5. */
constexpr int REG_BACKLIGHT = 0x0014;

/** SLEEP - Display sleep timeout (minutes). R/W. */
constexpr int REG_SLEEP = 0x0015;

/**
 * MODEL - Product model number register. R.
 * Used by verifyDevicePresent() for device identification.
 * Modern packed values: high byte = 0x59 ('Y'), low byte = board revision.
 * The exact value for the ZK-6522C has not yet been confirmed on live hardware.
 */
constexpr int REG_MODEL = 0x0016;

/**
 * FIRMWARE - Firmware version. R. Scale: raw / 100 = version.
 * Example: 110 → v1.10.
 */
constexpr int REG_FIRMWARE = 0x0017;

/** SLAVE_ADDR - Modbus slave address. R/W. */
constexpr int REG_SLAVE_ADDRESS = 0x0018;

/** BAUDRATE - Baud rate selector. R/W. */
constexpr int REG_BAUDRATE = 0x0019;

/**
 * MEMORY_RECALL - Memory recall (M0–M10). R/W.
 * Writing 1–10 loads preset into M0. ZK-series supports 11 groups (M0–M10),
 * one more than Sinilink/Riden (M0–M9).
 */
constexpr int REG_MEMORY_RECALL = 0x001D;

// ---- M0 active working-set preset registers (base 0x0050) ----------------

/** M0 VSET mirror. R/W. */
constexpr int REG_MEMORY_M0_VSET = 0x0050;

/** M0 ISET mirror. R/W. */
constexpr int REG_MEMORY_M0_ISET = 0x0051;

}  // namespace WuzhiRegisters
