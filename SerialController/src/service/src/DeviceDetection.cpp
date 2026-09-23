#include "DeviceDetection.h"

#include <Arduino.h>
#include <esp_task_wdt.h>

#include "../../devices/src/RidenRD50xx.h"
#include "../../devices/src/RidenRD60xx.h"
#include "../../devices/src/Sinilink.h"
#include "../../devices/src/Wuzhi.h"
#include "../../modbus/src/ModbusConstants.h"
#include "esp_log.h"

/**
 * DeviceDetection.cpp - Auto-detects the connected DC/DC converter.
 *
 * Java equivalent: DeviceService#detectDevice + DeviceService#probeDrivers
 *
 * The Java implementation constructs a fresh ModbusTransport(portName, baud)
 * for each baud rate inside verifyDevicePresent(). The C++ equivalent instead
 * reconfigures the existing Serial2 port via ModbusTransport::setBaud() to
 * avoid spawning a new FreeRTOS task on every probe iteration.
 */

static const char* TAG_DETECT = "DETECT";

// ---- Primary baud rates (Java: ModbusTransport.PRIMARY_BAUDS) ---------------
// 115200 and 9600 — detect >99% of devices in <2 s.
static const int PRIMARY_BAUDS[] = {
    ModbusConstants::BAUD_115200,
    ModbusConstants::BAUD_9600};
static const int PRIMARY_BAUDS_COUNT = 2;

// ---- Secondary / fallback baud rates (Java: ModbusTransport.SECONDARY_BAUDS)
static const int SECONDARY_BAUDS[] = {
    ModbusConstants::BAUD_19200,
    ModbusConstants::BAUD_38400,
    ModbusConstants::BAUD_57600};
static const int SECONDARY_BAUDS_COUNT = 3;

// ---- probeDrivers -----------------------------------------------------------

/**
 * Probes all driver types at the given baud rates.
 * Matches Java: DeviceService#probeDrivers(portName, bauds)
 *
 * Order: Sinilink → Wuzhi → RidenRD50xx → RidenRD60xx
 *
 * For each baud rate the transport is reconfigured via setBaud(), then
 * verifyDevicePresent() is called on each driver in order. The first
 * driver that recognises the device is returned; the caller owns it.
 * On a match the transport is left open at the detected baud rate.
 *
 * @param transport  shared ModbusTransport (Serial2)
 * @param slave      Modbus slave address
 * @param bauds      array of baud rates to try
 * @param baudCount  number of entries in bauds
 * @return detected driver, or nullptr if nothing responded
 */
static DC2DCConverter* probeDrivers(
    ModbusTransport* transport,
    uint8_t slave,
    const int* bauds,
    int baudCount) {
  for (int i = 0; i < baudCount; i++) {
    int baud = bauds[i];
    ESP_LOGI(TAG_DETECT, "Probing at %d baud...", baud);

    // Each probe attempt blocks for up to READ_TIMEOUT_MS per driver (serial
    // read timeout in modbusTask). Resetting the Task Watchdog here prevents
    // the TWDT from firing if the full scan takes longer than its threshold.
    esp_task_wdt_reset();

    transport->setBaud(baud);

    // Try Sinilink
    // Java: Sinilink sinilink = new Sinilink(portName, slave); sinilink.verifyDevicePresent(bauds)
    {
      Sinilink* sinilink = new Sinilink(transport, slave);
      if (sinilink->verifyDevicePresent()) {
        ESP_LOGI(TAG_DETECT, "Detected: %s %s at %d baud",
                 sinilink->getManufacturer(), sinilink->getDevice(), baud);
        return sinilink;
      }
      delete sinilink;
    }

    // Try Wuzhi
    // Java: Wuzhi wuzhi = new Wuzhi(portName, slave); wuzhi.verifyDevicePresent(bauds)
    {
      Wuzhi* wuzhi = new Wuzhi(transport, slave);
      if (wuzhi->verifyDevicePresent()) {
        ESP_LOGI(TAG_DETECT, "Detected: %s %s at %d baud",
                 wuzhi->getManufacturer(), wuzhi->getDevice(), baud);
        return wuzhi;
      }
      delete wuzhi;
    }

    // Try Riden RD50xx
    // Java: RidenRD50xx ridenRD50xx = new RidenRD50xx(portName, slave); ridenRD50xx.verifyDevicePresent(bauds)
    {
      RidenRD50xx* rd50xx = new RidenRD50xx(transport, slave);
      if (rd50xx->verifyDevicePresent()) {
        ESP_LOGI(TAG_DETECT, "Detected: %s %s at %d baud",
                 rd50xx->getManufacturer(), rd50xx->getDevice(), baud);
        return rd50xx;
      }
      delete rd50xx;
    }

    // Try Riden RD60xx
    // Java: RidenRD60xx ridenRD60xx = new RidenRD60xx(portName, slave); ridenRD60xx.verifyDevicePresent(bauds)
    {
      RidenRD60xx* rd60xx = new RidenRD60xx(transport, slave);
      if (rd60xx->verifyDevicePresent()) {
        ESP_LOGI(TAG_DETECT, "Detected: %s %s at %d baud",
                 rd60xx->getManufacturer(), rd60xx->getDevice(), baud);
        return rd60xx;
      }
      delete rd60xx;
    }
  }

  return nullptr;
}

// ---- detectDevice -----------------------------------------------------------

/**
 * Two-pass detection: primary baud rates first, secondary fallback if needed.
 *
 * Java equivalent: DeviceService#detectDevice(portName)
 */
DC2DCConverter* detectDevice(ModbusTransport* transport, uint8_t slave) {
  ESP_LOGI(TAG_DETECT, "Starting device detection.");

  // Pass 1: primary baud rates (115200, 9600).
  // Java: probeDrivers(portName, ModbusTransport.PRIMARY_BAUDS)
  ESP_LOGI(TAG_DETECT, "Probing primary baud rates (115200, 9600)...");
  DC2DCConverter* found = probeDrivers(transport, slave, PRIMARY_BAUDS, PRIMARY_BAUDS_COUNT);
  if (found != nullptr) {
    return found;
  }

  // Pass 2: secondary / fallback baud rates (19200, 38400, 57600).
  // Java: probeDrivers(portName, ModbusTransport.SECONDARY_BAUDS)
  ESP_LOGI(TAG_DETECT, "No device found in primary pass. Probing fallback baud rates (19200, 38400, 57600)...");
  found = probeDrivers(transport, slave, SECONDARY_BAUDS, SECONDARY_BAUDS_COUNT);
  if (found != nullptr) {
    return found;
  }

  ESP_LOGW(TAG_DETECT, "No supported device detected.");
  return nullptr;
}
