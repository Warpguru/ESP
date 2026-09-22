#pragma once

#include "base/DeviceRegister.h"

/**
 * SinilinkRegisters.h - Modbus register map for Sinilink XY-series.
 *
 * Java equivalent: com.serial.device.SinilinkRegisters
 */
class SinilinkRegisters {
 public:
  // TODO: Port register definitions from com.serial.device.SinilinkRegisters
  static const DeviceRegister VSET;
  static const DeviceRegister ISET;
  static const DeviceRegister VOUT;
  static const DeviceRegister IOUT;
};
