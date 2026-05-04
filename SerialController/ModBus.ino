#include <Arduino.h>
#include "esp_log.h"

/**
 * ModBus.ino - Protocol Implementation and Diagnostics
 */

static const char* TAG_MB = "MODBUS";

// Modbus Protocol Constants
#define MODBUS_FC_READ_REGS  0x03 
#define MODBUS_FC_WRITE_REG  0x06 
#define MODBUS_READ_REQ_LEN  8    
#define MODBUS_READ_RES_LEN  7    
#define MODBUS_WRITE_REQ_LEN 8    
#define MODBUS_WRITE_RES_LEN 8    
#define MODBUS_TIMEOUT_MS    1000 

/**
 * Helper to log raw hex bytes for diagnostics.
 */
void logHex(const char* prefix, uint8_t* buffer, uint8_t length) {
  // Classic Serial Log
  Serial.print(prefix);
  for (int i = 0; i < length; i++) {
    if (buffer[i] < 0x10) {
      Serial.print("0");
    }
    Serial.print(buffer[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  // ESP_LOG Version (as one line)
  char hex_str[length * 3 + 1];
  for (int i = 0; i < length; i++) {
    sprintf(hex_str + (i * 3), "%02X ", buffer[i]);
  }
  ESP_LOGI(TAG_MB, "%s%s", prefix, hex_str);
}

/**
 * Calculates the Modbus CRC-16 for a given buffer.
 */
uint16_t calculateCRC(uint8_t *buffer, uint8_t length) {
  uint16_t crc = 0xFFFF;
  for (int pos = 0; pos < length; pos++) {
    crc ^= (uint16_t)buffer[pos];
    for (int i = 8; i != 0; i--) {
      if ((crc & 0x0001) != 0) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

/**
 * Reads a single 16-bit register from a Modbus slave.
 */
bool readModbusRegister(uint8_t slaveId, uint16_t regAddress, uint16_t &value) {
  uint8_t request[MODBUS_READ_REQ_LEN];
  request[0] = slaveId;
  request[1] = MODBUS_FC_READ_REGS;
  request[2] = (regAddress >> 8);
  request[3] = (regAddress & 0xFF);
  request[4] = 0x00;
  request[5] = 0x01;
  
  uint16_t crc = calculateCRC(request, 6);
  request[6] = crc & 0xFF;
  request[7] = (crc >> 8);

  while (Serial2.available()) {
    Serial2.read();
  }
  Serial2.write(request, MODBUS_READ_REQ_LEN);
  logHex("TX: ", request, MODBUS_READ_REQ_LEN);

  uint8_t response[MODBUS_READ_RES_LEN];
  uint32_t startTime = millis();
  int bytesRead = 0;
  
  while (bytesRead < MODBUS_READ_RES_LEN && (millis() - startTime < MODBUS_TIMEOUT_MS)) {
    if (Serial2.available()) {
      response[bytesRead++] = Serial2.read();
    }
  }

  if (bytesRead > 0) {
    logHex("RX: ", response, bytesRead);
  }

  if (bytesRead == MODBUS_READ_RES_LEN) {
    uint16_t receivedCRC = response[5] | (response[6] << 8);
    if (calculateCRC(response, 5) == receivedCRC) {
      value = (response[3] << 8) | response[4];
      return true;
    } else {
      ESP_LOGE(TAG_MB, "CRC Mismatch on READ response.");
    }
  } else {
    ESP_LOGE(TAG_MB, "Incomplete READ response: received %d of %d bytes", bytesRead, MODBUS_READ_RES_LEN);
  }
  return false;
}

/**
 * Writes a single 16-bit register to a Modbus slave.
 */
bool writeModbusRegister(uint8_t slaveId, uint16_t regAddress, uint16_t value) {
  uint8_t request[MODBUS_WRITE_REQ_LEN];
  request[0] = slaveId;
  request[1] = MODBUS_FC_WRITE_REG;
  request[2] = (regAddress >> 8);
  request[3] = (regAddress & 0xFF);
  request[4] = (value >> 8);
  request[5] = (value & 0xFF);
  
  uint16_t crc = calculateCRC(request, 6);
  request[6] = crc & 0xFF;
  request[7] = (crc >> 8);

  while (Serial2.available()) {
    Serial2.read();
  }
  Serial2.write(request, MODBUS_WRITE_REQ_LEN);
  logHex("TX: ", request, MODBUS_WRITE_REQ_LEN);

  uint8_t response[MODBUS_WRITE_RES_LEN];
  uint32_t startTime = millis();
  int bytesRead = 0;
  
  while (bytesRead < MODBUS_WRITE_RES_LEN && (millis() - startTime < MODBUS_TIMEOUT_MS)) {
    if (Serial2.available()) {
      response[bytesRead++] = Serial2.read();
    }
  }

  if (bytesRead > 0) {
    logHex("RX: ", response, bytesRead);
  }

  if (bytesRead == MODBUS_WRITE_RES_LEN) {
    uint16_t receivedCRC = response[6] | (response[7] << 8);
    if (calculateCRC(response, 6) == receivedCRC) {
      return true;
    } else {
      ESP_LOGE(TAG_MB, "CRC Mismatch on WRITE response.");
    }
  } else {
    ESP_LOGE(TAG_MB, "Incomplete WRITE response: received %d of %d bytes", bytesRead, MODBUS_WRITE_RES_LEN);
  }
  return false;
}
