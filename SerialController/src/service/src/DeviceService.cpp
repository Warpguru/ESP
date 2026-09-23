#include "DeviceService.h"

#include <Arduino.h>
#include <freertos/task.h>

#include "esp_log.h"

/**
 * DeviceService.cpp - Service layer for device polling and validated setpoint writes.
 *
 * Java equivalent: com.serial.service.DeviceService
 */

static const char* TAG_DS = "DEVICE_SVC";

/** Polling interval — Java equivalent: POLL_INTERVAL_MS = 1000. */
static constexpr uint32_t POLL_INTERVAL_MS = 1000;

/**
 * Settle window in milliseconds after a setpoint write, during which the poll
 * task must not overwrite the just-written value with a potentially transient
 * device read-back.
 *
 * Java equivalent: DeviceService.SETPOINT_SETTLE_MS = 2000L
 */
static constexpr uint32_t SETPOINT_SETTLE_MS = 2000;

/** Delay between write and read-back in verified-write operations (ms). */
static constexpr uint32_t VERIFIED_READBACK_DELAY_MS = 50;

/** Tolerance for verified-write read-back comparison. */
static constexpr double VERIFIED_TOLERANCE = 0.01;

/**
 * Consecutive non-timeout failures before marking Offline and reconnecting.
 *
 * Java equivalent: MAX_CONSECUTIVE_FAILURES = 3
 */
static constexpr int MAX_CONSECUTIVE_FAILURES = 3;

/**
 * Consecutive successes (while Offline) before marking Online again.
 *
 * Java equivalent: MAX_CONSECUTIVE_SUCCESSES = 1
 */
static constexpr int MAX_CONSECUTIVE_SUCCESSES = 1;

/**
 * Dropout voltage for BUCK topology ceiling enforcement.
 *
 * Java equivalent: BUCK_DROPOUT_V = 1.0
 */
static constexpr double BUCK_DROPOUT_V = 1.0;

// ---- Constructor -----------------------------------------------------------

DeviceService::DeviceService(ConverterState* state, DC2DCConverter* converter)
    : _state(state), _converter(converter) {
  _mutex = xSemaphoreCreateMutex();
}

// ---- begin -----------------------------------------------------------------

/**
 * Starts the background Modbus polling FreeRTOS task.
 * Pinned to Core 1 alongside the Arduino loop() task so that Modbus I/O
 * does not compete with the WiFi/lwIP stack on Core 0.
 *
 * Java equivalent: DeviceService#start
 */
void DeviceService::begin() {
  xTaskCreatePinnedToCore(pollingTask, "DeviceService_Poll", 4096, this, 2, NULL, 1);
  ESP_LOGI(TAG_DS, "DeviceService polling task started on Core 1.");
}

// ---- isDeviceDetected ------------------------------------------------------

/**
 * Java equivalent: DeviceService#isDeviceDetected
 */
bool DeviceService::isDeviceDetected() const {
  return (_converter != nullptr) && (_converter->getDevice() != nullptr);
}

// ---- getState --------------------------------------------------------------

const ConverterState* DeviceService::getState() const {
  return _state;
}

// ---- setVoltage ------------------------------------------------------------

/**
 * Sets the voltage setpoint with range validation and settle-window suppression.
 *
 * Java equivalent: DeviceService#setVoltage (synchronized)
 * Deviation: bool return instead of void/throws; mutex instead of synchronized.
 */
bool DeviceService::setVoltage(double volts) {
  double maxV = effectiveMaxVoltage();
  if (!validateRange("Voltage", volts, _state->getMinVoltage(), maxV)) {
    return false;
  }
  xSemaphoreTake(_mutex, portMAX_DELAY);
  ESP_LOGI(TAG_DS, "Setting voltage to %.3f V", volts);
  bool ok = _converter->setVoltage(volts);
  if (ok) {
    _state->setVoltageSet(volts);
    _voltagePendingUntil = millis() + SETPOINT_SETTLE_MS;
  }
  xSemaphoreGive(_mutex);
  return ok;
}

// ---- setVoltageVerified ----------------------------------------------------

/**
 * Sets the voltage setpoint and synchronously verifies via read-back.
 * Retries once after VERIFIED_READBACK_DELAY_MS. Sets outConflict=true
 * if the device did not accept the value after two attempts.
 *
 * Java equivalent: DeviceService#setVoltageVerified (synchronized)
 */
