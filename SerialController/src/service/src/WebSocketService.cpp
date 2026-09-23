#include "WebSocketService.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <freertos/task.h>

#include "../../../src/SerialController/src/LogBuffer.h"

/**
 * WebSocketService.cpp - WebSocket broadcast and command-receive service.
 *
 * Java equivalent: com.serial.service.WebSocketService
 */

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
  // Pinned to Core 1 (same as pollingTask and the Arduino loop task) so that
  // ws->textAll() and cleanupClients() never compete with the WiFi/lwIP stack
  // and ESPAsyncWebServer TCP callbacks on Core 0.  Running on Core 0 caused
  // HTTP responses (GET /doc, GET /status) to stall during each 1-second broadcast.
  xTaskCreatePinnedToCore(broadcastTask, "WS_Broadcast", 4096, this, 1, NULL, 1);
  Log_info("WebSocketService started (endpoint: %s).", ws->url());
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
    // Serialise as the enum name string so the browser JS compares directly against
    // the TOPOLOGY_BUCK / TOPOLOGY_BOOST / TOPOLOGY_BUCK_BOOST string constants.
    // Sending an integer breaks the buck voltage-ceiling logic and the amber marker.
    switch (self->state->getConverterTopology()) {
      case ConverterTopology::BOOST:
        doc["converterTopology"] = "BOOST";
        break;
      case ConverterTopology::BUCK_BOOST:
        doc["converterTopology"] = "BUCK_BOOST";
        break;
      default:
        doc["converterTopology"] = "BUCK";
        break;
    }
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

    // Stable 1-second period - absorbs time spent in serialisation.
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
      // Java equivalent: WebSocketService#onConnect - add client to set, log.
      Log_info("Client #%u connected from %s",
               client->id(), client->remoteIP().toString().c_str());
      break;

    case WS_EVT_DISCONNECT:
      // Java equivalent: WebSocketService#onClose - remove client from set, log.
      Log_info("Client #%u disconnected.", client->id());
      break;

    case WS_EVT_ERROR:
      // Java equivalent: WebSocketService#onError - normal on peer disconnect.
      Log_warn("Client #%u error.", client->id());
      break;

    case WS_EVT_DATA: {
      // Java equivalent: WebSocketService#onMessage
      //
      // ESP32 deviation: WS_EVT_DATA fires on the lwIP async TCP task (Core 0,
      // high-priority, shared with all WiFi I/O). Blocking here with a Modbus
      // call would stall the network stack. Each recognised key is parsed into a
      // WsCommand and enqueued via xQueueOverwrite (non-blocking, latest wins).
      // Application.cpp drains the queue on Core 1 where Modbus calls are safe.
      if (data == nullptr || len == 0) {
        break;
      }

      // Null-terminate for ArduinoJson (stack buffer; 256-byte cap matches typical command size).
      char buf[257];
      size_t copy = (len < 256) ? len : 256;
      memcpy(buf, data, copy);
      buf[copy] = '\0';
      Log_info("Client #%u message: %s", client->id(), buf);

      // Parse incoming JSON.
      // Java equivalent: objectMapper.readValue(msg, Map.class)
      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, buf);
      if (err) {
        // Java equivalent: logger.warn("Failed to parse WebSocket message: {}", e.getMessage())
        Log_warn("Client #%u malformed JSON: %s", client->id(), err.c_str());
        break;
      }

      // Dispatch each recognised key - matches Java onMessage key iteration order.
      // xQueueOverwrite is used so the latest command always wins if the loop
      // hasn't drained the previous one yet (single-slot queue).
      bool dispatched = false;

      if (doc.containsKey(KEY_SET_VOLTAGE)) {
        WsCommand cmd;
        cmd.type = WsCommand::SET_VOLTAGE;
        cmd.value = doc[KEY_SET_VOLTAGE].as<double>();
        xQueueOverwrite(instance->commandQueue, &cmd);
        dispatched = true;
      }

      if (doc.containsKey(KEY_SET_CURRENT)) {
        WsCommand cmd;
        cmd.type = WsCommand::SET_CURRENT;
        cmd.value = doc[KEY_SET_CURRENT].as<double>();
        xQueueOverwrite(instance->commandQueue, &cmd);
        dispatched = true;
      }

      if (doc.containsKey(KEY_SET_OUTPUT)) {
        WsCommand cmd;
        cmd.type = WsCommand::SET_OUTPUT;
        cmd.flag = doc[KEY_SET_OUTPUT].as<bool>();
        xQueueOverwrite(instance->commandQueue, &cmd);
        dispatched = true;
      }

      if (doc.containsKey(KEY_SET_KEYPAD)) {
        WsCommand cmd;
        cmd.type = WsCommand::SET_KEYPAD;
        cmd.flag = doc[KEY_SET_KEYPAD].as<bool>();
        xQueueOverwrite(instance->commandQueue, &cmd);
        dispatched = true;
      }

      // Log unrecognised keys at DEBUG - ignored, connection kept open.
      // Java equivalent: logger.debug("WebSocket message: unrecognised key '{}' - ignored.")
      if (!dispatched) {
        Log_debug("Client #%u message contained no recognised keys - ignored.", client->id());
      }
      break;
    }

    default:
      break;
  }
}
