#include "ModbusTransport.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "../../../src/SerialController/src/LogBuffer.h"
#include "../../device/src/base/DeviceRegister.h"
#include "ModbusCRC.h"
#include "ModbusConstants.h"
#include "ModbusFunctionCodes.h"

/**
 * ModbusTransport.cpp - Low-level Modbus RTU serial framing and transport.
 *
 * Java equivalent: com.serial.modbus.ModbusTransport
 *
 * Thread-safety design (matches ModBus.cpp pattern):
 *   All Serial2 I/O is handled exclusively by modbusTransportTask(), which
 *   runs on Core 0. Callers on any other task enqueue a ModbusRequest and
 *   block on a per-call response queue until the task returns a result.
 *
 * The static private helpers below (readRegister, readRegisters,
 * writeRegister, writeRegisters, readBytes, verifyCRC,
 * verifyResponseHeader) mirror the private methods of the Java class
 * exactly - same names, same logic. The only necessary deviations are:
 *   - bool return instead of throws (no C++ exceptions on ESP32)
 *   - output values via reference / pointer parameters
 */

// How long a caller waits for the Modbus task to respond (ms).
static constexpr uint32_t MODBUS_CALL_TIMEOUT_MS = 1500;

// Maximum registers in one bulk frame (Modbus spec: 125 for read, 32 for write).
static constexpr uint8_t MAX_BULK_REGS = 125;

// ---- Queue message types -----------------------------------------------

typedef enum {
  MB_OP_READ_REG,    // single-register read  (fc 0x03, count=1)
  MB_OP_WRITE_REG,   // single-register write (fc 0x06)
  MB_OP_READ_REGS,   // bulk read             (fc 0x03, count > 1)
  MB_OP_WRITE_REGS,  // bulk write            (fc 0x10)
  MB_OP_SET_BAUD,    // baud-rate change - Serial2.end()/begin() inside the task
} ModbusOp;

typedef struct {
  ModbusOp op;
  uint8_t slave;
  uint16_t startReg;
  uint16_t writeValue;          // MB_OP_WRITE_REG: value to write
  int newBaud;                  // MB_OP_SET_BAUD: new baud rate
  uint8_t count;                // MB_OP_READ_REGS / MB_OP_WRITE_REGS: register count
  uint16_t writeValues[32];     // MB_OP_WRITE_REGS: values to write (max 32)
  QueueHandle_t responseQueue;  // caller-owned single-slot response queue
} ModbusRequest;

typedef struct {
  bool success;
  uint16_t values[MAX_BULK_REGS];  // populated on successful read; [0] used for single-reg
} ModbusResponse;

// ---- Private static helpers (called only from modbusTransportTask) -----
// Names and logic mirror the private methods of com.serial.modbus.ModbusTransport.

/**
 * Derives a short human-readable annotation from a raw Modbus RTU frame.
 *
 * Java equivalent: ModbusTransport#decodeFrame
 */
