#include "DeviceRegister.h"

#include <math.h>

/**
 * DeviceRegister.cpp - Modbus register descriptor implementation.
 *
 * Java equivalent: com.serial.device.base.DeviceRegister
 */

DeviceRegister::DeviceRegister(const char* name, const char* unit, uint16_t address, double scale)
    : name(name), unit(unit), address(address), scale(scale) {
  // TODO: register in global registry for transport log annotation
}

int DeviceRegister::encode(double value) const {
  return (int)round(value * scale);
}

double DeviceRegister::decode(int raw) const {
  return raw / scale;
}
