#include "ModbusCRC.h"

/**
 * ModbusCRC.cpp - Modbus CRC-16 checksum calculation.
 *
 * Java equivalent: com.serial.modbus.ModbusCRC
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
 * Java equivalent: ModbusCRC#calculate
 */
uint16_t calculate(const uint8_t* data, uint8_t len) {
  uint16_t crc = 0xFFFF;
  for (int i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i];
    for (int j = 0; j < 8; j++) {
      if ((crc & 0x0001) != 0) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

}  // namespace ModbusCRC
