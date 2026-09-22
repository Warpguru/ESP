#include "RidenRegistersRD50xx.h"

/**
 * RidenRegistersRD50xx.cpp - Register map for Riden RD50xx series.
 *
 * Java equivalent: com.serial.device.RidenRegistersRD50xx
 */

// TODO: Port register definitions from com.serial.device.RidenRegistersRD50xx
const DeviceRegister RidenRegistersRD50xx::VSET("Voltage Setpoint", "V", 0x0000, 100);
const DeviceRegister RidenRegistersRD50xx::ISET("Current Setpoint", "A", 0x0001, 1000);
const DeviceRegister RidenRegistersRD50xx::VOUT("Voltage Output", "V", 0x0002, 100);
const DeviceRegister RidenRegistersRD50xx::IOUT("Current Output", "A", 0x0003, 1000);
