#include "StatusLed.h"

#include <Arduino.h>

/**
 * StatusLed.cpp - Sole owner of the onboard status LED (GPIO 2).
 *
 * The led task runs permanently on whichever core FreeRTOS schedules it.
 * It blocks on ulTaskNotifyTake() in BOOTING and READY states (zero CPU),
 * uses vTaskDelay() in NO_DEVICE (yields for 1 s between toggles), and
 * loops synchronously in FAULT (SOS pattern) until setState() is called.
 *
 * No Java equivalent — ESP32-specific hardware feedback facility.
 */

// Global singleton instance.
StatusLed statusLed;

// ---- blinkSOS --------------------------------------------------------------

/**
 * Drives one full SOS cycle (... --- ...) on the given pin and returns.
 * Called repeatedly by ledTask while in FAULT state.
 */
void StatusLed::blinkSOS(int pin) {
  const int dot = 150;
  const int dash = 450;
  const int gap = 150;
  const int word = 700;

  // Three dots
  for (int i = 0; i < 3; i++) {
    digitalWrite(pin, HIGH);
    delay(dot);
    digitalWrite(pin, LOW);
    delay(gap);
  }
  delay(word);
  // Three dashes
  for (int i = 0; i < 3; i++) {
    digitalWrite(pin, HIGH);
    delay(dash);
    digitalWrite(pin, LOW);
    delay(gap);
  }
  delay(word);
  // Three dots
  for (int i = 0; i < 3; i++) {
    digitalWrite(pin, HIGH);
    delay(dot);
    digitalWrite(pin, LOW);
    delay(gap);
  }
  delay(2000);
}

// ---- ledTask ---------------------------------------------------------------

/**
 * Permanent FreeRTOS task — sole owner of STATUS_LED_PIN.
 *
 * BOOTING / READY : sets LED then blocks indefinitely on task notification.
 *                   setState() sends the notification to wake immediately.
 * NO_DEVICE       : toggles LED every 1 s via vTaskDelay (yields between
 * toggles). setState() interrupts the delay via task notification. FAULT : runs
 * one SOS cycle then checks for a state change. Loops forever until setState()
 * is called with another state.
 */
void StatusLed::ledTask(void *param) {
  StatusLed *self = static_cast<StatusLed *>(param);

  for (;;) {
    switch (self->ledState) {

    case LedState::BOOTING:
      // Set LED and block indefinitely. FreeRTOS removes this task from the
      // scheduler ready list — zero CPU consumed until setState() notifies.
      digitalWrite(STATUS_LED_PIN, HIGH);
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      break;

    case LedState::READY:
      // Set LED and block indefinitely — same mechanism as BOOTING.
      digitalWrite(STATUS_LED_PIN, LOW);
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      break;

    case LedState::NO_DEVICE:
      // Toggle LED then block for up to 1 s. Yields the core for the full
      // 1 s interval. setState() wakes it early via xTaskNotifyGive.
      digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
      break;

    case LedState::FAULT:
      // One SOS cycle (~3.5 s). Each delay() call yields via vTaskDelay
      // internally. Loops back to check ledState after each cycle so a
      // future setState(READY) can recover without waiting forever.
      blinkSOS(STATUS_LED_PIN);
      break;
    }
  }
}

// ---- begin -----------------------------------------------------------------

void StatusLed::begin() {
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, HIGH); // ON immediately while task starts
  xTaskCreate(ledTask, "StatusLed", 1024, this, 1, &taskHandle);
}

// ---- setState --------------------------------------------------------------

void StatusLed::setState(LedState state) {
  ledState = state;
  if (taskHandle != nullptr) {
    xTaskNotifyGive(taskHandle);
  }
}
