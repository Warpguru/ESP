#include "DeviceRegister.h"

#include <math.h>

/**
 * DeviceRegister.cpp - Modbus register descriptor implementation.
 *
 * Java equivalent: com.serial.device.base.DeviceRegister
 */

// Simple static registry mapping addresses to names for log annotations.
static constexpr size_t MAX_REGISTRY_ENTRIES = 64;

struct RegistryEntry {
  uint16_t address;
  const char* name;
};

static RegistryEntry registryEntries[MAX_REGISTRY_ENTRIES];
static size_t registryCount = 0;

const char* DeviceRegister::lookupName(uint16_t address, const char* defaultName) {
  for (size_t index = 0; index < registryCount; index++) {
    if (registryEntries[index].address == address) {
      return registryEntries[index].name;
    }
  }
  return defaultName;
}

DeviceRegister::DeviceRegister(const char* name, const char* unit, uint16_t address, double scale)
    : name(name), unit(unit), address(address), scale(scale) {
  for (size_t index = 0; index < registryCount; index++) {
    if (registryEntries[index].address == address) {
      registryEntries[index].name = name;
      return;
    }
  }
  if (registryCount < MAX_REGISTRY_ENTRIES) {
    registryEntries[registryCount++] = {address, name};
  }
}

int DeviceRegister::encode(double value) const {
  return (int)round(value * scale);
}

double DeviceRegister::decode(int raw) const {
  return raw / scale;
}
