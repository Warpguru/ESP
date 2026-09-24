#pragma once

#include "DeviceLimits.h"

/**
 * DeviceCatalogue.h - Static catalogue of per-device capability limits.
 *
 * Java equivalent: DeviceService#loadLimits() loading from
 *   src/main/resources/devices/<deviceName>.properties via java.util.Properties.
 *
 * On ESP32 the properties files cannot be read at runtime. The equivalent is
 * a compile-time array of DeviceLimits structs stored in flash (PROGMEM) and
 * looked up by the device name string returned by DC2DCConverter::getDevice().
 *
 * Usage:
 *   const DeviceLimits* limits = findDeviceLimits("XY6008");
 *   if (limits != nullptr) { ... apply limits to ConverterState ... }
 */

/**
 * Searches the catalogue for the entry whose name matches deviceName exactly.
 *
 * Java equivalent: the classpath lookup
 *   DeviceService.class.getResourceAsStream("/devices/" + deviceName + ".properties")
 *
 * @param deviceName  device model string from DC2DCConverter::getDevice(),
 *                    e.g. "XY6008", "RD6006", "ZK-6522C"
 * @return pointer to the matching DeviceLimits entry, or nullptr if not found
 */
const DeviceLimits* findDeviceLimits(const char* deviceName);
