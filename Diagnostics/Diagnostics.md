# ESP Diagnostics

## Overview

`Diagnostics.ino` is a single-file Arduino sketch that performs a comprehensive hardware interrogation of an ESP32 or ESP8266 board. On each run it connects to a Wi-Fi network, collects every hardware metric the SDK exposes, prints a structured report to the serial monitor, and then enters deep sleep for five seconds before repeating the cycle. It is useful for verifying a new board, comparing silicon revisions, or sanity-checking the runtime environment before deploying production firmware.

## Technical Architecture

The sketch is structured around the standard Arduino `setup()` / `loop()` lifecycle, deliberately keeping all diagnostic work in `setup()` so that each deep-sleep wakeup produces one complete, self-contained report.

**Startup sequence (`setup`)**

1. Both serial ports are initialised at 115200 baud. On ESP32, `Serial2` (pins 16/17) is opened alongside `Serial` so that output is mirrored to a secondary UART without requiring a USB connection.
2. The sketch attempts to join the configured Wi-Fi network (credentials are loaded from a git-ignored `Secrets.h` at compile time). Connection progress is printed dot-by-dot with a hard cap of 20 attempts (10 seconds).
3. The full diagnostic report is printed via `printDual()`, a thin `printf`-style wrapper that writes to both serial ports simultaneously.

**Reported metrics**

| Category | ESP32 | ESP8266 |
|---|---|---|
| Chip model, revision, core count | ✅ | — |
| CPU frequency, reset reason | ✅ | ✅ |
| Radio features (WiFi / BT / BLE / 802.15.4) | ✅ | — |
| Flash size, speed, mode | ✅ | ✅ |
| Heap size, free heap, watermark, max alloc | ✅ | free only |
| PSRAM (if present) | ✅ | — |
| Sketch size, free sketch space | ✅ | ✅ |
| Partition table map | ✅ | — |
| Internal temperature | ✅ (selected targets) | — |
| CPU cycle count, SDK version | ✅ | ✅ |
| EFuse MAC, STA MAC, AP MAC | ✅ | STA only |
| Wi-Fi IP, subnet, gateway, RSSI | ✅ | ✅ |

**Loop and sleep (`loop`)**

On ESP32, `loop()` prints a single live temperature / free-heap line, flushes both UARTs, and calls `esp_deep_sleep_start()`. The chip powers down completely and is woken after five seconds by the RTC timer, which re-runs `setup()` from scratch. On ESP8266, `loop()` simply delays five seconds — deep sleep from `loop()` would prevent the serial output from completing before power-down.

**Compile-time portability**

All ESP32-specific code is guarded by `#if defined(ESP32)` / `#elif defined(ESP8266)` preprocessor blocks. A hard `#error` fires for any unsupported target, keeping porting failures explicit rather than silent. Temperature sensor output is further gated on `CONFIG_IDF_TARGET_*` macros, since not all ESP32 variants expose that peripheral.

## Output

Every 5 seconds the ESP wakes via an RTC timer wakeup. This is not a full hardware reset — the chip re-runs `setup()` from the beginning, but `esp_reset_reason()` reports `DEEPSLEEP_RESET` to indicate the cause.
The sketch outputs diagnostic data over the primary serial port on each wake cycle:

### ESP32-WROOM-32