static void decodeFrame(
    const char* direction,
    const uint8_t* frameData,
    int frameLength,
    char* outputBuffer,
    size_t outputBufferSize) {
  outputBuffer[0] = '\0';
  if (frameData == nullptr || frameLength < 4) {
    return;
  }
  const uint8_t functionCode = frameData[1];
  if (strcmp(direction, "TX") == 0) {
    if (frameLength >= 6) {
      const uint16_t startAddress = ((uint16_t)frameData[2] << 8) | frameData[3];
      if (functionCode == ModbusFunctionCodes::READ_HOLDING_REGISTERS) {
        const uint16_t registerCount = ((uint16_t)frameData[4] << 8) | frameData[5];
        if (registerCount == 1) {
          const char* registerName = DeviceRegister::lookupName(startAddress);
          if (registerName != nullptr) {
            snprintf(outputBuffer, outputBufferSize, "Read %s", registerName);
          } else {
            snprintf(outputBuffer, outputBufferSize, "Read 0x%04X", startAddress);
          }
          return;
        }
        snprintf(
            outputBuffer,
            outputBufferSize,
            "Read 0x%04X-0x%04X (%d regs)",
            startAddress,
            (uint16_t)(startAddress + registerCount - 1),
            (int)registerCount);
        return;
      }
      if (functionCode == ModbusFunctionCodes::WRITE_SINGLE_REGISTER) {
        const char* registerName = DeviceRegister::lookupName(startAddress);
        const uint16_t rawValue = ((uint16_t)frameData[4] << 8) | frameData[5];
        if (registerName != nullptr) {
          snprintf(outputBuffer, outputBufferSize, "Write %s = %d", registerName, (int)rawValue);
        } else {
          snprintf(outputBuffer, outputBufferSize, "Write 0x%04X = %d", startAddress, (int)rawValue);
        }
        return;
      }
      if (functionCode == ModbusFunctionCodes::WRITE_MULTIPLE_REGISTERS && frameLength >= 7) {
        const uint16_t quantity = ((uint16_t)frameData[4] << 8) | frameData[5];
        snprintf(
            outputBuffer,
            outputBufferSize,
            "Write 0x%04X-0x%04X (%d regs)",
            startAddress,
            (uint16_t)(startAddress + quantity - 1),
            (int)quantity);
        return;
      }
    }
  } else {
    if (functionCode == ModbusFunctionCodes::READ_HOLDING_REGISTERS) {
      if (frameLength == 7) {
        const uint16_t registerValue = ((uint16_t)frameData[3] << 8) | frameData[4];
        snprintf(outputBuffer, outputBufferSize, "Value = %d (0x%04X)", (int)registerValue, (unsigned)registerValue);
        return;
      }
      if (frameLength > 7) {
        const uint8_t dataByteCount = frameData[2];
        snprintf(outputBuffer, outputBufferSize, "Read %d regs, %d data bytes", (int)(dataByteCount / 2), (int)dataByteCount);
        return;
      }
    }
    if (functionCode == ModbusFunctionCodes::WRITE_SINGLE_REGISTER && frameLength >= 6) {
      const uint16_t registerAddress = ((uint16_t)frameData[2] << 8) | frameData[3];
      const char* registerName = DeviceRegister::lookupName(registerAddress);
      const uint16_t registerValue = ((uint16_t)frameData[4] << 8) | frameData[5];
      if (registerName != nullptr) {
        snprintf(outputBuffer, outputBufferSize, "Write %s = %d", registerName, (int)registerValue);
      } else {
        snprintf(outputBuffer, outputBufferSize, "Write 0x%04X = %d", registerAddress, (int)registerValue);
      }
      return;
    }
    if (functionCode == ModbusFunctionCodes::WRITE_MULTIPLE_REGISTERS && frameLength == 8) {
      const uint16_t startAddress = ((uint16_t)frameData[2] << 8) | frameData[3];
      const uint16_t quantity = ((uint16_t)frameData[4] << 8) | frameData[5];
      snprintf(
          outputBuffer,
          outputBufferSize,
          "Wrote 0x%04X-0x%04X (%d regs)",
          startAddress,
          (uint16_t)(startAddress + quantity - 1),
          (int)quantity);
      return;
    }
  }
}

/**
 * Logs a Modbus frame, formatting raw hex at DEBUG and decoded annotations at TRACE.
 *
 * Java equivalent: ModbusTransport#log
 */
