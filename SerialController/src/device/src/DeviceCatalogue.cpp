#include "DeviceCatalogue.h"

#include <string.h>

/**
 * DeviceCatalogue.cpp - Compile-time catalogue of all supported DC/DC converter limits.
 *
 * Java equivalent: the .properties files under
 *   src/main/resources/devices/<deviceName>.properties
 *
 * Each entry corresponds to exactly one .properties file.
 * Field order: name, manufacturer, maxVoltageVolts, minVoltageVolts,
 *              maxCurrentAmperes, minCurrentAmperes, maxPowerWatts, topology.
 *
 * The catalogue name strings MUST match DC2DCConverter::getDevice() exactly,
 * i.e. the strings set by each driver's KNOWN_MODELS / KNOWN_DEVICE_IDS table:
 *   Sinilink.cpp KNOWN_MODELS    → "XY5008", "XY6008", "XY6014", "XY6020L",
 *                                   "XYH3680", "XY3607F", "SK180S", "SK220S"
 *   Wuzhi.cpp KNOWN_MODELS       → "ZK-6522", "ZK-6522C", "ZK-10022C",
 *                                   "ZK-SK150C", "WZ3605E", "WZ5005E", "WZ-6008"
 *   RidenRD50xx KNOWN_DEVICE_IDS → "DPS5005", "DPS5010", "DPS5020"
 *   RidenRD60xx KNOWN_DEVICE_IDS → "RD6006", "RD6006P", "RK6006", "RD6012",
 *                                   "RD6012P", "RD6018", "RD6024", "RD6030"
 *
 * Additional entries included for completeness (properties files exist in the
 * Java reference but driver KNOWN tables are not yet updated for them):
 *   "RD5006", "RD5020", "RD6020", "ZK-SK150Pro"
 *
 * Note: XYH3680 - no official .properties file exists in the Java reference.
 * Included here with values sourced from the Sinilink XYH3680 product manual.
 * TODO: replace with confirmed datasheet values once available.
 *
 * The array is placed in read-only data (.rodata) by the linker automatically
 * because it is const. No PROGMEM annotation is needed on ESP32 (Xtensa LX6
 * reads flash via normal pointers without pgm_read_* helpers).
 */