bool DeviceService::setVoltageVerified(double volts, double& confirmedOut, bool& outConflict) {
  outConflict = false;
  double maxV = effectiveMaxVoltage();
  if (!validateRange("Voltage", volts, _state->getMinVoltage(), maxV)) {
    return false;
  }
  xSemaphoreTake(_mutex, portMAX_DELAY);
  ESP_LOGI(TAG_DS, "setVoltageVerified: writing %.3f V", volts);
  bool ok = _converter->setVoltage(volts);
  if (!ok) {
    xSemaphoreGive(_mutex);
    return false;
  }
  _voltagePendingUntil = millis() + SETPOINT_SETTLE_MS;
  vTaskDelay(pdMS_TO_TICKS(VERIFIED_READBACK_DELAY_MS));
  double confirmed = _converter->getVoltageSetVerified();
  if (fabs(confirmed - volts) > VERIFIED_TOLERANCE) {
    ESP_LOGD(TAG_DS, "setVoltageVerified: first read-back %.3f, retrying", confirmed);
    vTaskDelay(pdMS_TO_TICKS(VERIFIED_READBACK_DELAY_MS));
    confirmed = _converter->getVoltageSetVerified();
    if (fabs(confirmed - volts) > VERIFIED_TOLERANCE) {
      ESP_LOGW(TAG_DS, "setVoltageVerified: device did not accept %.3f V (read back %.3f V)", volts, confirmed);
      outConflict = true;
      xSemaphoreGive(_mutex);
      return false;
    }
  }
  ESP_LOGI(TAG_DS, "setVoltageVerified: confirmed %.3f V", confirmed);
  _state->setVoltageSet(confirmed);
  confirmedOut = confirmed;
  xSemaphoreGive(_mutex);
  return true;
}

// ---- setCurrent ------------------------------------------------------------

/**
 * Java equivalent: DeviceService#setCurrent (synchronized)
 */
bool DeviceService::setCurrent(double amperes) {
  double maxI = effectiveMaxCurrent();
  if (!validateRange("Current", amperes, _state->getMinCurrent(), maxI)) {
    return false;
  }
  xSemaphoreTake(_mutex, portMAX_DELAY);
  ESP_LOGI(TAG_DS, "Setting current to %.3f A", amperes);
  bool ok = _converter->setCurrent(amperes);
  if (ok) {
    _state->setCurrentSet(amperes);
    _currentPendingUntil = millis() + SETPOINT_SETTLE_MS;
  }
  xSemaphoreGive(_mutex);
  return ok;
}

// ---- setCurrentVerified ----------------------------------------------------

/**
 * Java equivalent: DeviceService#setCurrentVerified (synchronized)
 */
bool DeviceService::setCurrentVerified(double amperes, double& confirmedOut, bool& outConflict) {
  outConflict = false;
  double maxI = effectiveMaxCurrent();
  if (!validateRange("Current", amperes, _state->getMinCurrent(), maxI)) {
    return false;
  }
  xSemaphoreTake(_mutex, portMAX_DELAY);
  ESP_LOGI(TAG_DS, "setCurrentVerified: writing %.3f A", amperes);
  bool ok = _converter->setCurrent(amperes);
  if (!ok) {
    xSemaphoreGive(_mutex);
    return false;
  }
  _currentPendingUntil = millis() + SETPOINT_SETTLE_MS;
  vTaskDelay(pdMS_TO_TICKS(VERIFIED_READBACK_DELAY_MS));
  double confirmed = _converter->getCurrentSetVerified();
  if (fabs(confirmed - amperes) > VERIFIED_TOLERANCE) {
    ESP_LOGD(TAG_DS, "setCurrentVerified: first read-back %.3f, retrying", confirmed);
    vTaskDelay(pdMS_TO_TICKS(VERIFIED_READBACK_DELAY_MS));
    confirmed = _converter->getCurrentSetVerified();
    if (fabs(confirmed - amperes) > VERIFIED_TOLERANCE) {
      ESP_LOGW(TAG_DS, "setCurrentVerified: device did not accept %.3f A (read back %.3f A)", amperes, confirmed);
      outConflict = true;
      xSemaphoreGive(_mutex);
      return false;
    }
  }
  ESP_LOGI(TAG_DS, "setCurrentVerified: confirmed %.3f A", confirmed);
  _state->setCurrentSet(confirmed);
  confirmedOut = confirmed;
  xSemaphoreGive(_mutex);
  return true;
}

