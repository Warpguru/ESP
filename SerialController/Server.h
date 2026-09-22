#pragma once

/**
 * Server.h - Public API for WiFi management and the RESTful HTTP server.
 *
 * Initialize WiFi via WiFiManager and start the ESPAsyncWebServer by calling
 * setupServer() once from applicationSetup(). handleServerRequests() is a
 * no-op kept for API symmetry — ESPAsyncWebServer is fully non-blocking.
 */

/**
 * Connects to WiFi using WiFiManager and starts the async HTTP + WebSocket server.
 * Must be called once from applicationSetup().
 */
void setupServer();

/**
 * No-op: ESPAsyncWebServer handles all requests on its own internal FreeRTOS task.
 * Kept for API compatibility with the call site in applicationLoop().
 */
void handleServerRequests();