static void logFrame(
    const char* direction,
    const uint8_t* frameData,
    int frameLength,
    const char* annotationHint = nullptr) {
  // Raw hex bytes at DEBUG level
  char hexBuffer[128];
  int writePosition = 0;
  writePosition += snprintf(hexBuffer + writePosition, sizeof(hexBuffer) - writePosition, "%s  ", direction);
  for (int byteIndex = 0; byteIndex < frameLength && writePosition < (int)sizeof(hexBuffer) - 4; byteIndex++) {
    writePosition += snprintf(
        hexBuffer + writePosition,
        sizeof(hexBuffer) - writePosition,
        "%02X ",
        frameData[byteIndex]);
  }
  Log_debug("%s", hexBuffer);

  // Decoded annotation at TRACE level
  char decodedText[128];
  decodeFrame(direction, frameData, frameLength, decodedText, sizeof(decodedText));
  if (decodedText[0] != '\0' || annotationHint != nullptr) {
    if (decodedText[0] != '\0' && annotationHint != nullptr) {
      Log_trace("    -> %s, %s", decodedText, annotationHint);
    } else if (decodedText[0] != '\0') {
      Log_trace("    -> %s", decodedText);
    } else if (annotationHint != nullptr) {
      Log_trace("    -> %s", annotationHint);
    }
  }
}

/**
 * Reads exactly expectedByteCount bytes from Serial2, blocking up to READ_TIMEOUT_MS.
 *
 * Java equivalent: ModbusTransport#readBytes - same partial-read accumulation loop.
 * Deviation: returns bool (false on timeout) instead of throwing RuntimeException.
 */
static bool readBytes(uint8_t* destinationBuffer, int expectedByteCount) {
  uint32_t startTime = millis();
  int bytesRead = 0;
  while (bytesRead < expectedByteCount) {
    if (millis() - startTime >= (uint32_t)ModbusConstants::READ_TIMEOUT_MS) {
      Log_warn("Serial timeout: expected %d bytes, got %d", expectedByteCount, bytesRead);
      return false;
    }
    if (Serial2.available()) {
      destinationBuffer[bytesRead++] = Serial2.read();
    } else {
      vTaskDelay(1);
    }
  }
  return true;
}

/**
 * Verifies the Modbus CRC appended to the end of the frame.
 * CRC is stored little-endian: frame[length-2]=lo, frame[length-1]=hi.
 *
 * Java equivalent: ModbusTransport#verifyCRC
 * Deviation: returns bool instead of throwing RuntimeException.
 */
static bool verifyCRC(const uint8_t* frame, int length) {
  uint16_t calc = ModbusCRC::calculate(frame, (uint8_t)(length - 2));
  uint16_t received = (uint16_t)frame[length - 2] | ((uint16_t)frame[length - 1] << 8);
  if (calc != received) {
    Log_error("CRC mismatch: calc=0x%04X received=0x%04X", calc, received);
    return false;
  }
  return true;
}

/**
 * Validates the three-byte header of a Read Holding Registers response.
 *
 * Checks:
 *   resp[0] - slave address matches request slave
 *   resp[1] - function code is 0x03
 *   resp[2] - byte count equals expectedByteCount (count * 2)
 *
 * Java equivalent: ModbusTransport#verifyResponseHeader
 * Deviation: returns bool instead of throwing RuntimeException.
 */
static bool verifyResponseHeader(const uint8_t* resp, uint8_t slave, uint8_t expectedByteCount) {
  if (resp[0] != slave) {
    Log_error("Unexpected slave: expected 0x%02X, got 0x%02X", slave, resp[0]);
    return false;
  }
  if (resp[1] != ModbusFunctionCodes::READ_HOLDING_REGISTERS) {
    Log_error("Unexpected FC: expected 0x%02X, got 0x%02X",
              ModbusFunctionCodes::READ_HOLDING_REGISTERS, resp[1]);
    return false;
  }
  if (resp[2] != expectedByteCount) {
    Log_error("Unexpected byte count: expected %d, got %d", expectedByteCount, resp[2]);
    return false;
  }
  return true;
}

/**
 * Reads a single 16-bit holding register (fc 0x03, count=1).
 *
 * Frame transmitted:
 *   [slave][0x03][reg_hi][reg_lo][00][01][crc_lo][crc_hi]
 * Response (7 bytes):
 *   [slave][0x03][0x02][value_hi][value_lo][crc_lo][crc_hi]
 *
 * Java equivalent: ModbusTransport#readRegister
 * Deviation: bool return + output-by-reference instead of int return / throws.
 */
