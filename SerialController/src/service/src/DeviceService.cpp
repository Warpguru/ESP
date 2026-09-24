#include "DeviceService.h"

#include <Arduino.h>
#include <freertos/task.h>

#include "../../../src/SerialController/src/LogBuffer.h"
#include "../../../src/SerialController/src/StatusLed.h"
#include "../../device/src/DeviceCatalogue.h"

/**
 * DeviceService.cpp - Service layer for device polling and validated setpoint writes.
 *
 * Java equivalent: com.serial.service.DeviceService
 */

/** Polling interval - Java equivalent: POLL_INTERVAL_MS = 1000. */
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

/**
 * Returns the string name of a ConverterTopology enum value for logging.
 * Java equivalent: ConverterTopology.name() (enum toString).
 */
static const char* topologyName(ConverterTopology topology) {
  switch (topology) {
    case ConverterTopology::BUCK:       return "BUCK";
    case ConverterTopology::BOOST:      return "BOOST";
    case ConverterTopology::BUCK_BOOST: return "BUCK_BOOST";
    default:                            return "UNKNOWN";
  }
}

// ---- loadLimits ------------------------------------------------------------

/**
 * Loads device capability limits from the compile-time catalogue and applies
 * them to ConverterState. Called once after construction, before begin().
 *
 * Looks up the device name string returned by the detected driver in the
 * CATALOGUE array. On a match, sets maxVoltage / minVoltage / maxCurrent /
 * minCurrent / maxPower / converterTopology on ConverterState. Also sets
 * deviceName and manufacturer from the catalogue entry so that ConverterState
 * is fully populated before the first poll cycle.
 *
 * If no entry is found a warning is logged and all limits remain at 0, which
 * prevents any setpoint write from being accepted - matching Java's behaviour
 * when the properties file is absent.
 *
 * Java equivalent: DeviceService#loadLimits()
 */
void DeviceService::loadLimits() {
  if (converter == nullptr) {
    Log_warn("No device detected - skipping limits load. All limits remain at 0.");
    return;
  }

  const char* deviceName = converter->getDevice();
  if (deviceName == nullptr) {
    Log_warn("Could not determine device name - skipping limits load.");
    return;
  }

  const DeviceLimits* limits = findDeviceLimits(deviceName);
  if (limits == nullptr) {
    Log_warn("No catalogue entry for device '%s'. All limits remain at 0.", deviceName);
    return;
  }

  state->setDeviceName(limits->name);
  state->setManufacturer(limits->manufacturer);
  state->setMaxVoltage(limits->maxVoltageVolts);
  state->setMinVoltage(limits->minVoltageVolts);
  state->setMaxCurrent(limits->maxCurrentAmperes);
  state->setMinCurrent(limits->minCurrentAmperes);
  state->setMaxPower(limits->maxPowerWatts);
  state->setConverterTopology(limits->topology);

  Log_info("Device limits loaded: %s %s | topology=%s V=[%.1f, %.1f] A=[%.1f, %.1f] P_max=%.1fW",
           limits->manufacturer, limits->name,
           topologyName(limits->topology),
           limits->minVoltageVolts, limits->maxVoltageVolts,
           limits->minCurrentAmperes, limits->maxCurrentAmperes,
           limits->maxPowerWatts);
}

// ---- Constructor -----------------------------------------------------------

DeviceService::DeviceService(ConverterState* state, DC2DCConverter* converter)
    : state(state), converter(converter) {
  mutex = xSemaphoreCreateMutex();
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
  Log_info("DeviceService polling task started on Core 1.");
}

// ---- isDeviceDetected ------------------------------------------------------

/**
 * Java equivalent: DeviceService#isDeviceDetected
 */
bool DeviceService::isDeviceDetected() const {
  return (converter != nullptr) && (converter->getDevice() != nullptr);
}

// ---- getState --------------------------------------------------------------

const ConverterState* DeviceService::getState() const {
  return state;
}

// ---- setVoltage ------------------------------------------------------------

