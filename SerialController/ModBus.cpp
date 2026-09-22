#include "ModBus.h"

#include <Arduino.h>

#include "esp_log.h"

/**
 * ModBus.cpp - Protocol Implementation and Diagnostics
 *
 * Thread-safety design:
 *   All Modbus communication is handled exclusively by modbusTask(), which
 *   runs on Core 0. Callers (HTTP handlers, background loop) never touch
 *   Serial2 directly. Instead they call readModbusRegister() /
 *   writeModbusRegister(), which enqueue a request and block until the
 *   Modbus task returns a result. This ensures Serial2 has exactly one owner.
 */

static const char* TAG_MB = "MODBUS";

// Modbus Protocol Constants
#define MODBUS_FC_READ_REGS 0x03
#define MODBUS_FC_WRITE_REG 0x06
#define MODBUS_READ_REQ_LEN 8
#define MODBUS_READ_RES_LEN 7
#define MODBUS_WRITE_REQ_LEN 8
#define MODBUS_WRITE_RES_LEN 8
#define MODBUS_TIMEOUT_MS 1000

// Modbus frame is at most 8 bytes; buffer for "XX " x 8 + null = 25 bytes
#define MODBUS_MAX_FRAME 8
#define HEX_BUF_SIZE (MODBUS_MAX_FRAME * 3 + 1)

// Caller timeout: how long a caller waits for the Modbus task to respond
#define MODBUS_CALL_TIMEOUT_MS 1500

// ---- Queue message types ------------------------------------------------

typedef enum {
  MB_OP_READ,
  MB_OP_WRITE
} ModbusOp;

typedef struct {
  ModbusOp op;
  uint8_t slaveId;
  uint16_t regAddress;
  uint16_t writeValue;          // used for MB_OP_WRITE
  QueueHandle_t responseQueue;  // caller-owned single-slot queue
} ModbusRequest;

typedef struct {
  bool success;
  uint16_t value;  // populated on successful MB_OP_READ
} ModbusResponse;

// The single request queue consumed by modbusTask()
static QueueHandle_t modbusRequestQueue = NULL;

// ---- Internal helpers (only called from modbusTask) --------------------

/**
 * Log raw hex bytes for diagnostics.
 * Fixed-size buffer — no VLA, bounded snprintf writes.
 */
static void logHex(const char* prefix, uint8_t* buffer, uint8_t length) {
  Serial.print(prefix);
  for (int i = 0; i < length; i++) {
    if (buffer[i] < 0x10) {
      Serial.print("0");
    }
    Serial.print(buffer[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  if (length > MODBUS_MAX_FRAME) {
    length = MODBUS_MAX_FRAME;
  }
  char hex_str[HEX_BUF_SIZE];
  int pos = 0;
  for (int i = 0; i < length; i++) {
    pos += snprintf(hex_str + pos, HEX_BUF_SIZE - pos, "%02X ", buffer[i]);
  }
  ESP_LOGI(TAG_MB, "%s%s", prefix, hex_str);
}

/**
 * Calculates the Modbus CRC-16 for a given buffer.
 */
static uint16_t calculateCRC(uint8_t* buffer, uint8_t length) {
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
 * Executes a Modbus READ internally (called only from modbusTask).
 */
static bool mbDoRead(uint8_t slaveId, uint16_t regAddress, uint16_t& value) {
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
    } else {
      vTaskDelay(1);
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
 * Executes a Modbus WRITE internally (called only from modbusTask).
 */
static bool mbDoWrite(uint8_t slaveId, uint16_t regAddress, uint16_t value) {
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
    } else {
      vTaskDelay(1);
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

// ---- Modbus task (Core 0) ----------------------------------------------

/**
 * FreeRTOS task: sole owner of Serial2.
 * Dequeues requests, executes them, and returns results via per-request
 * response queues. Runs on Core 0 alongside the WiFi/WebServer stack.
 */
static void modbusTask(void* parameter) {
  ModbusRequest req;
  for (;;) {
    if (xQueueReceive(modbusRequestQueue, &req, portMAX_DELAY) == pdTRUE) {
      ModbusResponse resp;
      resp.value = 0;

      if (req.op == MB_OP_READ) {
        resp.success = mbDoRead(req.slaveId, req.regAddress, resp.value);
      } else {
        resp.success = mbDoWrite(req.slaveId, req.regAddress, req.writeValue);
      }

      xQueueSend(req.responseQueue, &resp, portMAX_DELAY);
    }
  }
}

/**
 * Creates the request queue and starts modbusTask on Core 0.
 * Must be called once from setup() before any Modbus calls.
 */
void setupModbus() {
  modbusRequestQueue = xQueueCreate(4, sizeof(ModbusRequest));
  xTaskCreatePinnedToCore(modbusTask, "Modbus_Task", 4096, NULL, 2, NULL, 0);
  ESP_LOGI(TAG_MB, "Modbus task started on Core 0.");
}

// ---- Public API (safe to call from any task) ---------------------------

/**
 * Reads a single 16-bit register from a Modbus slave.
 * Enqueues the request and blocks until modbusTask returns a result.
 */
bool readModbusRegister(uint8_t slaveId, uint16_t regAddress, uint16_t& value) {
  QueueHandle_t respQ = xQueueCreate(1, sizeof(ModbusResponse));

  ModbusRequest req;
  req.op = MB_OP_READ;
  req.slaveId = slaveId;
  req.regAddress = regAddress;
  req.writeValue = 0;
  req.responseQueue = respQ;

  xQueueSend(modbusRequestQueue, &req, portMAX_DELAY);

  ModbusResponse resp;
  bool timedOut = (xQueueReceive(respQ, &resp, pdMS_TO_TICKS(MODBUS_CALL_TIMEOUT_MS)) != pdTRUE);
  vQueueDelete(respQ);

  if (timedOut) {
    ESP_LOGE(TAG_MB, "readModbusRegister: caller timed out waiting for task.");
    return false;
  }
  if (resp.success) {
    value = resp.value;
  }
  return resp.success;
}

/**
 * Writes a single 16-bit register to a Modbus slave.
 * Enqueues the request and blocks until modbusTask returns a result.
 */
bool writeModbusRegister(uint8_t slaveId, uint16_t regAddress, uint16_t value) {
  QueueHandle_t respQ = xQueueCreate(1, sizeof(ModbusResponse));

  ModbusRequest req;
  req.op = MB_OP_WRITE;
  req.slaveId = slaveId;
  req.regAddress = regAddress;
  req.writeValue = value;
  req.responseQueue = respQ;

  xQueueSend(modbusRequestQueue, &req, portMAX_DELAY);

  ModbusResponse resp;
  bool timedOut = (xQueueReceive(respQ, &resp, pdMS_TO_TICKS(MODBUS_CALL_TIMEOUT_MS)) != pdTRUE);
  vQueueDelete(respQ);

  if (timedOut) {
    ESP_LOGE(TAG_MB, "writeModbusRegister: caller timed out waiting for task.");
    return false;
  }
  return resp.success;
}