static bool readRegister(uint8_t slave, uint16_t reg, uint16_t& value) {
  uint8_t frame[8];
  frame[0] = slave;
  frame[1] = ModbusFunctionCodes::READ_HOLDING_REGISTERS;
  frame[2] = (uint8_t)(reg >> 8);
  frame[3] = (uint8_t)reg;
  frame[4] = 0x00;
  frame[5] = 0x01;
  uint16_t crc = ModbusCRC::calculate(frame, 6);
  frame[6] = (uint8_t)crc;
  frame[7] = (uint8_t)(crc >> 8);

  logFrame("TX", frame, 8);
  while (Serial2.available()) {
    Serial2.read();
  }
  Serial2.write(frame, 8);

  uint8_t resp[7];
  if (!readBytes(resp, 7)) {
    return false;
  }
  logFrame("RX", resp, 7);
  if (!verifyCRC(resp, 7)) {
    return false;
  }
  if (!verifyResponseHeader(resp, slave, 2)) {
    return false;
  }
  value = ((uint16_t)resp[3] << 8) | resp[4];
  return true;
}

/**
 * Reads a contiguous block of count consecutive registers (fc 0x03).
 *
 * Frame transmitted:
 *   [slave][0x03][start_hi][start_lo][count_hi][count_lo][crc_lo][crc_hi]
 * Response (3 + count*2 + 2 bytes):
 *   [slave][0x03][byte_count][val0_hi][val0_lo]...[crc_lo][crc_hi]
 *
 * Java equivalent: ModbusTransport#readRegisters
 * Deviation: bool return + output via pointer instead of int[] return / throws.
 */
static bool readRegisters(uint8_t slave, uint16_t startReg, uint8_t count, uint16_t* values) {
  if (count < 1 || count > ModbusConstants::MAX_READ_REGISTERS) {
    Log_error("readRegisters() called with invalid count %d (must be 1-%d); this is a programming error",
              (int)count, ModbusConstants::MAX_READ_REGISTERS);
    return false;
  }
  uint8_t frame[8];
  frame[0] = slave;
  frame[1] = ModbusFunctionCodes::READ_HOLDING_REGISTERS;
  frame[2] = (uint8_t)(startReg >> 8);
  frame[3] = (uint8_t)startReg;
  frame[4] = (uint8_t)(count >> 8);
  frame[5] = (uint8_t)count;
  uint16_t crc = ModbusCRC::calculate(frame, 6);
  frame[6] = (uint8_t)crc;
  frame[7] = (uint8_t)(crc >> 8);

  logFrame("TX", frame, 8);
  while (Serial2.available()) {
    Serial2.read();
  }
  Serial2.write(frame, 8);

  // Response: [slave][fc][byte_count][val_hi][val_lo]... × count [crc_lo][crc_hi]
  int respLen = 3 + count * 2 + 2;
  uint8_t resp[3 + MAX_BULK_REGS * 2 + 2];
  if (!readBytes(resp, respLen)) {
    return false;
  }
  logFrame("RX", resp, respLen);
  if (!verifyCRC(resp, respLen)) {
    return false;
  }
  if (!verifyResponseHeader(resp, slave, (uint8_t)(count * 2))) {
    return false;
  }
  for (int i = 0; i < count; i++) {
    values[i] = ((uint16_t)resp[3 + i * 2] << 8) | resp[4 + i * 2];
  }
  return true;
}

/**
 * Writes a 16-bit value to a single holding register (fc 0x06).
 * Device echoes the same 8-byte frame back on success.
 *
 * Frame transmitted:
 *   [slave][0x06][reg_hi][reg_lo][value_hi][value_lo][crc_lo][crc_hi]
 *
 * Java equivalent: ModbusTransport#writeRegister
 * Deviation: bool return instead of void / throws.
 */
