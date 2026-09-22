#include "Application.h"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "esp_log.h"
#include "../../../RidenConfig.h"
#include "../../../ModBus.h"
#include "../../../Server.h"

/**
 * Application.cpp - Main application logic for SerialController.
 *
 * Contains the full setup and loop implementation. Both SerialController.ino
 * (Arduino IDE) and SerialController.cpp (PlatformIO) are thin stubs that
 * forward setup() / loop() to applicationSetup() / applicationLoop() here.
 */

static const char* TAG_MAIN = "MAIN";

void applicationSetup() {
  Serial.begin(115200);
  delay(1000);

  ESP_LOGI(TAG_MAIN, "Starting SerialController: Iteration 4 (WiFi & REST)");
  Serial.println("\n--- SerialController: Iteration 4 (WiFi & REST) ---");

  // Initialize UART2 for Riden, then start the dedicated Modbus task (ModBus.ino).
  // All Serial2 access is owned by that task — never call Serial2 directly.
  Serial2.begin(BAUDRATE, SERIAL_8N1, RX_PIN, TX_PIN);
  ESP_LOGI(TAG_MAIN, "Riden serial port (UART2) initialized at %d baud", BAUDRATE);
  Serial.println("Riden serial port (UART2) initialized.");
  setupModbus();

  // Initialize WiFi and WebServer (Server.ino)
  setupServer();
}

void applicationLoop() {
  static uint32_t lastRequest   = 0;
  static bool     toggleVoltage = false;

  // Handle HTTP API Requests (Server.ino)
  handleServerRequests();

  // Perform the Background Write -> Read sequence every 30 seconds
  if (millis() - lastRequest > 30000) {
    lastRequest = millis();

    uint16_t targetVoltage  = toggleVoltage ? 500 : 330;
    float    targetVoltageF = targetVoltage / 100.0;

    ESP_LOGI(TAG_MAIN, "[Background Task] Setting Voltage: %.2f V", targetVoltageF);
    Serial.printf("\n[Background Task] Setting Voltage: %.2f V\n", targetVoltageF);

    if (writeModbusRegister(RIDEN_ID, REG_V_SET, targetVoltage)) {
      ESP_LOGI(TAG_MAIN, "Write SUCCESS.");
      Serial.println("Write SUCCESS.");
    } else {
      ESP_LOGE(TAG_MAIN, "Write FAILED.");
      Serial.println("Write FAILED.");
    }

    delay(100);

    uint16_t vOutRaw;
    if (readModbusRegister(RIDEN_ID, REG_V_OUT, vOutRaw)) {
      float voltage = vOutRaw / 100.0;
      ESP_LOGI(TAG_MAIN, "Read SUCCESS: %.2f V", voltage);
      Serial.printf("Read SUCCESS: %.2f V\n", voltage);
    }

    toggleVoltage = !toggleVoltage;
  }
}
