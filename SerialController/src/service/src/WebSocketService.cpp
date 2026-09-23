#include "WebSocketService.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <freertos/task.h>

#include "esp_log.h"

/**
 * WebSocketService.cpp - WebSocket broadcast and command-receive service.
 *
 * Java equivalent: com.serial.service.WebSocketService
 */

static const char* TAG_WS = "WS";

/** Broadcast interval in milliseconds. Java equivalent: BROADCAST_INTERVAL_MS = 1000. */
static constexpr int BROADCAST_INTERVAL_MS = 1000;

// ---- WebSocket message keys ------------------------------------------------
// Java equivalent: private static final String KEY_SET_* constants.

static const char* KEY_SET_VOLTAGE = "setVoltage";
static const char* KEY_SET_CURRENT = "setCurrent";
static const char* KEY_SET_OUTPUT = "setOutput";
static const char* KEY_SET_KEYPAD = "setKeypad";

// File-scope pointer so the static onEvent handler can reach the instance.
// Set once in begin() before any clients can connect.
static WebSocketService* instance = nullptr;

// ---- Constructor -----------------------------------------------------------

WebSocketService::WebSocketService(AsyncWebSocket* ws, ConverterState* state)
    : ws(ws), state(state), commandQueue(nullptr) {
}

// ---- begin -----------------------------------------------------------------

/**
 * Creates the command queue, stores the instance pointer for the static event
 * handler, registers the event handler, and starts the broadcast FreeRTOS task.
 *
 * Java equivalent: WebSocketService#start
 */
void WebSocketService::begin() {
  // Single-slot queue: each item is one WsCommand struct.
  // xQueueOverwrite is used on write so the latest command always wins.
  commandQueue = xQueueCreate(1, sizeof(WsCommand));

  // Store instance pointer so the static onEvent handler can reach the queue.
  instance = this;

  ws->onEvent(onEvent);

  // Pass `this` as the task parameter so the static broadcastTask can access
  // instance members (ws, state). Same pattern as ModbusTransport.
  xTaskCreatePinnedToCore(broadcastTask, "WS_Broadcast", 4096, this, 1, NULL, 0);
  ESP_LOGI(TAG_WS, "WebSocketService started (endpoint: %s).", ws->url());
}

// ---- dequeueCommand --------------------------------------------------------

/**
 * Non-blocking dequeue. Returns true and fills cmd if a command is waiting.
 * Called from Application.cpp on Core 1 (the loop task that owns Modbus).
 */
bool WebSocketService::dequeueCommand(WsCommand& cmd) {
  return xQueueReceive(commandQueue, &cmd, 0) == pdTRUE;
}

// ---- broadcastTask ---------------------------------------------------------

/**
 * FreeRTOS task: serialises the full ConverterState to JSON and broadcasts it
 * to every connected WebSocket client every BROADCAST_INTERVAL_MS milliseconds.
 *
 * Uses vTaskDelayUntil so the period is stable regardless of how long JSON
 * serialisation takes.
 *
 * ws.cleanupClients() is called each cycle to reclaim memory for stale
 * connections (ESPAsyncWebServer requires the application to call this).
 *
 * Java equivalent: WebSocketService#broadcastLoop
 */
void WebSocketService::broadcastTask(void* param) {
  WebSocketService* self = static_cast<WebSocketService*>(param);
  TickType_t xLastWakeTime = xTaskGetTickCount();

  for (;;) {
    // Build the full ConverterState JSON snapshot.
    // Field names match the Java spec GET /api/state table exactly.
    JsonDocument doc;
    doc["deviceName"] = self->state->getDeviceName();
    doc["manufacturer"] = self->state->getManufacturer();
    doc["firmwareVersion"] = self->state->getFirmwareVersion();
    doc["deviceOnline"] = self->state->isDeviceOnline();
    doc["converterTopology"] = (int)self->state->getConverterTopology();
    doc["voltageOut"] = self->state->getVoltageOut();
    doc["currentOut"] = self->state->getCurrentOut();
    doc["powerOut"] = self->state->getPowerOut();
    doc["voltageIn"] = self->state->getVoltageIn();
    doc["temperatureCelsius"] = self->state->getTemperatureCelsius();
    doc["voltageSet"] = self->state->getVoltageSet();
    doc["currentSet"] = self->state->getCurrentSet();
    doc["outputEnabled"] = self->state->isOutputEnabled();
    doc["keypadLocked"] = self->state->isKeypadLocked();
    doc["cvMode"] = self->state->isCvMode();
    doc["protectionState"] = self->state->getProtectionState();
    doc["maxVoltage"] = self->state->getMaxVoltage();
    doc["minVoltage"] = self->state->getMinVoltage();
    doc["maxCurrent"] = self->state->getMaxCurrent();
    doc["minCurrent"] = self->state->getMinCurrent();
    doc["maxPower"] = self->state->getMaxPower();
    doc["configMaxVoltage"] = self->state->getConfigMaxVoltage();
    doc["configMaxCurrent"] = self->state->getConfigMaxCurrent();

    String json;
    serializeJson(doc, json);
    self->ws->textAll(json);
    self->ws->cleanupClients();

    // Stable 1-second period — absorbs time spent in serialisation.
    // Java equivalent: Thread.sleep(BROADCAST_INTERVAL_MS).
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(BROADCAST_INTERVAL_MS));
  }
}

// ---- onEvent ---------------------------------------------------------------

/**
 * Static WebSocket event handler dispatching all four event types.
 *
 * Java equivalent:
 *   WS_EVT_CONNECT    → WebSocketService#onConnect
 *   WS_EVT_DISCONNECT → WebSocketService#onClose
 *   WS_EVT_ERROR      → WebSocketService#onError
 *   WS_EVT_DATA       → WebSocketService#onMessage (Step 5 stub; full impl in Step 6)
 */
void WebSocketService::onEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                               AwsEventType type, void* arg, uint8_t* data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      // Java equivalent: WebSocketService#onConnect — add client to set, log.
      ESP_LOGI(TAG_WS, "Client #%u connected from %s",
               client->id(), client->remoteIP().toString().c_str());
      break;

    case WS_EVT_DISCONNECT:
      // Java equivalent: WebSocketService#onClose — remove client from set, log.
      ESP_LOGI(TAG_WS, "Client #%u disconnected.", client->id());
      break;

    case WS_EVT_ERROR:
      // Java equivalent: WebSocketService#onError — normal on peer disconnect.
      ESP_LOGW(TAG_WS, "Client #%u error.", client->id());
      break;

    case WS_EVT_DATA: {
      // Java equivalent: WebSocketService#onMessage
      //
      // ESP32 deviation: WS_EVT_DATA fires on the lwIP async TCP task (Core 0,
      // high-priority, shared with all WiFi I/O). Blocking here with a Modbus
      // call would stall the network stack. The parsed command is instead enqueued
      // into a single-slot queue; Application.cpp drains it on Core 1 where Modbus
      // calls are safe.
      //
      // Step 5 stub: log the payload and respond {"error":"not implemented"}.
      // Step 6 will parse the JSON, populate a WsCommand, and enqueue it.
      if (data != nullptr && len > 0) {
        char buf[257];
        size_t copy = (len < 256) ? len : 256;
        memcpy(buf, data, copy);
        buf[copy] = '\0';
        ESP_LOGI(TAG_WS, "Client #%u message: %s", client->id(), buf);
      }
      client->text("{\"error\":\"not implemented\"}");
      break;
    }

    default:
      break;
  }
}
