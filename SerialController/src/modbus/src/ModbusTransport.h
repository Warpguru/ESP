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
 * Thread-safety design:
 *   All Serial2 I/O is handled exclusively by an internal FreeRTOS task
 *   (modbusTask) pinned to Core 0 alongside the WiFi/WebServer stack.
 *   Callers on any other task use readRegister / writeRegister /
 *   readRegisters / writeRegisters, which enqueue a request and block until
 *   the task returns a result via a per-call response queue.
 *
 * Java exceptions map to bool return values; output values are returned via
 * reference / pointer parameters (no C++ exceptions on ESP32 by default).
 */
class ModbusTransport {
 public:
  /**
   * Constructs a transport for the given RX pin, TX pin and baud rate.
   * Initialises Serial2 and starts the internal modbusTask on Core 0.
   *
   * @param rxPin  GPIO pin number for UART RX
   * @param txPin  GPIO pin number for UART TX
   * @param baud   baud rate (e.g. ModbusConstants::BAUD_9600)
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
   * Reads a single 16-bit holding register (function code 0x03).
   *
   * Frame transmitted:
   *   [slave][0x03][reg_hi][reg_lo][00][01][crc_lo][crc_hi]
   * Response expected:
   *   [slave][0x03][0x02][value_hi][value_lo][crc_lo][crc_hi]
   *
   * @param slave  Modbus slave address
   * @param reg    register address
   * @param value  output: raw 16-bit register value
   * @return true on success
   */
  bool readRegister(uint8_t slave, uint16_t reg, uint16_t& value);

  /**
   * Reads a contiguous block of 16-bit holding registers in a single 0x03 frame.
   *
   * More efficient than calling readRegister in a loop: one serial round-trip
   * regardless of how many registers are requested.
   *
   * Frame transmitted:
   *   [slave][0x03][start_hi][start_lo][count_hi][count_lo][crc_lo][crc_hi]
   * Response (3 + count*2 + 2 bytes):
   *   [slave][0x03][byte_count][val0_hi][val0_lo]...[crc_lo][crc_hi]
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
   * Device echoes the same frame back on success.
   *
   * Frame transmitted:
   *   [slave][0x06][reg_hi][reg_lo][value_hi][value_lo][crc_lo][crc_hi]
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
   * More efficient than calling writeRegister in a loop when two or more
   * adjacent registers must be updated atomically (e.g. VSET and ISET).
   *
   * Frame transmitted (7 + count*2 + 2 bytes):
   *   [slave][0x10][start_hi][start_lo][qty_hi][qty_lo][byte_count]
   *   [val0_hi][val0_lo]...[crc_lo][crc_hi]
   * Response (8 bytes):
   *   [slave][0x10][start_hi][start_lo][qty_hi][qty_lo][crc_lo][crc_hi]
   *
   * @param slave        Modbus slave address
   * @param startAddress first register address
   * @param values       raw values to write
   * @param count        number of registers (1–32)
   * @return true on success
   */
  bool writeRegisters(uint8_t slave, uint16_t startAddress, const uint16_t* values, uint8_t count);

 private:
  int _rxPin;
  int _txPin;
  int _baud;

  // FreeRTOS queue handle — receives ModbusRequest items from callers.
  // The internal modbusTask is the sole consumer; started in the constructor.
  void* _requestQueue;  // QueueHandle_t stored as void* to avoid Arduino.h in header

  friend void modbusTransportTask(void* param);
};
