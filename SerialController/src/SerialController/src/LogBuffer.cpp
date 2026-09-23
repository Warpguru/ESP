#include "LogBuffer.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/**
 * LogBuffer.cpp - Levelled logger with in-RAM ring buffer and Serial output.
 *
 * Level ordering matches Log4j2: higher integer = less severe.
 * A message is emitted when its level integer >= the active level integer.
 * Default active level: INFO (200). TRACE (0) is most verbose.
 *
 * Each Log_info/warn/error/debug/trace(fmt, ...) macro call:
 *   1. Expands to logInfo(__FILE__, __LINE__, fmt, ...) — call site captured.
 *   2. Checks the active level; discards if below threshold.
 *   3. Formats "[LEVEL][file:line] message" onto the stack.
 *   4. Prints to Serial (UART0) — no hooks, always works.
 *   5. Appends to the ring buffer under a FreeRTOS mutex.
 *
 * No ESP_LOG* macros, no linker wraps, no vprintf hooks.
 *
 * No Java equivalent — ESP32-specific remote diagnostics facility.
 */

// Global singleton defined here; declared extern in LogBuffer.h.
LogBufferClass Log;

// ---- Helpers ---------------------------------------------------------------

static const char* levelName(LogLevel lv) {
  switch (lv) {
    case LogLevel::ERROR:
      return "ERROR";
    case LogLevel::WARN:
      return "WARN ";
    case LogLevel::INFO:
      return "INFO ";
    case LogLevel::DEBUG:
      return "DEBUG";
    case LogLevel::TRACE:
      return "TRACE";
    default:
      return "?    ";
  }
}

// Strip directory prefix from __FILE__ so only the filename appears in output.
static const char* fileBasename(const char* path) {
  const char* p = path;
  while (*path) {
    if (*path == '/' || *path == '\\') {
      p = path + 1;
    }
    path++;
  }
  return p;
}

// ---- LogBufferClass --------------------------------------------------------

void LogBufferClass::begin() {
  mutex = xSemaphoreCreateMutex();
}

void LogBufferClass::logError(const char* file, int line, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  append(LogLevel::ERROR, file, line, fmt, args);
  va_end(args);
}

void LogBufferClass::logWarn(const char* file, int line, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  append(LogLevel::WARN, file, line, fmt, args);
  va_end(args);
}

void LogBufferClass::logInfo(const char* file, int line, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  append(LogLevel::INFO, file, line, fmt, args);
  va_end(args);
}

void LogBufferClass::logDebug(const char* file, int line, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  append(LogLevel::DEBUG, file, line, fmt, args);
  va_end(args);
}

void LogBufferClass::logTrace(const char* file, int line, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  append(LogLevel::TRACE, file, line, fmt, args);
  va_end(args);
}

LogLevel LogBufferClass::getLevel() const {
  return level;
}

void LogBufferClass::setLevel(LogLevel lv) {
  level = lv;
}

bool LogBufferClass::setLevelFromString(const char* name) {
  if (strcasecmp(name, "ERROR") == 0) {
    level = LogLevel::ERROR;
    return true;
  }
  if (strcasecmp(name, "WARN") == 0) {
    level = LogLevel::WARN;
    return true;
  }
  if (strcasecmp(name, "INFO") == 0) {
    level = LogLevel::INFO;
    return true;
  }
  if (strcasecmp(name, "DEBUG") == 0) {
    level = LogLevel::DEBUG;
    return true;
  }
  if (strcasecmp(name, "TRACE") == 0) {
    level = LogLevel::TRACE;
    return true;
  }
  return false;
}

const char* LogBufferClass::getLevelName() const {
  return levelName(level);
}

void LogBufferClass::getJson(String& out) const {
  // Build JSON directly from the ring buffer under the mutex.
  // No large stack allocation — ArduinoJson allocates on the heap.
  xSemaphoreTake(mutex, portMAX_DELAY);

  int oldest = (count < LOG_BUFFER_LINES) ? 0 : head;
  int n = count;

  JsonDocument doc;
  doc["level"] = getLevelName();
  JsonArray arr = doc["log"].to<JsonArray>();
  for (int i = 0; i < n; i++) {
    int idx = (oldest + i) % LOG_BUFFER_LINES;
    arr.add(lines[idx]);
  }

  xSemaphoreGive(mutex);

  serializeJson(doc, out);
}

void LogBufferClass::clear() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  head = 0;
  count = 0;
  xSemaphoreGive(mutex);
}

// ---- Private ---------------------------------------------------------------

void LogBufferClass::append(LogLevel lv, const char* file, int line, const char* fmt, va_list args) {
  // Log4j2 semantics: emit when message level integer >= active level integer.
  // e.g. active=INFO(200): emit ERROR(400) and WARN(300), suppress DEBUG(100) and TRACE(0).
  if (static_cast<int>(lv) < static_cast<int>(level)) {
    return;
  }

  // Format the message, then the full log line with location prefix.
  char msg[LOG_BUFFER_LINE_LEN];
  vsnprintf(msg, sizeof(msg), fmt, args);

  char logLine[LOG_BUFFER_LINE_LEN];
  snprintf(logLine, sizeof(logLine), "[%s][%s:%d] %s",
           levelName(lv), fileBasename(file), line, msg);

  // Print to Serial (UART0) — no hooks, no build flags required.
  Serial.println(logLine);

  // Append to ring buffer under mutex.
  if (mutex == nullptr) {
    return;
  }

  xSemaphoreTake(mutex, portMAX_DELAY);

  strncpy(lines[head], logLine, LOG_BUFFER_LINE_LEN - 1);
  lines[head][LOG_BUFFER_LINE_LEN - 1] = '\0';
  head = (head + 1) % LOG_BUFFER_LINES;
  if (count < LOG_BUFFER_LINES) {
    count++;
  }

  xSemaphoreGive(mutex);
}
