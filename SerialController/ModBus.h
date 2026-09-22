#pragma once

#include <Arduino.h>

/**
 * ModBus.h - Public API for the Modbus RTU implementation.
 *
 * All Modbus communication is handled exclusively by modbusTask() running
 * on Core 0. Callers never touch Serial2 directly — use these functions
 * instead; they enqueue a request and block until the task returns a result.
 */

/**
 * Creates the request queue and starts modbusTask on Core 0.
 * Must be called once from applicationSetup() before any Modbus calls.
 */
void setupModbus();

/**
 * Reads a single 16-bit register from a Modbus slave.
 * Thread-safe: may be called from any task.
 */
bool readModbusRegister(uint8_t slaveId, uint16_t regAddress, uint16_t &value);

/**
 * Writes a single 16-bit register to a Modbus slave.
 * Thread-safe: may be called from any task.
 */
bool writeModbusRegister(uint8_t slaveId, uint16_t regAddress, uint16_t value);
