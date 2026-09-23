#pragma once

#include <stdint.h>

/**
 * ModbusCRC.h - Modbus CRC-16 checksum calculation.
 *
 * Java equivalent: com.serial.modbus.ModbusCRC
 *
 * Replaces a Java utility class (private constructor, static method) with a
 * C++ namespace — the idiomatic equivalent, matching ModbusConstants.h and
 * ModbusFunctionCodes.h.
 */
namespace ModbusCRC {

/**
 * Calculates the Modbus RTU CRC-16 checksum for a frame.
 *
 * Uses the standard Modbus CRC algorithm with polynomial 0xA001 and an
 * initial value of 0xFFFF. Calculated over all bytes except the CRC field
 * itself. The result is appended in little-endian order: [crc_lo][crc_hi].
 *
 * Example frame before CRC:  01 03 00 02 00 01
 * Example full frame:         01 03 00 02 00 01 25 CA
 *
 * @param data   pointer to the frame bytes
 * @param len    number of bytes to include in the calculation
 * @return 16-bit CRC value (low byte transmitted first)
 *
 * Java equivalent: ModbusCRC.calculate(data, len)
 */
uint16_t calculate(const uint8_t* data, uint8_t len);

}  // namespace ModbusCRC
