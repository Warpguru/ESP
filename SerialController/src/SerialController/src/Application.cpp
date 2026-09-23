#include "Application.h"

#include <Arduino.h>

#include "../../devices/src/RidenRD60xx.h"
#include "../../modbus/src/ModbusConstants.h"
#include "../../modbus/src/ModbusTransport.h"
#include "../../service/src/DeviceService.h"
#include "../../service/src/WebSocketService.h"
#include "ActiveDevice.h"
#include "ConverterStateGlobal.h"
#include "Server.h"
#include "esp_log.h"

/**
 * Application.cpp - Main application setup and loop.
 *
 * Constructs the Modbus transport, device driver, and global singletons.
 * The polling loop has moved into DeviceService#pollingTask.
 * HTTP handler logic lives in RestService.
 * WebSocket broadcast + receive live in WebSocketService.
 *
 * Java equivalent: com.serial.SerialControllerApplication#main
 */

static const char* TAG_MAIN = "MAIN";

// Serial pins for the Riden device (UART2).
static constexpr int RIDEN_RX_PIN = 16;
static constexpr int RIDEN_TX_PIN = 17;
static constexpr int RIDEN_BAUD = ModbusConstants::BAUD_9600;
static constexpr uint8_t RIDEN_SLAVE = ModbusConstants::SLAVE_ADDRESS_1;

// ---- Global singletons -----------------------------------------------------

// ConverterState — shared across Server.cpp, DeviceService, RestService, WebSocketService.
// Java equivalent: DeviceService#state (private field; shared via getState()).
ConverterState converterState;

// Active device driver pointer — set in applicationSetup(), used by Server.cpp
// (legacy /setVoltage endpoint) and DeviceService.
// Java equivalent: DeviceService#converter (private field).
DC2DCConverter* activeDevice = nullptr;

// ---- applicationSetup ------------------------------------------------------

void applicationSetup() {
  Serial.begin(115200);
  delay(1000);

  ESP_LOGI(TAG_MAIN, "Starting SerialController: Step 7 (REST API + DeviceService refactor)");
  Serial.println("\n--- SerialController: Step 7 ---");

  // Construct the Modbus transport and RidenRD60xx driver.
  // ModbusTransport constructor initialises Serial2 and starts the Modbus
  // task on Core 0.
  // Java equivalent: DeviceService constructor → detectDevice() → RidenRD60xx
  ModbusTransport* transport = new ModbusTransport(RIDEN_RX_PIN, RIDEN_TX_PIN, RIDEN_BAUD);
  activeDevice = new RidenRD60xx(transport, RIDEN_SLAVE);

  // Initialize WiFi, construct DeviceService + RestService + WebSocketService,
  // register all routes, and start server.begin() + DeviceService.begin().
  // Java equivalent: SerialControllerApplication main() wiring block.
  setupServer();
}

// ---- applicationLoop -------------------------------------------------------

// DeviceService is defined in Server.cpp (constructed there after WiFi is up).
// We need access to wsService (also in Server.cpp) for WS command dispatch.
// DeviceService is constructed in Server.cpp after WiFi/activeDevice are ready.
// Access it through the pointer exported from Server.cpp.
// Java equivalent: DeviceService instance created in SerialControllerApplication.
extern DeviceService* deviceServicePtr;
extern WebSocketService wsService;

void applicationLoop() {
  // ESPAsyncWebServer is fully non-blocking; this is a no-op kept for symmetry.
  handleServerRequests();

  // Drain WebSocket command queue and dispatch via DeviceService.
  // Runs on Core 1 (the Arduino loop task) — same task that would call
  // DeviceService write methods from REST handlers on this same core.
  //
  // Java equivalent: WebSocketService#onMessage calls DeviceService methods
  // directly on the WS thread. Here we use a queue to avoid blocking the
  // lwIP task that delivers WS_EVT_DATA.
  if (deviceServicePtr == nullptr) {
    return;
  }

  WsCommand cmd;
  if (wsService.dequeueCommand(cmd)) {
    switch (cmd.type) {
      case WsCommand::SET_VOLTAGE:
        // Java equivalent: deviceService.setVoltage(value)
        if (!deviceServicePtr->setVoltage(cmd.value)) {
          ESP_LOGW(TAG_MAIN, "WS setVoltage %.3f failed (range or Modbus error)", cmd.value);
        } else {
          ESP_LOGI(TAG_MAIN, "WS setVoltage %.3f V OK", cmd.value);
        }
        break;

      case WsCommand::SET_CURRENT:
        // Java equivalent: deviceService.setCurrent(value)
        if (!deviceServicePtr->setCurrent(cmd.value)) {
          ESP_LOGW(TAG_MAIN, "WS setCurrent %.3f failed (range or Modbus error)", cmd.value);
        } else {
          ESP_LOGI(TAG_MAIN, "WS setCurrent %.3f A OK", cmd.value);
        }
        break;

      case WsCommand::SET_OUTPUT:
        // Java equivalent: deviceService.setOutput(flag)
        if (!deviceServicePtr->setOutput(cmd.flag)) {
          ESP_LOGW(TAG_MAIN, "WS setOutput %s failed (Modbus error)", cmd.flag ? "ON" : "OFF");
        } else {
          ESP_LOGI(TAG_MAIN, "WS setOutput %s OK", cmd.flag ? "ON" : "OFF");
        }
        break;

      case WsCommand::SET_KEYPAD:
        // Java equivalent: deviceService.setKeypad(flag)
        if (!deviceServicePtr->setKeypad(cmd.flag)) {
          ESP_LOGW(TAG_MAIN, "WS setKeypad %s failed (Modbus error)", cmd.flag ? "LOCKED" : "UNLOCKED");
        } else {
          ESP_LOGI(TAG_MAIN, "WS setKeypad %s OK", cmd.flag ? "LOCKED" : "UNLOCKED");
        }
        break;

      default:
        break;
    }
  }
}
