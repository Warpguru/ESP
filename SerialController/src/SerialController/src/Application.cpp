#include "Application.h"

#include <Arduino.h>

#include "../../modbus/src/ModbusConstants.h"
#include "../../modbus/src/ModbusTransport.h"
#include "../../service/src/DeviceDetection.h"
#include "../../service/src/DeviceService.h"
#include "../../service/src/WebSocketService.h"
#include "ActiveDevice.h"
#include "ConverterStateGlobal.h"
#include "LogBuffer.h"
#include "Server.h"
#include "StatusLed.h"

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

// Serial pins for the Riden device (UART2).
static constexpr int RIDEN_RX_PIN = 16;
static constexpr int RIDEN_TX_PIN = 17;
static constexpr int RIDEN_BAUD = ModbusConstants::BAUD_9600;
static constexpr uint8_t RIDEN_SLAVE = ModbusConstants::SLAVE_ADDRESS_1;

// ---- Global singletons -----------------------------------------------------

// ConverterState - shared across Server.cpp, DeviceService, RestService, WebSocketService.
// Java equivalent: DeviceService#state (private field; shared via getState()).
ConverterState converterState;

// Active device driver pointer - set in applicationSetup(), used by Server.cpp
// (legacy /setVoltage endpoint) and DeviceService.
// Java equivalent: DeviceService#converter (private field).
DC2DCConverter* activeDevice = nullptr;

// ---- applicationSetup ------------------------------------------------------

void applicationSetup() {
  // Turn the status LED on immediately (BOOTING state) before anything else.
  // The led task becomes the sole owner of GPIO 2 from this point forward.
  statusLed.begin();

  Serial.begin(115200);
  delay(1000);

  // Initialise the levelled logger. Must be called before any Log.* call.
  // No Java equivalent - ESP32-specific remote diagnostics facility.
  Log.begin();

  Log_info("Starting SerialController v1.0.0 (built " __DATE__ " " __TIME__ ")");
  Serial.println("\n--- SerialController v1.0.0 (built " __DATE__ " " __TIME__ ") ---");

  // Construct the Modbus transport, then auto-detect the connected device.
  // ModbusTransport constructor initialises Serial2 and starts the Modbus
  // task on Core 0. detectDevice() probes Sinilink → Wuzhi → RD50xx → RD60xx
  // at primary baud rates first, then secondary baud rates as fallback.
  // Java equivalent: DeviceService constructor → detectDevice()
  ModbusTransport* transport = new ModbusTransport(RIDEN_RX_PIN, RIDEN_TX_PIN, RIDEN_BAUD);
  activeDevice = detectDevice(transport, RIDEN_SLAVE);

  if (activeDevice == nullptr) {
    // No device found - warn and continue.
    // Java equivalent: DeviceService logs a warning and continues with
    // converter = null; poll() and write operations are no-ops until a
    // device connects. The HTTP server and WebSocket still start normally
    // so /api/state, /status, and /ws/data remain reachable for diagnostics.
    Log_warn("No supported device detected. Server will start without a device.");
  } else {
    Log_info("Device detected: %s %s",
             activeDevice->getManufacturer() ? activeDevice->getManufacturer() : "?",
             activeDevice->getDevice() ? activeDevice->getDevice() : "?");
  }

  // Initialize WiFi, construct DeviceService + RestService + WebSocketService,
  // register all routes, and start server.begin() + DeviceService.begin().
  // Java equivalent: SerialControllerApplication main() wiring block.
  setupServer();

  // Set final LED state based on whether a device was found.
  // statusLed.setState(FAULT) is called inside setupServer() on WiFi failure;
  // if we reach this point WiFi is up and the server is running.
  if (activeDevice != nullptr) {
    statusLed.setState(LedState::READY); // OFF - fully operational
  } else {
    statusLed.setState(
        LedState::NO_DEVICE); // slow blink - server up, no device
  }
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
  // Runs on Core 1 (the Arduino loop task) - same task that would call
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
          Log_warn("WS setVoltage %.3f failed (range or Modbus error)", cmd.value);
        } else {
          Log_info("WS setVoltage %.3f V OK", cmd.value);
        }
        break;

      case WsCommand::SET_CURRENT:
        // Java equivalent: deviceService.setCurrent(value)
        if (!deviceServicePtr->setCurrent(cmd.value)) {
          Log_warn("WS setCurrent %.3f failed (range or Modbus error)", cmd.value);
        } else {
          Log_info("WS setCurrent %.3f A OK", cmd.value);
        }
        break;

      case WsCommand::SET_OUTPUT:
        // Java equivalent: deviceService.setOutput(flag)
        if (!deviceServicePtr->setOutput(cmd.flag)) {
          Log_warn("WS setOutput %s failed (Modbus error)", cmd.flag ? "ON" : "OFF");
        } else {
          Log_info("WS setOutput %s OK", cmd.flag ? "ON" : "OFF");
        }
        break;

      case WsCommand::SET_KEYPAD:
        // Java equivalent: deviceService.setKeypad(flag)
        if (!deviceServicePtr->setKeypad(cmd.flag)) {
          Log_warn("WS setKeypad %s failed (Modbus error)", cmd.flag ? "LOCKED" : "UNLOCKED");
        } else {
          Log_info("WS setKeypad %s OK", cmd.flag ? "LOCKED" : "UNLOCKED");
        }
        break;

      default:
        break;
    }
  }
}