static bool writeRegister(uint8_t slave, uint16_t reg, uint16_t value) {
  uint8_t frame[8];
  frame[0] = slave;
  frame[1] = ModbusFunctionCodes::WRITE_SINGLE_REGISTER;
  frame[2] = (uint8_t)(reg >> 8);
  frame[3] = (uint8_t)reg;
  frame[4] = (uint8_t)(value >> 8);
  frame[5] = (uint8_t)value;
  uint16_t crc = ModbusCRC::calculate(frame, 6);
  frame[6] = (uint8_t)crc;
  frame[7] = (uint8_t)(crc >> 8);

  logFrame("TX", frame, 8);
  while (Serial2.available()) {
    Serial2.read();
  }
  Serial2.write(frame, 8);

  uint8_t resp[8];
  if (!readBytes(resp, 8)) {
    return false;
  }
  logFrame("RX", resp, 8);
  if (!verifyCRC(resp, 8)) {
    return false;
  }
  return true;
}

/**
 * Writes 16-bit values to a contiguous block of registers (fc 0x10).
 *
 * Frame transmitted (7 + count*2 + 2 bytes):
 *   [slave][0x10][start_hi][start_lo][qty_hi][qty_lo][byte_count]
 *   [val0_hi][val0_lo]...[crc_lo][crc_hi]
 * Response (8 bytes):
 *   [slave][0x10][start_hi][start_lo][qty_hi][qty_lo][crc_lo][crc_hi]
 *
 * Java equivalent: ModbusTransport#writeRegisters
 * Deviation: bool return instead of void / throws.
 */
static bool writeRegisters(uint8_t slave, uint16_t startReg, const uint16_t* values, uint8_t count) {
  const uint8_t byteCount = count * 2;
  const int frameLen = 7 + byteCount + 2;
  uint8_t frame[7 + 32 * 2 + 2];  // max 32 registers = 71 bytes
  frame[0] = slave;
  frame[1] = ModbusFunctionCodes::WRITE_MULTIPLE_REGISTERS;
  frame[2] = (uint8_t)(startReg >> 8);
  frame[3] = (uint8_t)startReg;
  frame[4] = (uint8_t)(count >> 8);
  frame[5] = (uint8_t)count;
  frame[6] = byteCount;
  for (int i = 0; i < count; i++) {
    frame[7 + i * 2] = (uint8_t)(values[i] >> 8);
    frame[7 + i * 2 + 1] = (uint8_t)values[i];
  }
  uint16_t crc = ModbusCRC::calculate(frame, (uint8_t)(frameLen - 2));
  frame[frameLen - 2] = (uint8_t)crc;
  frame[frameLen - 1] = (uint8_t)(crc >> 8);

  logFrame("TX", frame, frameLen);
  while (Serial2.available()) {
    Serial2.read();
  }
  Serial2.write(frame, frameLen);

  // Response: [slave][0x10][start_hi][start_lo][qty_hi][qty_lo][crc_lo][crc_hi]
  uint8_t resp[8];
  if (!readBytes(resp, 8)) {
    return false;
  }
  logFrame("RX", resp, 8);
  if (!verifyCRC(resp, 8)) {
    return false;
  }
  return true;
}

// ---- FreeRTOS task (Core 0) --------------------------------------------

/**
 * Sole owner of Serial2. Dequeues requests, executes them, and returns
 * results via per-request response queues.
 * Pinned to Core 0 alongside the WiFi/WebServer stack.
 */
