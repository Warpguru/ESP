#include "Application.h"

#include <Arduino.h>

#include "../../../ActiveDevice.h"
#include "../../../ConverterStateGlobal.h"
#include "../../../Server.h"
#include "../../../src/devices/src/RidenRD60xx.h"
#include "../../../src/modbus/src/ModbusConstants.h"
#include "../../../src/modbus/src/ModbusTransport.h"
#include "esp_log.h"

/**
 * Application.cpp - Main application logic for SerialController.
 *
 * Contains the full setup and loop implementation. Both SerialController.ino
 * (Arduino IDE) and SerialController.cpp (PlatformIO) are thin stubs that
 * forward setup() / loop() to applicationSetup() / applicationLoop() here.
 */

static const char* TAG_MAIN = "MAIN";

// Serial pins for the Riden device (UART2).
// These will move into the ModbusTransport constructor call in Step 8 when
// RidenConfig.h is fully removed; kept here as named constants for clarity.
static constexpr int RIDEN_RX_PIN  = 16;
static constexpr int RIDEN_TX_PIN  = 17;
static constexpr int RIDEN_BAUD    = ModbusConstants::BAUD_9600;
static constexpr uint8_t RIDEN_SLAVE = ModbusConstants::SLAVE_ADDRESS_1;

// Single global ConverterState instance — shared with Server.cpp via
// ConverterStateGlobal.h (extern declaration).
ConverterState converterState;

// Single global DC2DCConverter pointer — shared with Server.cpp via
// ActiveDevice.h (extern declaration). Assigned in applicationSetup().
DC2DCConverter* activeDevice = nullptr;

// ---- Online/offline hysteresis counters ------------------------------------
// Match Java DeviceService: online immediately on first success;
// offline immediately on timeout; offline after 3 consecutive non-timeout errors.
static int failCount = 0;
static bool wasOnline = false;
static constexpr int FAIL_THRESHOLD = 3;

/**
 * Updates the online/offline hysteresis state after a poll result.
 *
 * Java equivalent: com.serial.service.DeviceService online/offline logic.
 */
static void applyPollResult(bool ok) {
  if (ok) {
    failCount = 0;
    if (!wasOnline) {
      converterState.setDeviceOnline(true);
      wasOnline = true;
      ESP_LOGI(TAG_MAIN, "Device came online");
    }
  } else {
    failCount++;
    if (!wasOnline) {
      // already offline — nothing to change
    } else if (failCount == 1) {
      // Timeout (first failure) → offline immediately
      converterState.setDeviceOnline(false);
      wasOnline = false;
      ESP_LOGW(TAG_MAIN, "Device offline (timeout / first failure)");
    } else if (failCount >= FAIL_THRESHOLD) {
      converterState.setDeviceOnline(false);
      wasOnline = false;
      ESP_LOGW(TAG_MAIN, "Device offline after %d consecutive failures", failCount);
    }
  }
}

void applicationSetup() {
  Serial.begin(115200);
  delay(1000);

  ESP_LOGI(TAG_MAIN, "Starting SerialController: Step 4 (C++ device class hierarchy)");
  Serial.println("\n--- SerialController: Step 4 (C++ device class hierarchy) ---");

  // Construct the Modbus transport and RidenRD60xx driver.
  // ModbusTransport constructor initialises Serial2 and starts the Modbus task on Core 0.
  ModbusTransport* transport = new ModbusTransport(RIDEN_RX_PIN, RIDEN_TX_PIN, RIDEN_BAUD);
  activeDevice = new RidenRD60xx(transport, RIDEN_SLAVE);

  // Initialize WiFi and the async HTTP + WebSocket server.
  setupServer();
}

void applicationLoop() {
  static uint32_t lastPoll = 0;

  // ESPAsyncWebServer is fully non-blocking; this call is a no-op but kept for
  // API compatibility with the Server.h declaration.
  handleServerRequests();

  // Poll all Riden registers every 1 second via a single bulk 0x03 frame,
  // then copy the cache into ConverterState for HTTP/WS handlers to read.
  if (millis() - lastPoll >= 1000) {
    lastPoll = millis();

    bool ok = activeDevice->pollAll();
    applyPollResult(ok);

    if (ok) {
      converterState.setVoltageOut(activeDevice->getVoltage());
      converterState.setCurrentOut(activeDevice->getCurrent());
      converterState.setPowerOut(activeDevice->getPower());
      converterState.setVoltageIn(activeDevice->getInputVoltage());
      converterState.setTemperatureCelsius(activeDevice->getTemperatureCelsius());
      converterState.setVoltageSet(activeDevice->getVoltageSet());
      converterState.setCurrentSet(activeDevice->getCurrentSet());
      converterState.setOutputEnabled(activeDevice->getOutput());
      converterState.setKeypadLocked(activeDevice->getKeypad());
      converterState.setProtectionState(activeDevice->getProtectionState() ? 1 : 0);
      converterState.setCvMode(activeDevice->isCvMode());

      // Set device identity on first successful poll.
      if (activeDevice->getDevice() != nullptr) {
        converterState.setDeviceName(activeDevice->getDevice());
      }
      if (activeDevice->getManufacturer() != nullptr) {
        converterState.setManufacturer(activeDevice->getManufacturer());
      }
    }
  }
}
