/*
 * Diagnostics.ino
 *
 * This program performs a deep-dive query of the hardware capabilities 
 * of an ESP32 or ESP8266 board.
 */

#if defined(ESP32)
  #include <WiFi.h>
  #include <esp_system.h>
  #include <esp_chip_info.h>
  #include <esp_partition.h>
//  #include <esp_ota_ops.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#else
  #error "This sketch is designed for ESP32 or ESP8266 boards."
#endif

// ESP32/PI Pico
#define INTERNAL_ESP_PICO_LED 1
#define INTERNAL_LED INTERNAL_ESP_PICO_LED

// WLAN Credentials
#if __has_include("Secrets.h")
  #include "Secrets.h"
#else
  #error "Secrets.h not found. Copy Diagnostics/Secrets.h.template to Diagnostics/Secrets.h and fill in your credentials."
#endif

/* 
 * ADDITIONAL SERIAL PORT (TTL Adapter):
 * GND: Connect to ESP32 GND
 * TX:  Connect to ESP32 Pin 17 (TX2)
 * RX:  Connect to ESP32 Pin 16 (RX2)
 */

void printDual(const char* format, ...) {
  char buf[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  Serial.print(buf);
  #if defined(ESP32)
    Serial2.print(buf);
  #endif
}

void printSeparator() {
  printDual("---------------------------------------------------------\r\n");
}

#if defined(ESP32)
// Also defined in SerialController/ESPInfo.h — Arduino sketch isolation prevents sharing
const char* getResetReasonString(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_UNKNOWN:   return "Unknown";
    case ESP_RST_POWERON:   return "Power On";
    case ESP_RST_EXT:       return "External Pin";
    case ESP_RST_SW:        return "Software";
    case ESP_RST_PANIC:     return "Exception/Panic";
    case ESP_RST_INT_WDT:   return "Interrupt Watchdog";
    case ESP_RST_TASK_WDT:  return "Task Watchdog";
    case ESP_RST_WDT:       return "Other Watchdog";
    case ESP_RST_DEEPSLEEP: return "Deep Sleep Wakeup";
    case ESP_RST_BROWNOUT:  return "Brownout";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "Other";
  }
}

// Also defined in SerialController/ESPInfo.h — Arduino sketch isolation prevents sharing
const char* getFlashModeString(uint32_t mode) {
  switch (mode) {
    case 0: return "QIO";
    case 1: return "QOUT";
    case 2: return "DIO";
    case 3: return "DOUT";
    default: return "Unknown";
  }
}
#endif

void setup() {
  pinMode(INTERNAL_LED, OUTPUT);
  digitalWrite(INTERNAL_LED, LOW);
  Serial.begin(115200);
  #if defined(ESP32)
    Serial2.begin(115200, SERIAL_8N1, 16, 17); // RX:16, TX:17
  #endif
  // NOTE: On ESP32/ESP8266, Serial (UART0) is always ready — this guard only
  // has effect on boards with USB-CDC serial (e.g. Leonardo, RP2040, ESP32-S2/S3
  // native USB). It is kept here for Arduino compatibility but is a no-op on
  // classic ESP32 and ESP8266.
  #if !defined(ESP32) && !defined(ESP8266)
    while(!Serial) delay(10);
  #endif
  delay(1000);

  // Initialize WiFi and connect
  WiFi.mode(WIFI_STA);
  WiFi.begin(WLAN_SSID, WLAN_PASSWORD);
  
  printSeparator();
  printDual("Connecting to WiFi (%s)", WLAN_SSID);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    printDual(".");
    attempts++;
  }
  printDual(WiFi.status() == WL_CONNECTED ? " Connected!\r\n" : " Failed!\r\n");

  printDual("ESP ULTIMATE HARDWARE DIAGNOSTICS\r\n");
  printDual("Build Date:       %s %s\r\n", __DATE__, __TIME__);
  printSeparator();

