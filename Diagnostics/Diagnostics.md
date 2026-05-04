# ESP Diagnostics

## Output

Every 5 seconds the ESP is awaken by a hardware reset from the RTC.
It then outputs diagnostic data over the serial port:

```
ets Jul 29 2019 12:21:46

rst:0x5 (DEEPSLEEP_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:1
load:0x3fff0030,len:4876
ho 0 tail 12 room 4
load:0x40078000,len:16560
load:0x40080400,len:3500
entry 0x400805b4
Connecting to WiFi..... Connected!
---------------------------------------------------------
ESP ULTIMATE HARDWARE DIAGNOSTICS
Build Date:       Mar 22 2026 19:13:07
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
Heap Size:        332912 bytes
Free Heap:        235048 bytes
Min Free Heap:    231472 bytes (Watermark)
Max Alloc Heap:   110580 bytes
PSRAM:            Not Found / Disabled
Sketch Size:      908528 bytes
Free Sketch:      1310720 bytes

Partition Table Map:
 - nvs        | Data   | 0x009000 |    20 KB
 - otadata    | Data   | 0x00E000 |     8 KB
 - app0       | App    | 0x010000 |  1280 KB
 - app1       | App    | 0x150000 |  1280 KB
 - spiffs     | Data   | 0x290000 |  1408 KB
 - coredump   | Data   | 0x3F0000 |    64 KB

Internal Temp:    48.33 °C
Cycle Count:      907603406
SDK Version:      v5.5.2-729-g87912cd291
Efuse MAC:        046059F924F0
STA MAC Address:  F0:24:F9:59:60:04
AP MAC Address:   00:00:00:00:00:00
WLAN IP Address:  192.168.1.149
WLAN Subnet Mask: 255.255.255.0
WLAN Gateway:     192.168.1.1
WLAN RSSI:        -65 dBm
---------------------------------------------------------
Diagnostics Complete. Loop running...
Live Update -> Temp: 48.3 C, Free Heap: 234684

Entering Deep Sleep for 5 seconds...
```

Additionally, everything is also output on a second serial port (connect a USB TTL serial adapter):

```
ESP32 GND          → USB TTL (GND)
ESP32 Pin 17 (TX2) → USB TTL (RX)
ESP32 Pin 16 (RX2) → USB TTL (TX)
```
