#pragma once

#include <stdint.h>

/**
 * ModbusFunctionCodes.h - Modbus RTU function codes.
 *
 * Java equivalent: com.serial.modbus.ModbusFunctionCodes
 *
 * Replaces a Java utility class with a C++ namespace of constexpr constants.
 */
namespace ModbusFunctionCodes {

/**
 * Read Holding Registers (0x03).
 * Reads one or more 16-bit registers from the device.
 * Request:  [slave][0x03][start_hi][start_lo][count_hi][count_lo][crc_lo][crc_hi]
 * Response: [slave][0x03][byte_count][data...][crc_lo][crc_hi]
 */
constexpr uint8_t READ_HOLDING_REGISTERS = 0x03;

/**
 * Write Single Register (0x06).
 * Writes a single 16-bit value to a register. Device echoes the same frame on success.
 * Request:  [slave][0x06][reg_hi][reg_lo][value_hi][value_lo][crc_lo][crc_hi]
 */
constexpr uint8_t WRITE_SINGLE_REGISTER = 0x06;

/**
 * Write Multiple Registers (0x10).
 * Writes 16-bit values to a number of consecutive registers in a single frame.
 */
constexpr uint8_t WRITE_MULTIPLE_REGISTERS = 0x10;

}  // namespace ModbusFunctionCodes
