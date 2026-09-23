#pragma once

#include "../../devices/src/ifc/DC2DCConverter.h"
#include "../../modbus/src/ModbusTransport.h"

/**
 * DeviceDetection.h - Auto-detection of a supported DC/DC converter.
 *
 * Java equivalent: DeviceService#detectDevice / DeviceService#probeDrivers
 *
 * Detection order (matches Java probeDrivers):
 *   Sinilink → Wuzhi → RidenRD50xx → RidenRD60xx
 *
 * Two-pass strategy (matches Java PRIMARY_BAUDS / SECONDARY_BAUDS):
 *   Pass 1 - primary baud rates:   115200, 9600
 *   Pass 2 - secondary baud rates: 19200, 38400, 57600
 *
 * @param transport pre-constructed ModbusTransport (Serial2 already open)
 * @param slave     Modbus slave address to probe
 * @return pointer to detected driver, or nullptr if no device found
 */
DC2DCConverter* detectDevice(ModbusTransport* transport, uint8_t slave);
