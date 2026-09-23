#pragma once

#include <stdint.h>

/**
 * ModbusConstants.h - Modbus protocol constants.
 *
 * Java equivalent: com.serial.modbus.ModbusConstants
 *
 * Replaces a Java utility class (private constructor, static final fields)
 * with a C++ namespace of constexpr constants - the idiomatic equivalent.
 */
namespace ModbusConstants {

/** Default data bits. */
constexpr int DATABITS_8 = 8;

/** Default baud rate. */
constexpr int BAUD_9600 = 9600;
constexpr int BAUD_19200 = 19200;
constexpr int BAUD_38400 = 38400;
constexpr int BAUD_57600 = 57600;
constexpr int BAUD_115200 = 115200;

/** Default Modbus slave address. */
constexpr uint8_t SLAVE_ADDRESS_1 = 0x01;

/**
 * Read timeout in milliseconds.
 * 200 ms is generous for any supported device - real responses arrive in <50 ms.
 * Keeping it low is critical during device detection: with 4 drivers × 5 baud
 * rates, a 1000 ms timeout would make a full scan take ~20 s and trip the TWDT.
 */
constexpr int READ_TIMEOUT_MS = 100;

/** Write timeout in milliseconds (0 = non-blocking). */
constexpr int WRITE_TIMEOUT_MS = 0;

/** Generic digital state ON / active / locked / enabled. */
constexpr int STATE_ON = 1;

/** Generic digital state OFF / inactive / unlocked / disabled. */
constexpr int STATE_OFF = 0;

/**
 * Maximum number of holding registers per single Modbus 0x03 request.
 * Per Modbus Application Protocol spec, quantity is limited to 0x007D (125).
 */
constexpr int MAX_READ_REGISTERS = 125;

}  // namespace ModbusConstants
