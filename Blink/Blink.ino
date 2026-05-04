// ESP32/PI Pico
#define INTERNAL_ESP_PICO_LED 2

// Cheap Yellow Display
#define INTERNAL_LED_CYD_RED 4
#define INTERNAL_LED_CYD_GREEN 16
#define INTERNAL_LED_CYD_BLUE 17

// Arduino Nano
#define INTERNAL_ARDUINO_NANO_LED LED_BUILTIN

#define INTERNAL_LED 2

void setup() {
  pinMode(INTERNAL_LED, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(4, INPUT);
  Serial.begin(115200);
  digitalWrite(3, LOW);
}

void loop() {
  int interval = 1000;
  int intensity = 0;
  int button = 0;
  char buffer[128];
  do {
    digitalWrite(INTERNAL_LED, HIGH);
    //    analogWrite(3, 0);
    Serial.println("LED ON");
    delay(interval);
    digitalWrite(INTERNAL_LED, LOW);
    button = digitalRead(4);
    sprintf(buffer, "Button: %d", button);
    Serial.println(buffer);
    if (button == HIGH) {
      analogWrite(3, 255 - intensity);
    } else {
      analogWrite(3, intensity);
    }
    Serial.println("LED OFF");
    delay(interval);
    interval -= 50;
    if (interval < 100) {
      interval = 1000;
    }
    intensity += 5;
    if (intensity > 255) {
      intensity = 0;
    }
  } while (true);
}
