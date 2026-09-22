#pragma once

#include "base/DeviceRegister.h"

/**
 * RidenRegistersRD50xx.h - Modbus register map for Riden RD50xx series.
 *
 * Java equivalent: com.serial.device.RidenRegistersRD50xx
 */
class RidenRegistersRD50xx {
 public:
  // TODO: Port register definitions from com.serial.device.RidenRegistersRD50xx
  static const DeviceRegister VSET;
  static const DeviceRegister ISET;
  static const DeviceRegister VOUT;
  static const DeviceRegister IOUT;
};
