#pragma once

#include <stdint.h>

/**
 * ModbusCRC.h - Modbus CRC-16 checksum calculation.
 *
 * Java equivalent: com.serial.modbus.ModbusCRC
 */

/**
 * Calculates the Modbus CRC-16 checksum for the given buffer.
 *
 * @param buffer pointer to the data bytes
 * @param length number of bytes to include in the calculation
 * @return 16-bit CRC value
 */
uint16_t modbusCalculateCRC(const uint8_t* buffer, uint8_t length);
