#pragma once

#include <ESPAsyncWebServer.h>

#include "ConverterState.h"

/**
 * WebSocketService.h - WebSocket broadcast service.
 *
 * Java equivalent: com.serial.service.WebSocketService
 *
 * Broadcasts converter state as JSON to all connected WebSocket clients
 * at a configurable interval from a dedicated FreeRTOS task.
 */
class WebSocketService {
 public:
  /**
   * Constructs the WebSocket service.
   *
   * @param ws    AsyncWebSocket instance (owned externally)
   * @param state shared converter state (owned externally)
   */
  WebSocketService(AsyncWebSocket* ws, ConverterState* state);

  /**
   * Starts the broadcast FreeRTOS task and registers the WebSocket event handler.
   * Must be called before server.begin().
   */
  void begin();

  /**
   * WebSocket event handler — registered with AsyncWebSocket::onEvent().
   */
  static void onEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                      AwsEventType type, void* arg, uint8_t* data, size_t len);

 private:
  AsyncWebSocket* ws;
  ConverterState* state;

  static void broadcastTask(void* param);
};