// ---- setVoltageCurrent -----------------------------------------------------

/**
 * Sets voltage and current atomically in a single 0x10 frame.
 *
 * Java equivalent: DeviceService#setMeasurements (synchronized)
 */
bool DeviceService::setVoltageCurrent(double volts, double amperes) {
  if (!validateRange("Voltage", volts, _state->getMinVoltage(), effectiveMaxVoltage())) {
    return false;
  }
  if (!validateRange("Current", amperes, _state->getMinCurrent(), effectiveMaxCurrent())) {
    return false;
  }
  xSemaphoreTake(_mutex, portMAX_DELAY);
  ESP_LOGI(TAG_DS, "Setting voltage %.3f V and current %.3f A (atomic)", volts, amperes);
  bool ok = _converter->setVoltageCurrent(volts, amperes);
  if (ok) {
    _state->setVoltageSet(volts);
    _state->setCurrentSet(amperes);
    _voltagePendingUntil = millis() + SETPOINT_SETTLE_MS;
    _currentPendingUntil = millis() + SETPOINT_SETTLE_MS;
  }
  xSemaphoreGive(_mutex);
  return ok;
}

// ---- setOutput -------------------------------------------------------------

/**
 * Java equivalent: DeviceService#setOutput (synchronized)
 */
bool DeviceService::setOutput(bool on) {
  xSemaphoreTake(_mutex, portMAX_DELAY);
  ESP_LOGI(TAG_DS, "Setting output to %s", on ? "ON" : "OFF");
  bool ok = _converter->setOutput(on);
  if (ok) {
    _state->setOutputEnabled(on);
  }
  xSemaphoreGive(_mutex);
  return ok;
}

// ---- setKeypad -------------------------------------------------------------

/**
 * Java equivalent: DeviceService#setKeypad (synchronized)
 */
bool DeviceService::setKeypad(bool locked) {
  xSemaphoreTake(_mutex, portMAX_DELAY);
  ESP_LOGI(TAG_DS, "Setting keypad lock to %s", locked ? "LOCKED" : "UNLOCKED");
  bool ok = _converter->setKeypad(locked);
  if (ok) {
    _state->setKeypadLocked(locked);
  }
  xSemaphoreGive(_mutex);
  return ok;
}

// ---- clearProtection -------------------------------------------------------

/**
 * Java equivalent: DeviceService#clearProtection (synchronized)
 */
bool DeviceService::clearProtection() {
  xSemaphoreTake(_mutex, portMAX_DELAY);
  ESP_LOGI(TAG_DS, "Clearing protection state.");
  bool ok = _converter->setProtectionState(false);
  if (ok) {
    _state->setProtectionState(0);
  }
  xSemaphoreGive(_mutex);
  return ok;
}

// ---- pollingTask -----------------------------------------------------------

/**
 * FreeRTOS task entry point. Calls poll() every POLL_INTERVAL_MS.
 *
 * Java equivalent: DeviceService#pollLoop running on pollThread.
 */
void DeviceService::pollingTask(void* param) {
  DeviceService* self = static_cast<DeviceService*>(param);
  for (;;) {
    self->poll();
    vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
  }
}

// ---- poll ------------------------------------------------------------------

/**
 * Reads all device registers in one bulk 0x03 frame and updates ConverterState.
 * Acquires _mutex to prevent concurrent serial access from write operations.
 * Respects the post-write settle windows for voltageSet and currentSet.
 *
 * Java equivalent: DeviceService#poll (synchronized)
 */
