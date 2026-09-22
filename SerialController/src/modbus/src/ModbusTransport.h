#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * ModbusTransport.h - Low-level Modbus RTU serial framing and transport.
 *
 * Java equivalent: com.serial.modbus.ModbusTransport
 *
 * Owns the serial port (Serial2 on ESP32) and provides single-register and
 * bulk-register read/write operations. All Serial2 access must go through
 * this class — callers must never touch Serial2 directly.
 *
 * Java exceptions map to bool return values; output values are returned via
 * reference parameters (no C++ exceptions on ESP32 by default).
 */
class ModbusTransport {
 public:
  /**
   * Constructs a transport for the given UART, RX pin, TX pin and baud rate.
   *
   * @param rxPin   GPIO pin number for UART RX
   * @param txPin   GPIO pin number for UART TX
   * @param baud    baud rate (e.g. ModbusConstants::BAUD_9600)
   */
  ModbusTransport(int rxPin, int txPin, int baud);

  /**
   * Closes and re-opens the serial port at the same parameters.
   * Called after consecutive poll failures to recover from a disconnected adapter.
   *
   * @return true on success
   */
  bool reconnect();

  /**
   * Reads a single 16-bit holding register.
   *
   * @param slave    Modbus slave address
   * @param reg      register address
   * @param value    output: decoded register value
   * @return true on success
   */
  bool readRegister(uint8_t slave, uint16_t reg, uint16_t& value);

  /**
   * Reads a contiguous block of 16-bit holding registers in a single 0x03 frame.
   *
   * @param slave        Modbus slave address
   * @param startAddress first register address
   * @param count        number of registers (1–125)
   * @param values       output buffer; must hold at least count elements
   * @return true on success
   */
  bool readRegisters(uint8_t slave, uint16_t startAddress, uint8_t count, uint16_t* values);

  /**
   * Writes a single 16-bit register using function code 0x06.
   *
   * @param slave  Modbus slave address
   * @param reg    register address
   * @param value  raw 16-bit value to write
   * @return true on success
   */
  bool writeRegister(uint8_t slave, uint16_t reg, uint16_t value);

  /**
   * Writes a contiguous block of 16-bit registers in a single 0x10 frame.
   *
   * @param slave        Modbus slave address
   * @param startAddress first register address
   * @param values       raw values to write
   * @param count        number of registers (1–32)
   * @return true on success
   */
  bool writeRegisters(uint8_t slave, uint16_t startAddress, const uint16_t* values, uint8_t count);

 private:
  int rxPin;
  int txPin;
  int baud;
};
