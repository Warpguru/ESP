#include "Application.h"

#include <Arduino.h>

#include "../../../ActiveDevice.h"
#include "../../../ConverterStateGlobal.h"
#include "../../../Server.h"
#include "../../../src/devices/src/RidenRD60xx.h"
#include "../../../src/modbus/src/ModbusConstants.h"
#include "../../../src/modbus/src/ModbusTransport.h"
#include "../../../src/service/src/WebSocketService.h"
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
static constexpr int RIDEN_RX_PIN = 16;
static constexpr int RIDEN_TX_PIN = 17;
static constexpr int RIDEN_BAUD = ModbusConstants::BAUD_9600;
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

  ESP_LOGI(TAG_MAIN, "Starting SerialController: Step 6 (WebSocket command handling)");
  Serial.println("\n--- SerialController: Step 6 (WebSocket command handling) ---");

  // Construct the Modbus transport and RidenRD60xx driver.
  // ModbusTransport constructor initialises Serial2 and starts the Modbus task on Core 0.
  ModbusTransport* transport = new ModbusTransport(RIDEN_RX_PIN, RIDEN_TX_PIN, RIDEN_BAUD);
  activeDevice = new RidenRD60xx(transport, RIDEN_SLAVE);

  // Initialize WiFi and the async HTTP + WebSocket server.
  setupServer();
}

// Global WebSocketService reference — defined in Server.cpp, declared extern
// so Application.cpp can drain its command queue in the loop.
// Java equivalent: WebSocketService is injected into DeviceService constructor.
extern WebSocketService wsService;

void applicationLoop() {
  static uint32_t lastPoll = 0;

  // ESPAsyncWebServer is fully non-blocking; this call is a no-op but kept for
  // API compatibility with the Server.h declaration.
  handleServerRequests();

  // Drain WebSocket command queue — execute any command the browser sent.
  // Runs on Core 1 (this loop task) which owns the Modbus transport, so
  // activeDevice calls are safe here without stalling the network stack.
  // Java equivalent: WebSocketService#onMessage dispatches directly on its thread;
  // here we dispatch on the loop task via the queue instead.
  WsCommand cmd;
  if (wsService.dequeueCommand(cmd)) {
    if (activeDevice == nullptr) {
      ESP_LOGW(TAG_MAIN, "WS command ignored — activeDevice not ready.");
    } else {
      switch (cmd.type) {
        case WsCommand::SET_VOLTAGE: {
          // Java equivalent: deviceService.setVoltage(value)
          // Range validation: reject if outside [0, maxVoltage].
          // Java equivalent: IllegalArgumentException from DeviceService.setVoltage
          double maxV = converterState.getMaxVoltage();
          if (maxV > 0.0 && (cmd.value < 0.0 || cmd.value > maxV)) {
            ESP_LOGW(TAG_MAIN, "setVoltage %.3f rejected: out of range [0, %.3f]", cmd.value, maxV);
          } else if (activeDevice->setVoltage(cmd.value)) {
            converterState.setVoltageSet(cmd.value);
            ESP_LOGI(TAG_MAIN, "setVoltage %.3f V OK", cmd.value);
          } else {
            ESP_LOGW(TAG_MAIN, "setVoltage %.3f V failed (Modbus error)", cmd.value);
          }
          break;
        }
        case WsCommand::SET_CURRENT: {
          // Java equivalent: deviceService.setCurrent(value)
          double maxI = converterState.getMaxCurrent();
          if (maxI > 0.0 && (cmd.value < 0.0 || cmd.value > maxI)) {
            ESP_LOGW(TAG_MAIN, "setCurrent %.3f rejected: out of range [0, %.3f]", cmd.value, maxI);
          } else if (activeDevice->setCurrent(cmd.value)) {
            converterState.setCurrentSet(cmd.value);
            ESP_LOGI(TAG_MAIN, "setCurrent %.3f A OK", cmd.value);
          } else {
            ESP_LOGW(TAG_MAIN, "setCurrent %.3f A failed (Modbus error)", cmd.value);
          }
          break;
        }
        case WsCommand::SET_OUTPUT:
          // Java equivalent: deviceService.setOutput(flag)
          if (activeDevice->setOutput(cmd.flag)) {
            converterState.setOutputEnabled(cmd.flag);
            ESP_LOGI(TAG_MAIN, "setOutput %s OK", cmd.flag ? "ON" : "OFF");
          } else {
            ESP_LOGW(TAG_MAIN, "setOutput %s failed (Modbus error)", cmd.flag ? "ON" : "OFF");
          }
          break;
        case WsCommand::SET_KEYPAD:
          // Java equivalent: deviceService.setKeypad(flag)
          if (activeDevice->setKeypad(cmd.flag)) {
            converterState.setKeypadLocked(cmd.flag);
            ESP_LOGI(TAG_MAIN, "setKeypad %s OK", cmd.flag ? "LOCKED" : "UNLOCKED");
          } else {
            ESP_LOGW(TAG_MAIN, "setKeypad %s failed (Modbus error)", cmd.flag ? "LOCKED" : "UNLOCKED");
          }
          break;
        default:
          break;
      }
    }
  }

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
