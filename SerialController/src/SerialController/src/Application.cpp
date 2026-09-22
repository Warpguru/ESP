#include "Application.h"

#include <Arduino.h>

#include "../../../ConverterStateGlobal.h"
#include "../../../ModBus.h"
#include "../../../RidenConfig.h"
#include "../../../Server.h"
#include "esp_log.h"

/**
 * Application.cpp - Main application logic for SerialController.
 *
 * Contains the full setup and loop implementation. Both SerialController.ino
 * (Arduino IDE) and SerialController.cpp (PlatformIO) are thin stubs that
 * forward setup() / loop() to applicationSetup() / applicationLoop() here.
 */

static const char* TAG_MAIN = "MAIN";

// Single global ConverterState instance — shared with Server.cpp via
// ConverterStateGlobal.h (extern declaration).
ConverterState converterState;

// ---- Online/offline hysteresis counters ------------------------------------
// Match Java DeviceService: online immediately on first success;
// offline immediately on timeout; offline after 3 consecutive non-timeout errors.
static int failCount = 0;
static bool wasOnline = false;
static constexpr int FAIL_THRESHOLD = 3;

/**
 * Read one 16-bit register; on success update state and reset failure counter.
 * On failure increment the counter and apply hysteresis rules.
 * Returns true on success.
 */
static bool pollRegister(uint8_t slaveId, uint16_t reg, uint16_t& out) {
  bool ok = readModbusRegister(slaveId, reg, out);
  if (ok) {
    failCount = 0;
    if (!wasOnline) {
      converterState.setDeviceOnline(true);
      wasOnline = true;
      ESP_LOGI(TAG_MAIN, "Device came online");
    }
  } else {
    failCount++;
    // Timeout (first failure) → offline immediately; other errors need 3 strikes
    if (!wasOnline) {
      // already offline — nothing to change
    } else if (failCount == 1) {
      converterState.setDeviceOnline(false);
      wasOnline = false;
      ESP_LOGW(TAG_MAIN, "Device offline (timeout / first failure)");
    } else if (failCount >= FAIL_THRESHOLD) {
      converterState.setDeviceOnline(false);
      wasOnline = false;
      ESP_LOGW(TAG_MAIN, "Device offline after %d consecutive failures", failCount);
    }
  }
  return ok;
}

void applicationSetup() {
  Serial.begin(115200);
  delay(1000);

  ESP_LOGI(TAG_MAIN, "Starting SerialController: Step 3 (ConverterState wiring)");
  Serial.println("\n--- SerialController: Step 3 (ConverterState wiring) ---");

  // Initialize UART2 for Riden, then start the dedicated Modbus task.
  // All Serial2 access is owned by that task — never call Serial2 directly.
  Serial2.begin(BAUDRATE, SERIAL_8N1, RX_PIN, TX_PIN);
  ESP_LOGI(TAG_MAIN, "Riden serial port (UART2) initialized at %d baud", BAUDRATE);
  setupModbus();

  // Initialize WiFi and the async HTTP + WebSocket server.
  setupServer();
}

void applicationLoop() {
  static uint32_t lastPoll = 0;

  // ESPAsyncWebServer is fully non-blocking; this call is a no-op but kept for
  // API compatibility with the Server.h declaration.
  handleServerRequests();

  // Poll Riden registers every 1 second and update ConverterState.
  if (millis() - lastPoll >= 1000) {
    lastPoll = millis();

    uint16_t raw;

    // Measured voltage out (REG_V_OUT = 0x0008, unit: 10 mV → divide by 100)
    if (pollRegister(RIDEN_ID, REG_V_OUT, raw)) {
      converterState.setVoltageOut(raw / 100.0);
    }

    // Measured current out (REG_I_OUT = 0x0009, unit: 1 mA → divide by 1000)
    if (pollRegister(RIDEN_ID, REG_I_OUT, raw)) {
      converterState.setCurrentOut(raw / 1000.0);
    }

    // Voltage setpoint — skip if a recent applyVoltageSetpoint() is still settling
    if (!converterState.isVoltagePending()) {
      if (pollRegister(RIDEN_ID, REG_V_SET, raw)) {
        converterState.setVoltageSet(raw / 100.0);
      }
    }

    // Current setpoint — skip if a recent applyCurrentSetpoint() is still settling
    if (!converterState.isCurrentPending()) {
      if (pollRegister(RIDEN_ID, REG_I_SET, raw)) {
        converterState.setCurrentSet(raw / 1000.0);
      }
    }
  }
}
