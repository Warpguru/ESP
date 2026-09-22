#pragma once

#include "base/DeviceRegister.h"

/**
 * RidenRegistersRD60xx.h - Modbus register map for Riden RD60xx series.
 *
 * Java equivalent: com.serial.device.RidenRegistersRD60xx
 */
class RidenRegistersRD60xx {
 public:
  // TODO: Port register definitions from com.serial.device.RidenRegistersRD60xx
  static const DeviceRegister VSET;
  static const DeviceRegister ISET;
  static const DeviceRegister VOUT;
  static const DeviceRegister IOUT;
};
