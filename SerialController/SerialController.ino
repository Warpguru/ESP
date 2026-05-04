#include <Arduino.h>
#include "esp_log.h"

/**
 * SerialController.ino - Main Application Logic
 */

static const char* TAG_MAIN = "MAIN";

// Serial Pins & Configuration
#define RX_PIN 16
#define TX_PIN 17
#define BAUDRATE 9600
#define RIDEN_ID 1

// Riden Registers (Hex)
#define REG_V_SET 0x0002
#define REG_I_SET 0x0003
#define REG_V_OUT 0x0008
#define REG_I_OUT 0x0009
#define REG_OUTPUT 0x0012 

// Function Prototypes (ModBus.ino)
bool readModbusRegister(uint8_t slaveId, uint16_t regAddress, uint16_t &value);
bool writeModbusRegister(uint8_t slaveId, uint16_t regAddress, uint16_t value);

// Function Prototypes (Server.ino)
void setupServer();
void handleServerRequests();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  ESP_LOGI(TAG_MAIN, "Starting SerialController: Iteration 4 (WiFi & REST)");
  Serial.println("\n--- SerialController: Iteration 4 (WiFi & REST) ---");
  
  // Initialize UART2 for Riden
  Serial2.begin(BAUDRATE, SERIAL_8N1, RX_PIN, TX_PIN);
  ESP_LOGI(TAG_MAIN, "Riden serial port (UART2) initialized at %d baud", BAUDRATE);
  Serial.println("Riden serial port (UART2) initialized.");

  // Initialize WiFi and WebServer (Server.ino)
  setupServer();
}

void loop() {
  static uint32_t lastRequest = 0;
  static bool toggleVoltage = false;
  
  // Handle HTTP API Requests (Server.ino)
  handleServerRequests();

  // Perform the Background Write -> Read sequence every 30 seconds
  if (millis() - lastRequest > 30000) {
    lastRequest = millis();
    
    uint16_t targetVoltage = toggleVoltage ? 500 : 330;
    float targetVoltageF = targetVoltage / 100.0;
    
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