void modbusTransportTask(void* param) {
  ModbusTransport* self = static_cast<ModbusTransport*>(param);
  QueueHandle_t requestQueue = static_cast<QueueHandle_t>(self->_requestQueue);

  ModbusRequest req;
  for (;;) {
    if (xQueueReceive(requestQueue, &req, portMAX_DELAY) == pdTRUE) {
      ModbusResponse resp;
      resp.success = false;

      switch (req.op) {
        case MB_OP_READ_REG:
          resp.success = readRegister(req.slave, req.startReg, resp.values[0]);
          break;
        case MB_OP_READ_REGS:
          resp.success = readRegisters(req.slave, req.startReg, req.count, resp.values);
          break;
        case MB_OP_WRITE_REG:
          resp.success = writeRegister(req.slave, req.startReg, req.writeValue);
          break;
        case MB_OP_WRITE_REGS:
          resp.success = writeRegisters(req.slave, req.startReg, req.writeValues, req.count);
          break;
        case MB_OP_SET_BAUD:
          // Reconfigure Serial2 from inside the task so no other operation
          // can interleave with Serial2.end() / Serial2.begin().
          Serial2.end();
          self->_baud = req.newBaud;
          Serial2.begin(self->_baud, SERIAL_8N1, self->_rxPin, self->_txPin);
          Log_info("Serial2 re-opened at %d baud (setBaud via task).", self->_baud);
          resp.success = true;
          break;
      }

      xQueueSend(req.responseQueue, &resp, portMAX_DELAY);
    }
  }
}

// ---- ModbusTransport public methods ------------------------------------

ModbusTransport::ModbusTransport(int rxPin, int txPin, int baud)
    : _rxPin(rxPin), _txPin(txPin), _baud(baud) {
  Serial2.begin(_baud, SERIAL_8N1, _rxPin, _txPin);
  Log_info("Serial2 initialised at %d baud (RX=%d TX=%d)", _baud, _rxPin, _txPin);

  _requestQueue = xQueueCreate(4, sizeof(ModbusRequest));
  xTaskCreatePinnedToCore(modbusTransportTask, "Modbus_Task", 4096, this, 2, NULL, 0);
  Log_info("Modbus task started on Core 0.");
}

/**
 * Closes and re-opens the serial port at the same parameters.
 * Called by ModbusDevice::reconnect() after consecutive poll failures.
 * Routes through the task queue to avoid racing with readBytes().
 *
 * Java equivalent: ModbusTransport#reconnect
 */
bool ModbusTransport::reconnect() {
  // Reuse setBaud() with the current baud - this routes through the task
  // queue, so Serial2.end()/begin() executes inside modbusTransportTask.
  setBaud(_baud);
  Log_info("Serial2 reconnected at %d baud.", _baud);
  return true;
}

/**
 * Changes the baud rate and re-opens Serial2 at the new rate.
 * Used by DeviceDetection to probe without reconstructing the transport.
 *
 * Java equivalent: new ModbusTransport(portName, baud) inside the
 * verifyDevicePresent() probing loop - C++ reconfigures in-place.
 */
void ModbusTransport::setBaud(int baud) {
  // Route through the task queue so the baud change is serialised with all
  // other Serial2 operations - avoids Serial2.end() racing with readBytes().
  QueueHandle_t respQ = xQueueCreate(1, sizeof(ModbusResponse));

  ModbusRequest req;
  req.op = MB_OP_SET_BAUD;
  req.newBaud = baud;
  req.responseQueue = respQ;

  xQueueSend(static_cast<QueueHandle_t>(_requestQueue), &req, portMAX_DELAY);

  ModbusResponse resp;
  xQueueReceive(respQ, &resp, pdMS_TO_TICKS(MODBUS_CALL_TIMEOUT_MS));
  vQueueDelete(respQ);
}

/**
 * Public wrapper: enqueues a single-register read and blocks for the result.
 *
 * Java equivalent: ModbusTransport#readRegister (public method)
 */