```
ets Jul 29 2019 12:21:46

rst:0x5 (DEEPSLEEP_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:1
load:0x3fff0030,len:4876
ho 0 tail 12 room 4
load:0x40078000,len:16532
load:0x40080400,len:3500
entry 0x400805b4
---------------------------------------------------------
Connecting to WiFi (Linksys14202)..... Connected!
ESP ULTIMATE HARDWARE DIAGNOSTICS
Build Date:       Sep 21 2026 12:01:24
---------------------------------------------------------
Chip Model:       ESP32-D0WD-V3
Chip Revision:    301
CPU Cores:        2
CPU Frequency:    240 MHz
Reset Reason:     Deep Sleep Wakeup
Features:         WiFi BT BLE External-Flash 
Flash Size:       4 MB
Flash Speed:      80 MHz
Flash Mode:       QIO
Heap Size:        331812 bytes
Free Heap:        230648 bytes
Min Free Heap:    227076 bytes (Watermark)
Max Alloc Heap:   110580 bytes
PSRAM:            Not Found / Disabled
Sketch Size:      891904 bytes
Free Sketch:      1310720 bytes

Partition Table Map:
 - nvs        | Data   | 0x009000 |    20 KB
 - otadata    | Data   | 0x00E000 |     8 KB
 - app0       | App    | 0x010000 |  1280 KB
 - app1       | App    | 0x150000 |  1280 KB
 - spiffs     | Data   | 0x290000 |  1408 KB
 - coredump   | Data   | 0x3F0000 |    64 KB

Internal Temp:    51.11 °C
Cycle Count:      908900923
SDK Version:      v5.5.5
Efuse MAC:        F0:24:F9:59:60:04
STA MAC Address:  F0:24:F9:59:60:04
AP MAC Address:   00:00:00:00:00:00
WLAN IP Address:  192.168.1.148
WLAN Subnet Mask: 255.255.255.0
WLAN Gateway:     192.168.1.1
WLAN RSSI:        -62 dBm
---------------------------------------------------------
Diagnostics Complete. Loop running...
Live Update -> Temp: 51.1 C, Free Heap: 230276

Entering Deep Sleep for 5 seconds...
```

### ESP32-C6-WROOM-1

```
ESP-ROM:esp32c6-20220919
Build:Sep 19 2022
rst:0x5 (SLEEP_WAKEUP),boot:0xc (SPI_FAST_FLASH_BOOT)
SPIWP:0xee
mode:DIO, clock div:2
load:0x40875730,len:0x1278
load:0x4086b910,len:0xc58
load:0x4086e610,len:0x31c0
entry 0x4086b910
---------------------------------------------------------
Connecting to WiFi (Linksys14202)... Connected!
ESP ULTIMATE HARDWARE DIAGNOSTICS
Build Date:       Sep 21 2026 12:07:11
---------------------------------------------------------
Chip Model:       ESP32-C6
Chip Revision:    1
CPU Cores:        1
CPU Frequency:    160 MHz
Reset Reason:     Deep Sleep Wakeup
Features:         WiFi BLE 802.15.4 External-Flash 
Flash Size:       16 MB
Flash Speed:      80 MHz
Flash Mode:       QIO
Heap Size:        402456 bytes
Free Heap:        312544 bytes
Min Free Heap:    307680 bytes (Watermark)
Max Alloc Heap:   294900 bytes
PSRAM:            Not Found / Disabled
Sketch Size:      990848 bytes
Free Sketch:      1310720 bytes

Partition Table Map:
 - nvs        | Data   | 0x009000 |    20 KB
 - otadata    | Data   | 0x00E000 |     8 KB
 - app0       | App    | 0x010000 |  1280 KB
 - app1       | App    | 0x150000 |  1280 KB
 - spiffs     | Data   | 0x290000 |  1408 KB
 - coredump   | Data   | 0x3F0000 |    64 KB

Internal Temp:    Not Supported
Cycle Count:      39473368
SDK Version:      v5.5.5
Efuse MAC:        40:4C:CA:FF:FE:5F
STA MAC Address:  40:4C:CA:5F:A7:38
AP MAC Address:   00:00:00:00:00:00
WLAN IP Address:  192.168.1.158
WLAN Subnet Mask: 255.255.255.0
WLAN Gateway:     192.168.1.1
WLAN RSSI:        -71 dBm
---------------------------------------------------------
Diagnostics Complete. Loop running...

Entering Deep Sleep for 5 seconds...
```

### CYB (ESP32-WROOM-32D)