static const DeviceLimits CATALOGUE[] = {

  // ---- Sinilink XY-series (BUCK) -------------------------------------------
  // Source: XY6020L Modbus Interface Documentation / XY5008 datasheet

  { "XY5008",  "Sinilink",  50.0, 0.0,  8.0, 0.0,  400.0, ConverterTopology::BUCK },
  { "XY6008",  "Sinilink",  60.0, 0.0,  8.0, 0.0,  480.0, ConverterTopology::BUCK },
  { "XY6014",  "Sinilink",  60.0, 0.0, 14.0, 0.0,  840.0, ConverterTopology::BUCK },
  { "XY6020L", "Sinilink",  60.0, 0.0, 20.0, 0.0, 1200.0, ConverterTopology::BUCK },

  // ---- Sinilink XY-series (BUCK_BOOST) -------------------------------------
  // Source: XYH3680 / XY3607F / SK180S / SK220S product manuals

  { "XY3607F", "Sinilink",  36.0, 0.0,  7.0, 0.0,  250.0, ConverterTopology::BUCK_BOOST },
  { "SK180S",  "Sinilink",  36.0, 0.0,  5.0, 0.0,  180.0, ConverterTopology::BUCK_BOOST },
  { "SK220S",  "Sinilink",  36.0, 0.0,  9.0, 0.0,  220.0, ConverterTopology::BUCK_BOOST },

  // XYH3680: no .properties file in the Java reference.
  // Values sourced from the Sinilink XYH3680 product manual.
  // TODO: replace with confirmed datasheet values once available.
  { "XYH3680", "Sinilink",  40.0, 0.0,  8.0, 0.0,  320.0, ConverterTopology::BUCK_BOOST },

  // ---- Wuzhi ZK-series (BUCK) ----------------------------------------------
  // Source: ZK-series product datasheets

  { "ZK-6522",    "Wuzhi",  65.0, 0.0, 22.0, 0.0, 1430.0, ConverterTopology::BUCK },
  { "ZK-6522C",   "Wuzhi",  65.0, 0.0, 22.0, 0.0, 1430.0, ConverterTopology::BUCK },
  { "ZK-10022C",  "Wuzhi", 125.0, 0.0, 22.0, 0.0, 1500.0, ConverterTopology::BUCK },
  { "WZ5005E",    "Wuzhi",  50.0, 0.0,  5.0, 0.0,  250.0, ConverterTopology::BUCK },
  { "WZ-6008",    "Wuzhi",  60.0, 0.0,  8.0, 0.0,  480.0, ConverterTopology::BUCK },

  // ---- Wuzhi ZK-series (BUCK_BOOST) ----------------------------------------
  // Source: ZK-SK150C / ZK-SK150Pro / WZ3605E product datasheets

  { "ZK-SK150C",  "Wuzhi",  40.0, 0.5,  8.0, 0.0,  150.0, ConverterTopology::BUCK_BOOST },
  { "ZK-SK150Pro","Wuzhi",  40.0, 0.5,  8.0, 0.0,  150.0, ConverterTopology::BUCK_BOOST },
  { "WZ3605E",    "Wuzhi",  36.0, 0.6,  5.0, 0.0,   80.0, ConverterTopology::BUCK_BOOST },

  // ---- Riden DPS50xx series ------------------------------------------------
  // Source: DPS5020 communication protocol V1.2 / DPS5005 / DPS5010 datasheets

  { "DPS5005", "Riden",  50.0, 0.0,  5.0, 0.0,  250.0, ConverterTopology::BUCK },
  { "DPS5010", "Riden",  50.0, 0.0, 10.0, 0.0,  500.0, ConverterTopology::BUCK },
  { "DPS5020", "Riden",  50.0, 0.0, 20.0, 0.0, 1000.0, ConverterTopology::BUCK },

  // ---- Riden RD50xx series -------------------------------------------------
  // Source: RD5006 / RD5020 datasheets

  { "RD5006",  "Riden",  50.0, 0.0,  6.0, 0.0,  300.0, ConverterTopology::BUCK },
  { "RD5020",  "Riden",  50.0, 0.0, 20.0, 0.0, 1000.0, ConverterTopology::BUCK },

  // ---- Riden RD60xx series -------------------------------------------------
  // Source: RD60xx datasheets

  { "RD6006",  "Riden",  60.0, 0.0,  6.0, 0.0,  360.0, ConverterTopology::BUCK },
  { "RD6006P", "Riden",  60.0, 0.0,  6.0, 0.0,  360.0, ConverterTopology::BUCK },
  { "RK6006",  "Riden",  60.0, 0.0,  6.0, 0.0,  360.0, ConverterTopology::BUCK },
  { "RD6012",  "Riden",  60.0, 0.0, 12.0, 0.0,  720.0, ConverterTopology::BUCK },
  { "RD6012P", "Riden",  60.0, 0.0, 12.0, 0.0,  720.0, ConverterTopology::BUCK },
  { "RD6018",  "Riden",  60.0, 0.0, 18.0, 0.0, 1080.0, ConverterTopology::BUCK },
  { "RD6020",  "Riden",  60.0, 0.0, 20.0, 0.0, 1200.0, ConverterTopology::BUCK },
  { "RD6024",  "Riden",  60.0, 0.0, 24.0, 0.0, 1440.0, ConverterTopology::BUCK },
  { "RD6030",  "Riden",  60.0, 0.0, 30.0, 0.0, 1800.0, ConverterTopology::BUCK },

  // Sentinel: marks the end of the array. findDeviceLimits() stops here.
  { nullptr, nullptr, 0.0, 0.0, 0.0, 0.0, 0.0, ConverterTopology::BUCK_BOOST }
};

// ---- findDeviceLimits -------------------------------------------------------

/**
 * Searches the compile-time catalogue for the entry whose name field matches
 * deviceName exactly (case-sensitive strcmp).
 *
 * Java equivalent: DeviceService#loadLimits() constructing the classpath path
 *   "/devices/" + deviceName + ".properties" and opening it via
 *   DeviceService.class.getResourceAsStream().
 *
 * @param deviceName  device model string from DC2DCConverter::getDevice(),
 *                    e.g. "XY6008", "RD6006", "ZK-6522C"
 * @return pointer to the matching DeviceLimits entry, or nullptr if not found
 */
const DeviceLimits* findDeviceLimits(const char* deviceName) {
  if (deviceName == nullptr) {
    return nullptr;
  }
  for (const DeviceLimits* entry = CATALOGUE; entry->name != nullptr; ++entry) {
    if (strcmp(entry->name, deviceName) == 0) {
      return entry;
    }
  }
  return nullptr;
}