bool ModbusTransport::readRegister(uint8_t slave, uint16_t reg, uint16_t& value) {
  QueueHandle_t respQ = xQueueCreate(1, sizeof(ModbusResponse));

  ModbusRequest req;
  req.op = MB_OP_READ_REG;
  req.slave = slave;
  req.startReg = reg;
  req.writeValue = 0;
  req.count = 1;
  req.responseQueue = respQ;

  xQueueSend(static_cast<QueueHandle_t>(_requestQueue), &req, portMAX_DELAY);

  ModbusResponse resp;
  bool timedOut = (xQueueReceive(respQ, &resp, pdMS_TO_TICKS(MODBUS_CALL_TIMEOUT_MS)) != pdTRUE);
  vQueueDelete(respQ);

  if (timedOut) {
    Log_error("readRegister: caller timed out waiting for task.");
    return false;
  }
  if (resp.success) {
    value = resp.values[0];
  }
  return resp.success;
}

/**
 * Public wrapper: enqueues a bulk register read and blocks for the result.
 *
 * Java equivalent: ModbusTransport#readRegisters (public method)
 */
bool ModbusTransport::readRegisters(uint8_t slave, uint16_t startAddress, uint8_t count, uint16_t* values) {
  QueueHandle_t respQ = xQueueCreate(1, sizeof(ModbusResponse));

  ModbusRequest req;
  req.op = MB_OP_READ_REGS;
  req.slave = slave;
  req.startReg = startAddress;
  req.count = count;
  req.responseQueue = respQ;

  xQueueSend(static_cast<QueueHandle_t>(_requestQueue), &req, portMAX_DELAY);

  ModbusResponse resp;
  bool timedOut = (xQueueReceive(respQ, &resp, pdMS_TO_TICKS(MODBUS_CALL_TIMEOUT_MS)) != pdTRUE);
  vQueueDelete(respQ);

  if (timedOut) {
    Log_error("readRegisters: caller timed out waiting for task.");
    return false;
  }
  if (resp.success) {
    for (int i = 0; i < count; i++) {
      values[i] = resp.values[i];
    }
  }
  return resp.success;
}

/**
 * Public wrapper: enqueues a single-register write and blocks for the result.
 *
 * Java equivalent: ModbusTransport#writeRegister (public method)
 */
bool ModbusTransport::writeRegister(uint8_t slave, uint16_t reg, uint16_t value) {
  QueueHandle_t respQ = xQueueCreate(1, sizeof(ModbusResponse));

  ModbusRequest req;
  req.op = MB_OP_WRITE_REG;
  req.slave = slave;
  req.startReg = reg;
  req.writeValue = value;
  req.count = 1;
  req.responseQueue = respQ;

  xQueueSend(static_cast<QueueHandle_t>(_requestQueue), &req, portMAX_DELAY);

  ModbusResponse resp;
  bool timedOut = (xQueueReceive(respQ, &resp, pdMS_TO_TICKS(MODBUS_CALL_TIMEOUT_MS)) != pdTRUE);
  vQueueDelete(respQ);

  if (timedOut) {
    Log_error("writeRegister: caller timed out waiting for task.");
    return false;
  }
  return resp.success;
}

/**
 * Public wrapper: enqueues a bulk register write and blocks for the result.
 *
 * Java equivalent: ModbusTransport#writeRegisters (public method)
 */
bool ModbusTransport::writeRegisters(uint8_t slave, uint16_t startAddress, const uint16_t* values, uint8_t count) {
  QueueHandle_t respQ = xQueueCreate(1, sizeof(ModbusResponse));

  ModbusRequest req;
  req.op = MB_OP_WRITE_REGS;
  req.slave = slave;
  req.startReg = startAddress;
  req.count = count;
  for (int i = 0; i < count; i++) {
    req.writeValues[i] = values[i];
  }
  req.responseQueue = respQ;

  xQueueSend(static_cast<QueueHandle_t>(_requestQueue), &req, portMAX_DELAY);

  ModbusResponse resp;
  bool timedOut = (xQueueReceive(respQ, &resp, pdMS_TO_TICKS(MODBUS_CALL_TIMEOUT_MS)) != pdTRUE);
  vQueueDelete(respQ);

  if (timedOut) {
    Log_error("writeRegisters: caller timed out waiting for task.");
    return false;
  }
  return resp.success;
}