```
ets Jul 29 2019 12:21:46

rst:0x5 (DEEPSLEEP_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:1
load:0x3fff0030,len:4876
ho 0 tail 12 room 4
load:0x40078000,len:16532
load:0x40080400,len:3500
entry 0x400805b4
□---------------------------------------------------------
Connecting to WiFi (Linksys14202)... Connected!
ESP ULTIMATE HARDWARE DIAGNOSTICS
Build Date:       Sep 21 2026 12:16:03
---------------------------------------------------------
Chip Model:       ESP32-D0WD-V3
Chip Revision:    300
CPU Cores:        2
CPU Frequency:    240 MHz
Reset Reason:     Deep Sleep Wakeup
Features:         WiFi BT BLE External-Flash 
Flash Size:       4 MB
Flash Speed:      80 MHz
Flash Mode:       QIO
Heap Size:        331812 bytes
Free Heap:        230744 bytes
Min Free Heap:    227180 bytes (Watermark)
Max Alloc Heap:   110580 bytes
PSRAM:            Not Found / Disabled
Sketch Size:      891904 bytes
Free Sketch:      1310720 bytes

Partition Table Map:
 - nvs        | Data   | 0x009000 |    20 KB
 - otadata    | Data   | 0x00E000 |     8 KB
 - app0       | App    | 0x010000 |  1280 KB
 - app1       | App    | 0x150000 |  1280 KB
 - spiffs     | Data   | 0x290000 |  1408 KB
 - coredump   | Data   | 0x3F0000 |    64 KB

Internal Temp:    53.33 °C
Cycle Count:      666583249
SDK Version:      v5.5.5
Efuse MAC:        3C:E9:0E:08:40:30
STA MAC Address:  3C:E9:0E:08:40:30
AP MAC Address:   00:00:00:00:00:00
WLAN IP Address:  192.168.1.183
WLAN Subnet Mask: 255.255.255.0
WLAN Gateway:     192.168.1.1
WLAN RSSI:        -56 dBm
---------------------------------------------------------
Diagnostics Complete. Loop running...
Live Update -> Temp: 53.3 C, Free Heap: 230368

Entering Deep Sleep for 5 seconds...
```

### Guiton ESP32-S3-4848S040

```
ESP-ROM:esp32s3-20210327
Build:Mar 27 2021
rst:0x5 (DSLEEP),boot:0x18 (SPI_FAST_FLASH_BOOT)
SPIWP:0xee
mode:DIO, clock div:1
load:0x3fce2820,len:0x10cc
load:0x403c8700,len:0xc2c
load:0x403cb700,len:0x30b0
entry 0x403c88b8
E (98) esp_core_dump_flash: Core dump data check failed:
Calculated checksum='e9a64fa7'
Image checksum='ff00ffff'
---------------------------------------------------------
Connecting to WiFi (Linksys14202)... Connected!
ESP ULTIMATE HARDWARE DIAGNOSTICS
Build Date:       Sep 21 2026 12:25:25
---------------------------------------------------------
Chip Model:       ESP32-S3
Chip Revision:    2
CPU Cores:        2
CPU Frequency:    240 MHz
Reset Reason:     Deep Sleep Wakeup
Features:         WiFi BLE External-Flash 
Flash Size:       16 MB
Flash Speed:      80 MHz
Flash Mode:       QIO
Heap Size:        356408 bytes
Free Heap:        258336 bytes
Min Free Heap:    254824 bytes (Watermark)
Max Alloc Heap:   212980 bytes
PSRAM:            Not Found / Disabled
Sketch Size:      881472 bytes
Free Sketch:      1310720 bytes

Partition Table Map:
 - nvs        | Data   | 0x009000 |    20 KB
 - otadata    | Data   | 0x00E000 |     8 KB
 - app0       | App    | 0x010000 |  1280 KB
 - app1       | App    | 0x150000 |  1280 KB
 - spiffs     | Data   | 0x290000 |  1408 KB
 - coredump   | Data   | 0x3F0000 |    64 KB

Internal Temp:    29.90 °C
Cycle Count:      662806409
SDK Version:      v5.5.5
Efuse MAC:        98:A3:16:E5:9B:E4
STA MAC Address:  98:A3:16:E5:9B:E4
AP MAC Address:   00:00:00:00:00:00
WLAN IP Address:  192.168.1.156
WLAN Subnet Mask: 255.255.255.0
WLAN Gateway:     192.168.1.1
WLAN RSSI:        -55 dBm
---------------------------------------------------------
Diagnostics Complete. Loop running...
Live Update -> Temp: 29.9 C, Free Heap: 257928

Entering Deep Sleep for 5 seconds...
```

## Tools

On ESP32, all output is mirrored to a second hardware UART (Serial2). Connect a USB-to-TTL serial adapter to tap this port without occupying the primary USB connection.
A lightweight terminal such as [Termite](https://www.compuphase.com/software_termite.htm) works well — select the additional **COM** port and set the speed to **115200 baud**:

```
ESP32 GND          → USB TTL (GND)
ESP32 Pin 17 (TX2) → USB TTL (RX)
ESP32 Pin 16 (RX2) → USB TTL (TX)
```