/**
 * Sets the voltage setpoint with range validation and settle-window suppression.
 *
 * Java equivalent: DeviceService#setVoltage (synchronized)
 * Deviation: bool return instead of void/throws; mutex instead of synchronized.
 */
bool DeviceService::setVoltage(double volts) {
  if (converter == nullptr) {
    return false;
  }
  double maxV = effectiveMaxVoltage();
  if (!validateRange("Voltage", volts, state->getMinVoltage(), maxV)) {
    return false;
  }
  xSemaphoreTake(mutex, portMAX_DELAY);
  Log_info("Setting voltage to %.3f V", volts);
  bool ok = converter->setVoltage(volts);
  if (ok) {
    state->setVoltageSet(volts);
    voltagePendingUntil = millis() + SETPOINT_SETTLE_MS;
  }
  xSemaphoreGive(mutex);
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
  if (converter == nullptr) {
    return false;
  }
  double maxV = effectiveMaxVoltage();
  if (!validateRange("Voltage", volts, state->getMinVoltage(), maxV)) {
    return false;
  }
  xSemaphoreTake(mutex, portMAX_DELAY);
  Log_info("setVoltageVerified: writing %.3f V", volts);
  bool ok = converter->setVoltage(volts);
  if (!ok) {
    xSemaphoreGive(mutex);
    return false;
  }
  voltagePendingUntil = millis() + SETPOINT_SETTLE_MS;
  vTaskDelay(pdMS_TO_TICKS(VERIFIED_READBACK_DELAY_MS));
  double confirmed = converter->getVoltageSetVerified();
  if (fabs(confirmed - volts) > VERIFIED_TOLERANCE) {
    Log_debug("setVoltageVerified: first read-back %.3f, retrying", confirmed);
    vTaskDelay(pdMS_TO_TICKS(VERIFIED_READBACK_DELAY_MS));
    confirmed = converter->getVoltageSetVerified();
    if (fabs(confirmed - volts) > VERIFIED_TOLERANCE) {
      Log_warn("setVoltageVerified: device did not accept %.3f V (read back %.3f V)", volts, confirmed);
      outConflict = true;
      xSemaphoreGive(mutex);
      return false;
    }
  }
  Log_info("setVoltageVerified: confirmed %.3f V", confirmed);
  state->setVoltageSet(confirmed);
  confirmedOut = confirmed;
  xSemaphoreGive(mutex);
  return true;
}

// ---- setCurrent ------------------------------------------------------------

/**
 * Java equivalent: DeviceService#setCurrent (synchronized)
 */
bool DeviceService::setCurrent(double amperes) {
  if (converter == nullptr) {
    return false;
  }
  double maxI = effectiveMaxCurrent();
  if (!validateRange("Current", amperes, state->getMinCurrent(), maxI)) {
    return false;
  }
  xSemaphoreTake(mutex, portMAX_DELAY);
  Log_info("Setting current to %.3f A", amperes);
  bool ok = converter->setCurrent(amperes);
  if (ok) {
    state->setCurrentSet(amperes);
    currentPendingUntil = millis() + SETPOINT_SETTLE_MS;
  }
  xSemaphoreGive(mutex);
  return ok;
}

// ---- setCurrentVerified ----------------------------------------------------

/**
 * Java equivalent: DeviceService#setCurrentVerified (synchronized)
 */
