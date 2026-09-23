#pragma once

/**
 * Server.h - Public API for WiFi management and the RESTful HTTP server.
 *
 * Java equivalent: com.serial.SerialController — the top-level wiring class
 * that creates the Javalin server, registers routes, and starts the service layer.
 *
 * Initialise WiFi via WiFiManager and start the ESPAsyncWebServer by calling
 * setupServer() once from applicationSetup(). handleServerRequests() is a
 * no-op kept for API symmetry — ESPAsyncWebServer is fully non-blocking.
 */

/**
 * Connects to WiFi using WiFiManager and starts the async HTTP + WebSocket server.
 * Must be called once from applicationSetup().
 *
 * Java equivalent: SerialController#process — constructs DeviceService,
 * RestService, WebSocketService, registers routes, and calls javalin.start().
 */
void setupServer();

/**
 * No-op: ESPAsyncWebServer handles all requests on its own internal FreeRTOS task.
 * Kept for API compatibility with the call site in applicationLoop().
 *
 * Java equivalent: not needed — Javalin's Jetty server handles requests on its own
 * thread pool without any manual pump call.
 */
void handleServerRequests();
