/*
 * Threads.ino
 * ESP32 Dual Core Threading Demo with OOP Encapsulation
 * 
 * Demonstrates using a C++ Class to encapsulate thread-safety logic.
 */

// --- Configuration Constants ---
const uint32_t SERIAL_BAUD      = 115200;
const uint8_t  LED_PIN          = 21; 
const uint8_t  BUTTON_PIN       = 0;

const uint32_t TASK_STACK_SIZE  = 10000;
const uint32_t TASK_PRIO        = 1;
const BaseType_t CORE_SERIAL    = 0;
const BaseType_t CORE_LED       = 1;

const uint32_t INTERVAL_SERIAL  = 1000; 
const uint32_t INTERVAL_BLINK   = 500;  
const uint32_t INTERVAL_PROMPT  = 500;  

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

    // Returns a snapshot of the counts
    void getCounts(uint32_t &c0, uint32_t &c1) {
        if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
            c0 = core0Iterations;
            c1 = core1Iterations;
            xSemaphoreGive(mutex);
        }
    }
};

// --- Global State ---
volatile bool isLedActive = true;
SafeStats safeStats; // The class instance handles its own mutex internally

TaskHandle_t LedTaskHandle = NULL;
TaskHandle_t SerialTaskHandle = NULL;
SemaphoreHandle_t SerialMutex = NULL;

// --- Function Prototypes ---
void ledTaskCode(void * parameter);
void serialTaskCode(void * parameter);

void setup() {
  Serial.begin(SERIAL_BAUD);
  while (!Serial) { ; }

  SerialMutex = xSemaphoreCreateMutex();

  xSemaphoreTake(SerialMutex, portMAX_DELAY);
  Serial.println("\n--- Dual Core Threading: OOP Class Demo ---");
  xSemaphoreGive(SerialMutex);
  
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

  xSemaphoreTake(SerialMutex, portMAX_DELAY);
  Serial.println("System initialized. Using SafeStats class.");
  xSemaphoreGive(SerialMutex);
}

void loop() {
  static bool lastButtonState = HIGH;
  bool currentButtonState = digitalRead(BUTTON_PIN);

  if (lastButtonState == HIGH && currentButtonState == LOW) {
    isLedActive = !isLedActive;
    
    if (xSemaphoreTake(SerialMutex, (TickType_t) 10) == pdTRUE) {
      Serial.printf(">> LED Activity toggled to: %s\n", isLedActive ? "ON" : "OFF");
      xSemaphoreGive(SerialMutex);
    }
    delay(200); 
  }

  lastButtonState = currentButtonState;
  delay(20); 
}

// Thread 2 (Core 0): Serial Console Printer
void serialTaskCode(void * parameter) {
  for (;;) {
    safeStats.incCore0(); // Thread-safe increment handled by the class

    uint32_t c0, c1;
    safeStats.getCounts(c0, c1); // Thread-safe read handled by the class

    if (xSemaphoreTake(SerialMutex, portMAX_DELAY) == pdTRUE) {
      Serial.printf("[Core 0] Stats -> C0: %u | C1: %u\n", c0, c1);
      xSemaphoreGive(SerialMutex);
    }
    
    delay(INTERVAL_SERIAL);
  }
}

// Thread 1 (Core 1): LED Blinker
void ledTaskCode(void * parameter) {
  bool wasActive = true;

  for (;;) {
    safeStats.incCore1(); // Thread-safe increment handled by the class

    if (isLedActive) {
      wasActive = true;
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
      
      if (xSemaphoreTake(SerialMutex, (TickType_t) 10) == pdTRUE) {
        Serial.println("[Core 1] Waiting for activation...");
        xSemaphoreGive(SerialMutex);
      }
      delay(INTERVAL_PROMPT);
    }
  }
}
