#include "ModbusDevice.h"

#include <Arduino.h>

#include "../../../../src/SerialController/src/LogBuffer.h"

/**
 * ModbusDevice.cpp - Abstract base class for Modbus device drivers.
 *
 * Java equivalent: com.serial.device.base.ModbusDevice
 */

ModbusDevice::ModbusDevice(ModbusTransport* transport, uint8_t slave)
    : transport(transport), slave(slave) {
}

uint8_t ModbusDevice::getSlave() const {
  return slave;
}

const char* ModbusDevice::getDevice() const {
  return device;
}

const char* ModbusDevice::getManufacturer() const {
  return manufacturer;
}

/**
 * Returns true if both manufacturer and device strings have been populated.
 *
 * Java equivalent: ModbusDevice#isDeviceDetected
 */
bool ModbusDevice::isDeviceDetected() const {
  return (manufacturer != nullptr) && (device != nullptr);
}

/**
 * Closes and re-opens the serial transport.
 * Delegates to ModbusTransport#reconnect.
 *
 * Java equivalent: ModbusDevice#reconnect
 */
bool ModbusDevice::reconnect() {
  if (transport != nullptr) {
    return transport->reconnect();
  }
  return false;
}

/**
 * Reads a value from a device register using a DeviceRegister descriptor.
 *
 * Steps:
 *   1. Read the raw integer value from the specified Modbus register.
 *   2. Decode the value using the register's scaling factor.
 *   3. Return the value in engineering units (e.g. volts or amperes).
 *
 * Java equivalent: ModbusDevice#read(DeviceRegister)
 * Deviation: bool return + output-by-reference instead of double return / throws.
 */
bool ModbusDevice::read(const DeviceRegister& reg, double& value) {
  uint16_t raw = 0;
  if (!transport->readRegister(slave, reg.address, raw)) {
    return false;
  }
  value = reg.decode((int)raw);
  Log_debug("    -> %s: %.4f %s", reg.name, value, reg.unit ? reg.unit : "");
  return true;
}

/**
 * Reads a register using a DeviceRegister descriptor; returns raw integer value.
 *
 * Java equivalent: ModbusDevice#readInt(DeviceRegister)
 * Deviation: bool return + output-by-reference instead of int return / throws.
 */
bool ModbusDevice::readInt(const DeviceRegister& reg, int& value) {
  uint16_t raw = 0;
  if (!transport->readRegister(slave, reg.address, raw)) {
    return false;
  }
  value = (int)raw;
  return true;
}

/**
 * Writes an engineering value to a register using its scale factor.
 *
 * The value is encoded to a raw integer via the register's scaling factor
 * before being written to the Modbus register.
 *
 * Java equivalent: ModbusDevice#write(DeviceRegister, double)
 * Deviation: bool return instead of void / throws.
 */
bool ModbusDevice::write(const DeviceRegister& reg, double value) {
  Log_debug("    -> %s: %.4f %s (write)", reg.name, value, reg.unit ? reg.unit : "");
  return transport->writeRegister(slave, reg.address, (uint16_t)reg.encode(value));
}

/**
 * Writes a raw integer value to a register without scaling.
 *
 * Java equivalent: ModbusDevice#writeInt(DeviceRegister, int)
 * Deviation: bool return instead of void / throws.
 */
bool ModbusDevice::writeInt(const DeviceRegister& reg, int value) {
  return transport->writeRegister(slave, reg.address, (uint16_t)value);
}

/**
 * Writes a value and verifies it by reading back both the set register and
 * the output register. Retries up to MAX_RETRY times with a 200 ms delay
 * between attempts.
 *
 * Java equivalent: ModbusDevice#writeVerified
 * Deviation: bool return instead of void / throws.
 */
bool ModbusDevice::writeVerified(const DeviceRegister& regSet, const DeviceRegister& regOut, double value) {
  Log_info("writeVerified %s -> %.4f %s", regSet.name, value, regSet.unit ? regSet.unit : "");
  for (int attempt = 1; attempt <= MAX_RETRY; attempt++) {
    if (!write(regSet, value)) {
      continue;
    }
    vTaskDelay(pdMS_TO_TICKS(200));
    double readSet = 0.0;
    double readOut = 0.0;
    read(regSet, readSet);
    read(regOut, readOut);
    Log_info("  attempt %d: SET=%.4f %s OUT=%.4f %s",
             attempt, readSet, regSet.unit ? regSet.unit : "",
             readOut, regOut.unit ? regOut.unit : "");
    if (readSet == value) {
      Log_info("  %s verified", regSet.name);
      return true;
    }
  }
  Log_error("Failed to set %s after %d attempts", regSet.name, MAX_RETRY);
  return false;
}

/**
 * Reads a contiguous block of raw 16-bit registers in a single 0x03 frame.
 * Delegates to ModbusTransport#readRegisters.
 *
 * Java equivalent: ModbusDevice#readBlock
 * Deviation: bool return + output via pointer instead of int[] return / throws.
 */
bool ModbusDevice::readBlock(uint16_t startAddress, uint8_t count, uint16_t* values) {
  return transport->readRegisters(slave, startAddress, count, values);
}

/**
 * Writes raw 16-bit values to a contiguous block of registers in a single 0x10 frame.
 * Delegates to ModbusTransport#writeRegisters.
 *
 * Java equivalent: ModbusDevice#writeBlock
 * Deviation: bool return instead of void / throws.
 */
bool ModbusDevice::writeBlock(uint16_t startAddress, const uint16_t* values, uint8_t count) {
  return transport->writeRegisters(slave, startAddress, values, count);
}
