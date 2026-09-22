#include "ModbusDevice.h"

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

bool ModbusDevice::isDeviceDetected() const {
  return (manufacturer != nullptr) && (device != nullptr);
}

bool ModbusDevice::reconnect() {
  // TODO: Port from com.serial.device.base.ModbusDevice#reconnect
  return false;
}

bool ModbusDevice::read(const DeviceRegister& reg, double& value) {
  // TODO: Port from com.serial.device.base.ModbusDevice#read(DeviceRegister)
  return false;
}

bool ModbusDevice::readInt(const DeviceRegister& reg, int& value) {
  // TODO: Port from com.serial.device.base.ModbusDevice#readInt
  return false;
}

bool ModbusDevice::write(const DeviceRegister& reg, double value) {
  // TODO: Port from com.serial.device.base.ModbusDevice#write(DeviceRegister, double)
  return false;
}

bool ModbusDevice::writeInt(const DeviceRegister& reg, int value) {
  // TODO: Port from com.serial.device.base.ModbusDevice#writeInt
  return false;
}

bool ModbusDevice::writeVerified(const DeviceRegister& regSet, const DeviceRegister& regOut, double value) {
  // TODO: Port from com.serial.device.base.ModbusDevice#writeVerified
  return false;
}

bool ModbusDevice::readBlock(uint16_t startAddress, uint8_t count, uint16_t* values) {
  // TODO: Port from com.serial.device.base.ModbusDevice#readBlock
  return false;
}

bool ModbusDevice::writeBlock(uint16_t startAddress, const uint16_t* values, uint8_t count) {
  // TODO: Port from com.serial.device.base.ModbusDevice#writeBlock
  return false;
}