bool DeviceService::setCurrentVerified(double amperes, double& confirmedOut, bool& outConflict) {
  outConflict = false;
  if (converter == nullptr) {
    return false;
  }
  double maxI = effectiveMaxCurrent();
  if (!validateRange("Current", amperes, state->getMinCurrent(), maxI)) {
    return false;
  }
  xSemaphoreTake(mutex, portMAX_DELAY);
  Log_info("setCurrentVerified: writing %.3f A", amperes);
  bool ok = converter->setCurrent(amperes);
  if (!ok) {
    xSemaphoreGive(mutex);
    return false;
  }
  currentPendingUntil = millis() + SETPOINT_SETTLE_MS;
  vTaskDelay(pdMS_TO_TICKS(VERIFIED_READBACK_DELAY_MS));
  double confirmed = converter->getCurrentSetVerified();
  if (fabs(confirmed - amperes) > VERIFIED_TOLERANCE) {
    Log_debug("setCurrentVerified: first read-back %.3f, retrying", confirmed);
    vTaskDelay(pdMS_TO_TICKS(VERIFIED_READBACK_DELAY_MS));
    confirmed = converter->getCurrentSetVerified();
    if (fabs(confirmed - amperes) > VERIFIED_TOLERANCE) {
      Log_warn("setCurrentVerified: device did not accept %.3f A (read back %.3f A)", amperes, confirmed);
      outConflict = true;
      xSemaphoreGive(mutex);
      return false;
    }
  }
  Log_info("setCurrentVerified: confirmed %.3f A", confirmed);
  state->setCurrentSet(confirmed);
  confirmedOut = confirmed;
  xSemaphoreGive(mutex);
  return true;
}

// ---- setVoltageCurrent -----------------------------------------------------

/**
 * Sets voltage and current atomically in a single 0x10 frame.
 *
 * Java equivalent: DeviceService#setMeasurements (synchronized)
 */
bool DeviceService::setVoltageCurrent(double volts, double amperes) {
  if (converter == nullptr) {
    return false;
  }
  if (!validateRange("Voltage", volts, state->getMinVoltage(), effectiveMaxVoltage())) {
    return false;
  }
  if (!validateRange("Current", amperes, state->getMinCurrent(), effectiveMaxCurrent())) {
    return false;
  }
  xSemaphoreTake(mutex, portMAX_DELAY);
  Log_info("Setting voltage %.3f V and current %.3f A (atomic)", volts, amperes);
  bool ok = converter->setVoltageCurrent(volts, amperes);
  if (ok) {
    state->setVoltageSet(volts);
    state->setCurrentSet(amperes);
    voltagePendingUntil = millis() + SETPOINT_SETTLE_MS;
    currentPendingUntil = millis() + SETPOINT_SETTLE_MS;
  }
  xSemaphoreGive(mutex);
  return ok;
}

// ---- setOutput -------------------------------------------------------------

/**
 * Java equivalent: DeviceService#setOutput (synchronized)
 */
bool DeviceService::setOutput(bool on) {
  if (converter == nullptr) {
    return false;
  }
  xSemaphoreTake(mutex, portMAX_DELAY);
  Log_info("Setting output to %s", on ? "ON" : "OFF");
  bool ok = converter->setOutput(on);
  if (ok) {
    state->setOutputEnabled(on);
  }
  xSemaphoreGive(mutex);
  return ok;
}

// ---- setKeypad -------------------------------------------------------------

/**
 * Java equivalent: DeviceService#setKeypad (synchronized)
 */
bool DeviceService::setKeypad(bool locked) {
  if (converter == nullptr) {
    return false;
  }
  xSemaphoreTake(mutex, portMAX_DELAY);
  Log_info("Setting keypad lock to %s", locked ? "LOCKED" : "UNLOCKED");
  bool ok = converter->setKeypad(locked);
  if (ok) {
    state->setKeypadLocked(locked);
  }
  xSemaphoreGive(mutex);
  return ok;
}

// ---- clearProtection -------------------------------------------------------

/**
 * Java equivalent: DeviceService#clearProtection (synchronized)
 */
