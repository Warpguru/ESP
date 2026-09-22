#pragma once

#include "base/DeviceRegister.h"

/**
 * WuzhiRegisters.h - Modbus register map for Wuzhi ZK-series.
 *
 * Java equivalent: com.serial.device.WuzhiRegisters
 */
class WuzhiRegisters {
 public:
  // TODO: Port register definitions from com.serial.device.WuzhiRegisters
  static const DeviceRegister VSET;
  static const DeviceRegister ISET;
  static const DeviceRegister VOUT;
  static const DeviceRegister IOUT;
};
