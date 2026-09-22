#include "SinilinkRegisters.h"

/**
 * SinilinkRegisters.cpp - Register map for Sinilink XY-series.
 *
 * Java equivalent: com.serial.device.SinilinkRegisters
 */

// TODO: Port register definitions from com.serial.device.SinilinkRegisters
const DeviceRegister SinilinkRegisters::VSET("Voltage Setpoint", "V", 0x0000, 100);
const DeviceRegister SinilinkRegisters::ISET("Current Setpoint", "A", 0x0001, 1000);
const DeviceRegister SinilinkRegisters::VOUT("Voltage Output", "V", 0x0002, 100);
const DeviceRegister SinilinkRegisters::IOUT("Current Output", "A", 0x0003, 1000);
