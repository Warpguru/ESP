/*
 * Threads.ino
 * ESP32 Dual Core Threading Demo with OOP Encapsulation
 *
 * Demonstrates using a C++ Class to encapsulate thread-safety logic.
 */

#if !defined(ESP32)
  #error "This sketch requires an ESP32 board."
#endif
#if CONFIG_FREERTOS_UNICORE
  #error "This sketch requires a dual-core ESP32 (ESP32, S3). Single-core targets (C3, C6, S2, H2) are not supported."
#endif

#include <atomic>

// --- Configuration Constants ---
const uint32_t SERIAL_BAUD      = 115200;
const uint8_t  LED_PIN          = 21; 
const uint8_t  BUTTON_PIN       = 0;

// 10000 bytes: default Arduino loopTask is 8192; extra headroom covers printf
// formatting buffers and the SafeStats mutex call chain on both tasks.
const uint32_t TASK_STACK_SIZE  = 10000;
const uint32_t TASK_PRIO        = 1;
const BaseType_t CORE_SERIAL    = 0;
const BaseType_t CORE_LED       = 1;

const uint32_t INTERVAL_SERIAL  = 1000; // ms between serial stats prints
const uint32_t INTERVAL_BLINK   = 500;  // ms per LED half-period (on or off)
const uint32_t INTERVAL_IDLE    = 500;  // ms between "waiting" prints when LED is off
                                        // (equal to INTERVAL_BLINK today, but independent)

// --- Encapsulated Thread-Safe Statistics Class ---
class SafeStats {
private:
    uint32_t core0Iterations = 0;
    uint32_t core1Iterations = 0;
    SemaphoreHandle_t mutex;

public:
    SafeStats() {
        // Create the mutex during object construction
        mutex = xSemaphoreCreateMutex();
    }

    void incCore0() {
        if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
            core0Iterations++;
            xSemaphoreGive(mutex);
        }
    }

    void incCore1() {
        if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
            core1Iterations++;
            xSemaphoreGive(mutex);
        }
    }

    // Returns a snapshot of both counts (single lock acquisition).
    void getCounts(uint32_t &c0, uint32_t &c1) {
        if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
            c0 = core0Iterations;
            c1 = core1Iterations;
            xSemaphoreGive(mutex);
        }
    }

    // Increments core0 counter and returns a consistent snapshot of both
    // counters in one lock acquisition, avoiding a TOCTOU gap.
    void incCore0AndGetCounts(uint32_t &c0, uint32_t &c1) {
        if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
            core0Iterations++;
            c0 = core0Iterations;
            c1 = core1Iterations;
            xSemaphoreGive(mutex);
        }
    }
};

// --- Global State ---
// isLedActive is written from the Arduino main task (loop) and read from
// ledTaskCode (Core 1) and serialTaskCode (Core 0). On a dual-core SMP system
// volatile alone does not guarantee cross-core visibility; use atomic instead.
std::atomic<bool> isLedActive{true};
SafeStats safeStats; // The class instance handles its own mutex internally

TaskHandle_t LedTaskHandle = NULL;
TaskHandle_t SerialTaskHandle = NULL;
SemaphoreHandle_t SerialMutex = NULL;

// --- Function Prototypes ---
void ledTaskCode(void * parameter);
void serialTaskCode(void * parameter);

void setup() {
  Serial.begin(SERIAL_BAUD);
  // On ESP32 UART0 is always ready; this guard only matters for USB-CDC targets.
  #if !defined(ESP32)
    while (!Serial) { ; }
  #endif

  SerialMutex = xSemaphoreCreateMutex();
  if (SerialMutex == NULL) {
    // If heap is exhausted at startup there is nothing safe to do.
    esp_restart();
  }

  // No other tasks exist yet — no mutex needed for this print.
  Serial.println("\r\n--- Dual Core Threading: OOP Class Demo ---");
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Start Serial Task on Core 0
  xTaskCreatePinnedToCore(
    serialTaskCode,
    "Serial_Thread",
    TASK_STACK_SIZE,
    NULL,
    TASK_PRIO,
    &SerialTaskHandle,
    CORE_SERIAL);

  delay(100);

  // Start LED Task on Core 1
  xTaskCreatePinnedToCore(
    ledTaskCode,
    "LED_Thread",
    TASK_STACK_SIZE,
    NULL,
    TASK_PRIO,
    &LedTaskHandle,
    CORE_LED);

  if (xSemaphoreTake(SerialMutex, portMAX_DELAY) == pdTRUE) {
    Serial.println("System initialized. Using SafeStats class.");
    xSemaphoreGive(SerialMutex);
  }
}

void loop() {
  static bool lastButtonState = HIGH;
  bool currentButtonState = digitalRead(BUTTON_PIN);

  if (lastButtonState == HIGH && currentButtonState == LOW) {
    isLedActive = !isLedActive;
    
    // Short timeout: loop() must return promptly to keep button polling
    // responsive. If the serial port is busy, skip this print rather than stall.
    if (xSemaphoreTake(SerialMutex, (TickType_t) 10) == pdTRUE) {
      Serial.printf("[loop] LED Activity toggled to: %s\r\n", isLedActive ? "ON" : "OFF");
      xSemaphoreGive(SerialMutex);
    }
    delay(200); 
  }

  lastButtonState = currentButtonState;
  delay(20); 
}

// Task 1 (Core 0): Serial Console Printer
void serialTaskCode(void * parameter) {
  for (;;) {
    uint32_t c0, c1;
    safeStats.incCore0AndGetCounts(c0, c1); // single lock: increment + consistent snapshot

    // portMAX_DELAY: printing IS the job of this task — blocking until the
    // mutex is available is correct; there is nothing else time-sensitive here.
    if (xSemaphoreTake(SerialMutex, portMAX_DELAY) == pdTRUE) {
      Serial.printf("[Core 0] Stats -> C0: %u | C1: %u\r\n", c0, c1);
      xSemaphoreGive(SerialMutex);
    }
    
    delay(INTERVAL_SERIAL);
  }
}

// Task 2 (Core 1): LED Blinker
void ledTaskCode(void * parameter) {
  bool wasActive = true;

  for (;;) {
    safeStats.incCore1(); // Thread-safe increment handled by the class

    if (isLedActive) {
      wasActive = true;
      // Short timeout: printing is incidental — the LED timing must not stall
      // waiting for Core 0 to finish its stats print. Skip if mutex is busy.
      if (xSemaphoreTake(SerialMutex, (TickType_t) 10) == pdTRUE) {
        Serial.println("[Core 1] LED Blinking...");
        xSemaphoreGive(SerialMutex);
      }
      digitalWrite(LED_PIN, HIGH);
      delay(INTERVAL_BLINK);
      digitalWrite(LED_PIN, LOW);
      delay(INTERVAL_BLINK);
    } else {
      if (wasActive) {
        digitalWrite(LED_PIN, LOW);
        wasActive = false;
      }
      
      // Short timeout: same rationale as above — keep the idle loop responsive.
      if (xSemaphoreTake(SerialMutex, (TickType_t) 10) == pdTRUE) {
        Serial.println("[Core 1] Waiting for LED activation by Boot button...");
        xSemaphoreGive(SerialMutex);
      }
      delay(INTERVAL_IDLE);
    }
  }
  
}
