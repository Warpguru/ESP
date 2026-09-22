#include "WuzhiRegisters.h"

/**
 * WuzhiRegisters.cpp - Register map for Wuzhi ZK-series.
 *
 * Java equivalent: com.serial.device.WuzhiRegisters
 */

// TODO: Port register definitions from com.serial.device.WuzhiRegisters
const DeviceRegister WuzhiRegisters::VSET("Voltage Setpoint", "V", 0x0000, 100);
const DeviceRegister WuzhiRegisters::ISET("Current Setpoint", "A", 0x0001, 1000);
const DeviceRegister WuzhiRegisters::VOUT("Voltage Output", "V", 0x0002, 100);
const DeviceRegister WuzhiRegisters::IOUT("Current Output", "A", 0x0003, 1000);
