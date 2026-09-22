#include "RidenRegistersRD60xx.h"

/**
 * RidenRegistersRD60xx.cpp - Register map for Riden RD60xx series.
 *
 * Java equivalent: com.serial.device.RidenRegistersRD60xx
 */

// TODO: Port register definitions from com.serial.device.RidenRegistersRD60xx
const DeviceRegister RidenRegistersRD60xx::VSET("Voltage Setpoint", "V", 0x0000, 100);
const DeviceRegister RidenRegistersRD60xx::ISET("Current Setpoint", "A", 0x0001, 1000);
const DeviceRegister RidenRegistersRD60xx::VOUT("Voltage Output", "V", 0x0002, 100);
const DeviceRegister RidenRegistersRD60xx::IOUT("Current Output", "A", 0x0003, 1000);