bool DeviceService::clearProtection() {
  if (converter == nullptr) {
    return false;
  }
  xSemaphoreTake(mutex, portMAX_DELAY);
  Log_info("Clearing protection state.");
  bool ok = converter->setProtectionState(false);
  if (ok) {
    state->setProtectionState(0);
  }
  xSemaphoreGive(mutex);
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
 * Acquires mutex to prevent concurrent serial access from write operations.
 * Respects the post-write settle windows for voltageSet and currentSet.
 *
 * Java equivalent: DeviceService#poll (synchronized)
 */
void DeviceService::poll() {
  if (converter == nullptr) {
    return;
  }

  // Per-poll counters - static locals mirror the Java instance fields.
  // Java: private int consecutiveFailures / consecutiveSuccesses.
  static int consecutiveFailures = 0;
  static int consecutiveSuccesses = 0;

  xSemaphoreTake(mutex, portMAX_DELAY);
  bool ok = converter->pollAll();
  xSemaphoreGive(mutex);

  if (ok) {
    consecutiveSuccesses++;
    consecutiveFailures = 0;

    state->setVoltageOut(converter->getVoltage());
    state->setCurrentOut(converter->getCurrent());
    state->setPowerOut(converter->getPower());
    state->setVoltageIn(converter->getInputVoltage());
    state->setTemperatureCelsius(converter->getTemperatureCelsius());
    state->setOutputEnabled(converter->getOutput());
    state->setKeypadLocked(converter->getKeypad());
    state->setProtectionState(converter->getProtectionState() ? 1 : 0);
    state->setCvMode(converter->isCvMode());

    // Setpoints: only update if outside the post-write settle window.
    // Java equivalent: if (now >= voltagePendingUntil) state.setVoltageSet(...)
    if (millis() >= voltagePendingUntil) {
      state->setVoltageSet(converter->getVoltageSet());
    }
    if (millis() >= currentPendingUntil) {
      state->setCurrentSet(converter->getCurrentSet());
    }

    // Set device identity on first successful poll.
    if (converter->getDevice() != nullptr) {
      state->setDeviceName(converter->getDevice());
    }
    if (converter->getManufacturer() != nullptr) {
      state->setManufacturer(converter->getManufacturer());
    }

    // Online/offline hysteresis - Java equivalent: consecutiveSuccesses tracking.
    if (!state->isDeviceOnline()) {
      if (consecutiveSuccesses >= MAX_CONSECUTIVE_SUCCESSES) {
        consecutiveSuccesses = 0;
        Log_info("Device communication restored - marking Online.");
        state->setDeviceOnline(true);
        statusLed.setState(LedState::READY);
      }
    } else {
      consecutiveSuccesses = 0;
    }

  } else {
    consecutiveSuccesses = 0;
    consecutiveFailures++;
    Log_warn("Poll cycle failed (%d/%d).", consecutiveFailures, MAX_CONSECUTIVE_FAILURES);

    if (consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) {
      state->setDeviceOnline(false);
      statusLed.setState(LedState::NO_DEVICE);
      attemptReconnect();
      consecutiveFailures = 0;
    } else if (consecutiveFailures == 1) {
      // First failure (likely a timeout) → go Offline immediately.
      // Java equivalent: isTimeout → state.setDeviceOnline(false)
      state->setDeviceOnline(false);
      statusLed.setState(LedState::NO_DEVICE);
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
  Log_warn("Attempting serial port reconnect.");
  if (converter->reconnect()) {
    Log_info("Serial port reconnect succeeded.");
  } else {
    Log_warn("Serial port reconnect failed.");
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
  double base = state->getMaxVoltage();
  if (state->getConverterTopology() == ConverterTopology::BUCK && state->getVoltageIn() > 0.0) {
    double buckCeiling = state->getVoltageIn() - BUCK_DROPOUT_V;
    if (buckCeiling < base) {
      base = buckCeiling;
    }
  }
  double cap = state->getConfigMaxVoltage();
  return (cap > 0.0 && cap < base) ? cap : base;
}

// ---- effectiveMaxCurrent ---------------------------------------------------

/**
 * Returns the effective maximum current setpoint, applying the operator cap.
 *
 * Java equivalent: DeviceService#effectiveMaxCurrent
 */
double DeviceService::effectiveMaxCurrent() const {
  double base = state->getMaxCurrent();
  double cap = state->getConfigMaxCurrent();
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
    Log_warn("%s out of range: %.3f (min=%.3f, max=%.3f)", name, value, min, max);
    return false;
  }
  return true;
}