void DeviceService::poll() {
  if (_converter == nullptr) {
    return;
  }

  // Per-poll counters — static locals mirror the Java instance fields.
  // Java: private int consecutiveFailures / consecutiveSuccesses.
  static int consecutiveFailures = 0;
  static int consecutiveSuccesses = 0;

  xSemaphoreTake(_mutex, portMAX_DELAY);
  bool ok = _converter->pollAll();
  xSemaphoreGive(_mutex);

  if (ok) {
    consecutiveSuccesses++;
    consecutiveFailures = 0;

    _state->setVoltageOut(_converter->getVoltage());
    _state->setCurrentOut(_converter->getCurrent());
    _state->setPowerOut(_converter->getPower());
    _state->setVoltageIn(_converter->getInputVoltage());
    _state->setTemperatureCelsius(_converter->getTemperatureCelsius());
    _state->setOutputEnabled(_converter->getOutput());
    _state->setKeypadLocked(_converter->getKeypad());
    _state->setProtectionState(_converter->getProtectionState() ? 1 : 0);
    _state->setCvMode(_converter->isCvMode());

    // Setpoints: only update if outside the post-write settle window.
    // Java equivalent: if (now >= voltagePendingUntil) state.setVoltageSet(...)
    if (millis() >= _voltagePendingUntil) {
      _state->setVoltageSet(_converter->getVoltageSet());
    }
    if (millis() >= _currentPendingUntil) {
      _state->setCurrentSet(_converter->getCurrentSet());
    }

    // Set device identity on first successful poll.
    if (_converter->getDevice() != nullptr) {
      _state->setDeviceName(_converter->getDevice());
    }
    if (_converter->getManufacturer() != nullptr) {
      _state->setManufacturer(_converter->getManufacturer());
    }

    // Online/offline hysteresis — Java equivalent: consecutiveSuccesses tracking.
    if (!_state->isDeviceOnline()) {
      if (consecutiveSuccesses >= MAX_CONSECUTIVE_SUCCESSES) {
        consecutiveSuccesses = 0;
        ESP_LOGI(TAG_DS, "Device communication restored - marking Online.");
        _state->setDeviceOnline(true);
      }
    } else {
      consecutiveSuccesses = 0;
    }

  } else {
    consecutiveSuccesses = 0;
    consecutiveFailures++;
    ESP_LOGW(TAG_DS, "Poll cycle failed (%d/%d).", consecutiveFailures, MAX_CONSECUTIVE_FAILURES);

    if (consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) {
      _state->setDeviceOnline(false);
      attemptReconnect();
      consecutiveFailures = 0;
    } else if (consecutiveFailures == 1) {
      // First failure (likely a timeout) → go Offline immediately.
      // Java equivalent: isTimeout → state.setDeviceOnline(false)
      _state->setDeviceOnline(false);
    }
  }
}

// ---- attemptReconnect ------------------------------------------------------

/**
 * Closes and reopens the serial transport after repeated poll failures.
 *
 * Java equivalent: DeviceService#attemptReconnect
 */
void DeviceService::attemptReconnect() {
  ESP_LOGW(TAG_DS, "Attempting serial port reconnect.");
  if (_converter->reconnect()) {
    ESP_LOGI(TAG_DS, "Serial port reconnect succeeded.");
  } else {
    ESP_LOGW(TAG_DS, "Serial port reconnect failed.");
  }
}

// ---- effectiveMaxVoltage ---------------------------------------------------

/**
 * Returns the effective maximum voltage setpoint, applying the BUCK dropout
 * ceiling and the operator configMaxVoltage cap.
 *
 * Java equivalent: DeviceService#effectiveMaxVoltage
 */
double DeviceService::effectiveMaxVoltage() const {
  double base = _state->getMaxVoltage();
  if (_state->getConverterTopology() == ConverterTopology::BUCK && _state->getVoltageIn() > 0.0) {
    double buckCeiling = _state->getVoltageIn() - BUCK_DROPOUT_V;
    if (buckCeiling < base) {
      base = buckCeiling;
    }
  }
  double cap = _state->getConfigMaxVoltage();
  return (cap > 0.0 && cap < base) ? cap : base;
}

// ---- effectiveMaxCurrent ---------------------------------------------------

/**
 * Returns the effective maximum current setpoint, applying the operator cap.
 *
 * Java equivalent: DeviceService#effectiveMaxCurrent
 */
double DeviceService::effectiveMaxCurrent() const {
  double base = _state->getMaxCurrent();
  double cap = _state->getConfigMaxCurrent();
  return (cap > 0.0 && cap < base) ? cap : base;
}

// ---- validateRange ---------------------------------------------------------

/**
 * Validates that value is within [min, max]. Logs a warning and returns false
 * if out of range.
 *
 * Java equivalent: DeviceService#validateRange (throws IllegalArgumentException)
 * Deviation: returns bool instead of throwing.
 */
bool DeviceService::validateRange(const char* name, double value, double min, double max) {
  if (value < min || value > max) {
    ESP_LOGW(TAG_DS, "%s out of range: %.3f (min=%.3f, max=%.3f)", name, value, min, max);
    return false;
  }
  return true;
}
