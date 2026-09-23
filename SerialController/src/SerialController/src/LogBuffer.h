#pragma once

#include <Arduino.h>

/**
 * LogBuffer.h - Levelled logger with in-RAM ring buffer and Serial output.
 *
 * Replaces all ESP_LOG* calls throughout the codebase. Each log call:
 *   1. Prints a formatted line to UART0 via Serial.println() — always works,
 *      regardless of Arduino IDE / PlatformIO / build flags.
 *   2. Appends the same line to a fixed-size ring buffer accessible via
 *      GET /api/log, so logs are retrievable without a USB connection.
 *
 * A runtime log level (default INFO) gates both outputs identically.
 *
 * Use the macros — __FILE__ and __LINE__ are captured automatically:
 *   Log_error(fmt, ...)   — level ERROR
 *   Log_warn( fmt, ...)   — level WARN
 *   Log_info( fmt, ...)   — level INFO  (default active level)
 *   Log_debug(fmt, ...)   — level DEBUG
 *   Log_trace(fmt, ...)   — level TRACE
 *
 * Output format:
 *   [INFO ][Application.cpp:52] Starting SerialController v1.0.0 ...
 *
 * The active level is readable/writable at runtime via:
 *   Log.getLevel() / Log.setLevel(LogLevel)
 * and over HTTP via PUT /api/log/level {"level":"DEBUG"}.
 *
 * No Java equivalent — ESP32-specific remote diagnostics facility.
 * Java uses Log4j2/SLF4J; this is the direct embedded equivalent.
 */

// Ring buffer dimensions.
// 32 lines × 160 chars = ~5 KB — well within the ~300 KB free heap budget.
static constexpr int LOG_BUFFER_LINES = 32;
static constexpr int LOG_BUFFER_LINE_LEN = 160;

// Log4j2-compatible integer ordering: higher integer = less severe.
// A message is emitted when its level integer >= the active level integer,
// matching Log4j2 semantics: event.level.intLevel() >= logger.level.intLevel().
enum class LogLevel {
  TRACE = 0,
  DEBUG = 100,
  INFO = 200,  // default
  WARN = 300,
  ERROR = 400
};

class LogBufferClass {
 public:
  /**
   * Initialise the ring buffer mutex. Must be called once from
   * applicationSetup() before any Log_* macro call.
   */
  void begin();

  // Internal methods — do not call directly. Use the Log_* macros below.
  void logError(const char* file, int line, const char* fmt, ...);
  void logWarn(const char* file, int line, const char* fmt, ...);
  void logInfo(const char* file, int line, const char* fmt, ...);
  void logDebug(const char* file, int line, const char* fmt, ...);
  void logTrace(const char* file, int line, const char* fmt, ...);

  /** Get the current active log level. */
  LogLevel getLevel() const;

  /**
   * Set the active log level at runtime.
   * Messages with a lower integer level are silently discarded.
   */
  void setLevel(LogLevel lv);

  /**
   * Parse a level name string ("ERROR","WARN","INFO","DEBUG","TRACE").
   * Returns false and leaves the level unchanged if unrecognised.
   */
  bool setLevelFromString(const char* name);

  /** Name of the current log level as a C string (e.g. "INFO "). */
  const char* getLevelName() const;

  /**
   * Serialise the ring buffer to a JSON string.
   * Format: {"log":["line0","line1",...],"level":"INFO "}
   * Built under the mutex — no large stack allocation.
   */
  void getJson(String& out) const;

  /** Clear all entries from the ring buffer. Thread-safe. */
  void clear();

 private:
  void append(LogLevel lv, const char* file, int line, const char* fmt, va_list args);

  char lines[LOG_BUFFER_LINES][LOG_BUFFER_LINE_LEN];
  int head = 0;
  int count = 0;
  LogLevel level = LogLevel::INFO;
  SemaphoreHandle_t mutex = nullptr;
};

/** Global singleton. */
extern LogBufferClass Log;

// ---- Convenience macros ----------------------------------------------------
// __FILE__ and __LINE__ are captured at the call site by the preprocessor.
// ## before __VA_ARGS__ swallows the trailing comma when no extra args are given
// (GNU extension; supported by GCC/Clang/MSVC and all Arduino toolchains).
#define Log_error(fmt, ...) Log.logError(__FILE__, __LINE__, fmt, ##__VA_ARGS__)  // NOLINT
#define Log_warn(fmt, ...) Log.logWarn(__FILE__, __LINE__, fmt, ##__VA_ARGS__)    // NOLINT
#define Log_info(fmt, ...) Log.logInfo(__FILE__, __LINE__, fmt, ##__VA_ARGS__)    // NOLINT
#define Log_debug(fmt, ...) Log.logDebug(__FILE__, __LINE__, fmt, ##__VA_ARGS__)  // NOLINT
#define Log_trace(fmt, ...) Log.logTrace(__FILE__, __LINE__, fmt, ##__VA_ARGS__)  // NOLINT
