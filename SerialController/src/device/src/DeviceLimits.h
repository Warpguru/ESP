#pragma once

#include "../../service/src/ConverterTopology.h"

/**
 * DeviceLimits.h - Capability limits descriptor for a single DC/DC converter model.
 *
 * Java equivalent: the seven key-value pairs in each
 *   src/main/resources/devices/<modelName>.properties file.
 *
 * In Java the limits are loaded at runtime from the classpath via
 * java.util.Properties. On ESP32 the limits are compiled directly into
 * flash as a static array in DeviceCatalogue.cpp (no filesystem required).
 *
 * Field names map 1:1 to the Java properties keys:
 *   device.name         → name
 *   device.manufacturer → manufacturer
 *   device.maxVoltage   → maxVoltageVolts
 *   device.minVoltage   → minVoltageVolts
 *   device.maxCurrent   → maxCurrentAmperes
 *   device.minCurrent   → minCurrentAmperes
 *   device.maxPower     → maxPowerWatts
 *   device.topology     → topology
 */
struct DeviceLimits {
  /** Device model name string, e.g. "XY6008". Matches DC2DCConverter::getDevice(). */
  const char* name;
  /** Manufacturer string, e.g. "Sinilink". Matches DC2DCConverter::getManufacturer(). */
  const char* manufacturer;
  /** Maximum output voltage in volts (V). Java: device.maxVoltage */
  double maxVoltageVolts;
  /** Minimum output voltage in volts (V). Java: device.minVoltage */
  double minVoltageVolts;
  /** Maximum output current in amperes (A). Java: device.maxCurrent */
  double maxCurrentAmperes;
  /** Minimum output current in amperes (A). Java: device.minCurrent */
  double minCurrentAmperes;
  /** Maximum output power in watts (W). Java: device.maxPower */
  double maxPowerWatts;
  /** Converter topology. Java: device.topology */
  ConverterTopology topology;
};
