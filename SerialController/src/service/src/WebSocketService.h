#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "ConverterState.h"

/**
 * WebSocketService.h - WebSocket broadcast and command-receive service.
 *
 * Java equivalent: com.serial.service.WebSocketService
 *
 * Push:  A FreeRTOS task broadcasts the full ConverterState as JSON to every
 *        connected client every second, using vTaskDelayUntil for a stable period.
 *
 * Receive:  Incoming JSON commands from the browser are enqueued into a
 *           single-slot FreeRTOS queue and returned to the caller via
 *           dequeueCommand(). The Application loop drains this queue and
 *           executes the commands on Core 1 - the same task that owns the
 *           Modbus transport - avoiding any blocking call on the lwIP async TCP
 *           task that delivers WS_EVT_DATA.
 *
 *           Java uses a direct synchronized call from the WS thread because JVM
 *           threads can safely block. On ESP32 blocking the lwIP task stalls the
 *           entire WiFi stack, so the queue pattern is used instead.
 */

/**
 * A parsed WebSocket command sent by the browser.
 *
 * Java equivalent: the individual key/value pairs extracted inside
 * WebSocketService#onMessage. Each WsCommand carries at most one action.
 */
struct WsCommand {
  enum Type {
    NONE,
    SET_VOLTAGE,  // value: double
    SET_CURRENT,  // value: double
    SET_OUTPUT,   // flag: bool
    SET_KEYPAD,   // flag: bool
  } type = NONE;

  double value = 0.0;  // used for SET_VOLTAGE, SET_CURRENT
  bool flag = false;   // used for SET_OUTPUT, SET_KEYPAD
};

class WebSocketService {
 public:
  /**
   * Constructs the WebSocket service.
   *
   * @param ws    AsyncWebSocket instance (owned externally, must outlive this object)
   * @param state shared converter state (owned externally)
   */
  WebSocketService(AsyncWebSocket* ws, ConverterState* state);

  /**
   * Registers the WebSocket event handler and starts the broadcast FreeRTOS task.
   * Must be called before server.begin().
   *
   * Java equivalent: WebSocketService#start
   */
  void begin();

  /**
   * Attempts to dequeue one pending command from the command queue.
   * Returns true and fills cmd if a command is waiting; returns false immediately
   * if the queue is empty. Non-blocking - safe to call every loop iteration.
   *
   * Called from Application.cpp's polling loop (Core 1) to execute commands on
   * the task that owns the Modbus transport.
   *
   * Java equivalent: inlined into WebSocketService#onMessage which calls
   * deviceService methods directly. The queue is the ESP32-safe equivalent.
   */
  bool dequeueCommand(WsCommand& cmd);

  /**
   * WebSocket event handler - registered with AsyncWebSocket::onEvent().
   * Handles connect, disconnect, error, and data events.
   *
   * Java equivalent: WebSocketService#onConnect / #onClose / #onError / #onMessage
   */
  static void onEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                      AwsEventType type, void* arg, uint8_t* data, size_t len);

 private:
  /** Java equivalent: private final ... ws (the Javalin WsContext set / AsyncWebSocket here). */
  AsyncWebSocket* ws;

  /** Java equivalent: private final DeviceService deviceService (state half). */
  ConverterState* state;

  /**
   * Single-slot command queue. WS_EVT_DATA handler writes here (non-blocking);
   * Application loop reads here. xQueueOverwrite ensures the latest command
   * always wins if the loop hasn't drained the previous one yet.
   *
   * Java equivalent: the synchronized deviceService call in onMessage serves
   * the same purpose; this queue is the ESP32-safe equivalent.
   */
  QueueHandle_t commandQueue;

  /**
   * Broadcast FreeRTOS task. Wakes every 1 s via vTaskDelayUntil, serialises
   * ConverterState to JSON, and calls ws.textAll().
   *
   * Java equivalent: WebSocketService#broadcastLoop running on broadcastThread.
   */
  static void broadcastTask(void* param);
};