#if defined(ESP32)
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);

  printDual("Chip Model:       %s\r\n", ESP.getChipModel());
  printDual("Chip Revision:    %d\r\n", ESP.getChipRevision());
  printDual("CPU Cores:        %d\r\n", chip_info.cores);
  printDual("CPU Frequency:    %d MHz\r\n", ESP.getCpuFreqMHz());
  printDual("Reset Reason:     %s\r\n", getResetReasonString(esp_reset_reason()));
  
  printDual("Features:         %s%s%s%s%s\r\n",
    (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi " : "",
    (chip_info.features & CHIP_FEATURE_BT) ? "BT " : "",
    (chip_info.features & CHIP_FEATURE_BLE) ? "BLE " : "",
    (chip_info.features & CHIP_FEATURE_IEEE802154) ? "802.15.4 " : "",
    (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "Embedded-Flash " : "External-Flash ");

  uint32_t flash_size = ESP.getFlashChipSize();
  printDual("Flash Size:       %d MB\r\n", flash_size / (1024 * 1024));
  printDual("Flash Speed:      %d MHz\r\n", ESP.getFlashChipSpeed() / 1000000);
  printDual("Flash Mode:       %s\r\n", getFlashModeString(ESP.getFlashChipMode()));
  
  printDual("Heap Size:        %d bytes\r\n", ESP.getHeapSize());
  printDual("Free Heap:        %d bytes\r\n", ESP.getFreeHeap());
  printDual("Min Free Heap:    %d bytes (Watermark)\r\n", ESP.getMinFreeHeap());
  printDual("Max Alloc Heap:   %d bytes\r\n", ESP.getMaxAllocHeap());

  if (psramFound()) {
    printDual("PSRAM Size:       %d bytes\r\n", ESP.getPsramSize());
    printDual("Free PSRAM:       %d bytes\r\n", ESP.getFreePsram());
  } else {
    printDual("PSRAM:            Not Found / Disabled\r\n");
  }

  printDual("Sketch Size:      %d bytes\r\n", ESP.getSketchSize());
  printDual("Free Sketch:      %d bytes\r\n", ESP.getFreeSketchSpace());
  
  // Partition Table Info
  // esp_partition_next() advances the iterator in-place; it does NOT allocate a
  // new object. Releasing mid-loop frees the live iterator and corrupts the heap.
  // Release once after the loop — esp_partition_iterator_release(NULL) is a
  // documented no-op when the loop exhausts the iterator naturally.
  printDual("\r\nPartition Table Map:\r\n");
  esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
  while (it != NULL) {
    const esp_partition_t *p = esp_partition_get(it);
    printDual(" - %-10s | %-6s | 0x%06X | %5d KB\r\n",
      p->label, (p->type == 0 ? "App" : "Data"), (unsigned int)p->address, (int)p->size/1024);
    it = esp_partition_next(it);
  }
  esp_partition_iterator_release(it);

  // Internal Sensors
  printDual("\r\nInternal Temp:    ");
  #if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
    printDual("%.2f °C\r\n", temperatureRead());
  #else
    printDual("Not Supported\r\n");
  #endif

  printDual("Cycle Count:      %u\r\n", ESP.getCycleCount());
  printDual("SDK Version:      %s\r\n", ESP.getSdkVersion());
  
  // MAC Addresses (Physical Fuses)
  // getEfuseMac() returns the MAC stored in fuses as a 64-bit little-endian value,
  // so the bytes must be swapped to display in normal MSB-first MAC notation.
  uint64_t chipid = ESP.getEfuseMac();
  printDual("Efuse MAC:        %02X:%02X:%02X:%02X:%02X:%02X\r\n",
    (uint8_t)(chipid),       (uint8_t)(chipid >> 8),  (uint8_t)(chipid >> 16),
    (uint8_t)(chipid >> 24), (uint8_t)(chipid >> 32), (uint8_t)(chipid >> 40));
  printDual("STA MAC Address:  %s\r\n", WiFi.macAddress().c_str());
  // AP MAC is only valid after WiFi.softAP() is called; shown here for completeness.
  printDual("AP MAC Address:   %s\r\n", WiFi.softAPmacAddress().c_str());

  if (WiFi.status() == WL_CONNECTED) {
    printDual("WLAN IP Address:  %s\r\n", WiFi.localIP().toString().c_str());
    printDual("WLAN Subnet Mask: %s\r\n", WiFi.subnetMask().toString().c_str());
    printDual("WLAN Gateway:     %s\r\n", WiFi.gatewayIP().toString().c_str());
    printDual("WLAN RSSI:        %d dBm\r\n", WiFi.RSSI());
  }

#elif defined(ESP8266)
  // Note: ESP8266 does not expose total heap size, heap watermark, max alloc,
  // PSRAM, chip model/revision, EFuse MAC, partition table, or temperatureRead().
  // It offers getVcc() (ADC supply voltage) instead of a temperature sensor.
  printDual("Chip ID:          %08X\r\n", ESP.getChipId());
  printDual("CPU Frequency:    %d MHz\r\n", ESP.getCpuFreqMHz());
  printDual("Reset Reason:     %s\r\n", ESP.getResetReason().c_str());
  printDual("Flash Size:       %d bytes\r\n", ESP.getFlashChipSize());
  printDual("Flash Speed:      %d MHz\r\n", ESP.getFlashChipSpeed() / 1000000);
  printDual("Flash Mode:       %d\r\n", (int)ESP.getFlashChipMode());
  printDual("Free Heap:        %d bytes\r\n", ESP.getFreeHeap());
  printDual("Sketch Size:      %d bytes\r\n", ESP.getSketchSize());
  printDual("Free Sketch:      %d bytes\r\n", ESP.getFreeSketchSpace());
  printDual("Cycle Count:      %u\r\n", ESP.getCycleCount());
  printDual("SDK Version:      %s\r\n", ESP.getSdkVersion());
  printDual("MAC Address:      %s\r\n", WiFi.macAddress().c_str());
  if (WiFi.status() == WL_CONNECTED) {
    printDual("WLAN IP Address:  %s\r\n", WiFi.localIP().toString().c_str());
    printDual("WLAN Subnet Mask: %s\r\n", WiFi.subnetMask().toString().c_str());
    printDual("WLAN Gateway:     %s\r\n", WiFi.gatewayIP().toString().c_str());
    printDual("WLAN RSSI:        %d dBm\r\n", WiFi.RSSI());
  }
#endif

  printSeparator();
  printDual("Diagnostics Complete. Loop running...\r\n");
}

void loop() {
// Only print live temperature on the same targets that support it in setup().
// The outer ESP32 guard is intentionally NOT added here — doing so would
// silently skip the line on ESP32 variants that do not define CONFIG_IDF_TARGET_*,
// whereas the inner guard already covers exactly the right chips.
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
  printDual("Live Update -> Temp: %.1f C, Free Heap: %d\r\n",
    temperatureRead(), ESP.getFreeHeap());
#endif
#if defined(ESP32)
  printDual("\nEntering Deep Sleep for 5 seconds...\r\n");
  Serial.flush(); 
  Serial2.flush();
  digitalWrite(INTERNAL_LED, HIGH);
  esp_sleep_enable_timer_wakeup(5 * 1000000); 
  esp_deep_sleep_start();
#else
  delay(5000);
#endif
}
