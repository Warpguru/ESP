#include "WebSocketService.h"

#include <Arduino.h>

/**
 * WebSocketService.cpp - WebSocket broadcast service.
 *
 * Java equivalent: com.serial.service.WebSocketService
 */

WebSocketService::WebSocketService(AsyncWebSocket* ws, ConverterState* state)
    : ws(ws), state(state) {
}

void WebSocketService::begin() {
  this->ws->onEvent(onEvent);
  xTaskCreatePinnedToCore(broadcastTask, "WS_Broadcast", 4096, this, 1, NULL, 0);
}

void WebSocketService::onEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                               AwsEventType type, void* arg, uint8_t* data, size_t len) {
  // TODO: Port from com.serial.service.WebSocketService
}

void WebSocketService::broadcastTask(void* param) {
  // TODO: Port broadcast loop from com.serial.service.WebSocketService
  WebSocketService* self = static_cast<WebSocketService*>(param);
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
