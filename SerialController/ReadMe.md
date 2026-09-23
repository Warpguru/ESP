# SerialController

A proof-of-concept ESP32 sketch that controls a Riden bench power supply over
Modbus RTU (RS-232 via UART2) and exposes a REST API over WiFi.

## Architecture

- **`SerialController.ino`** — Main application logic, `setup()` / `loop()`, background polling task
- **`ModBus.ino`** — Modbus RTU protocol implementation; owns `Serial2` exclusively via a dedicated FreeRTOS task and a request/response queue
- **`Server.ino`** — WiFiManager setup, `AsyncWebServer` route registration, HTTP handlers, WebSocket server
- **`ESPInfo.h`** — ESP32 hardware diagnostics helper (chip, flash, memory, sketch, partitions, system, network)

## Libraries

The following libraries must be installed. In the Arduino IDE use **Sketch → Include Library → Manage Libraries…** and search for the exact names below.

| Library | Author | Arduino IDE search name | Version |
|---|---|---|---|
| WiFiManager | tzapu | `WiFiManager` | ≥ 2.0.17 |
| ArduinoJson | Benoit Blanchon | `ArduinoJson` | ≥ 7.4.3 |
| ESPAsyncWebServer | ESP32Async | `ESPAsyncWebServer` | ≥ 3.12.1 |
| AsyncTCP | ESP32Async | `AsyncTCP` | ≥ 3.5.0 |

> **Note:** `AsyncTCP` is a required dependency of `ESPAsyncWebServer` — install both.
> The built-in `WebServer` library is **not** used; all HTTP and WebSocket traffic is
> handled by `ESPAsyncWebServer` on port 80.

For PlatformIO the `platformio.ini` `lib_deps` section lists all dependencies with pinned versions.

## REST API

| Method | Endpoint | Description |
|---|---|---|
| GET | `/` | HTML landing page with API documentation |
| GET | `/voltage` | Read current output voltage from Riden |
| POST | `/setVoltage?v=5.0` | Set target output voltage |
| GET | `/status` | ESP32 hardware and WiFi diagnostics (JSON) |
| GET | `/reset` | Clear saved WiFi credentials and reboot into configuration mode |

## Hardware

### Riden RS-232 (UART2)

| Signal | ESP32 pin | Wire colour (Riden 4-pin header) |
|---|---|---|
| UART2 RX (receive from Riden) | GPIO 16 | Green (TxD on Riden) |
| UART2 TX (transmit to Riden) | GPIO 17 | White (RxD on Riden) |
| GND | GND | Black |
| VCC | **NC — do not connect** | Red |

> The Riden TTL header is 3.3 V. GPIO 16/17 are the default `Serial2` pins on
> the ESP32-WROOM-32. No level shifter is required.

### Fault indicator

| Signal | ESP32 pin |
|---|---|
| Fault indicator LED (SOS blink on WiFi failure) | GPIO 2 (onboard LED) |

### Hardware serial console (battery-powered operation)

When the ESP32 runs on a battery pack there is no USB connection and therefore
no access to the Serial Monitor. All `ESP_LOG*` messages are still transmitted
on **UART0**, which is also the USB-serial port. To read them wirelessly-free:

1. Connect a **3.3 V USB-to-UART adapter** (CP2102, CH340, FT232RL, etc.) to a
   laptop. **Do not use a 5 V adapter — GPIO 1/3 are not 5 V tolerant.**
2. Wire it to the ESP32 as follows:

| Signal | ESP32 pin | Adapter pin |
|---|---|---|
| UART0 TX (ESP32 transmits log output) | GPIO 1 (TX0) | RXD |
| UART0 RX (optional, for sending commands) | GPIO 3 (RX0) | TXD |
| GND | GND | GND |

3. Open a serial terminal (Arduino IDE Serial Monitor, PuTTY, `screen`, etc.)
   at **115200 baud, 8-N-1**.
4. All `ESP_LOGI` / `ESP_LOGW` / `ESP_LOGE` messages appear as normal, e.g.:
   ```
   I (1234) MAIN: Starting SerialController: Step 4
   I (2001) MODBUS: Serial2 initialised at 9600 baud (RX=16 TX=17)
   I (3010) RD60XX: Detected Riden RD60xx (Model: RD6006, ID: 60062, FW: 107)
   ```

> **Note:** GPIO 1/3 are also used by the USB-serial bridge chip on boards with
> an onboard USB port. While the adapter is connected, do not simultaneously
> connect USB — the two drivers will conflict. Disconnect the adapter before
> plugging in USB for a firmware upload.

> **Alternative (no extra hardware):** Once Step 10 (`GET /api/log`) is
> implemented, log messages can be retrieved over WiFi from any browser or
> `curl http://<ip>/api/log` — no serial adapter needed.

## First-time setup

On first boot (or after `/reset`), the ESP32 starts a WiFi access point named
**SerialController**. Connect to it and navigate to `192.168.4.1` to enter your
network credentials. The device saves them to NVS and reconnects automatically
on subsequent boots.

---

## Open Issues / Future Work

The following issues were identified during a code review. They are known and
deferred — not yet fixed.

---

### NOTE · `getResetReasonString` / `getFlashModeString` duplicated in `Diagnostics/Diagnostics.ino`

**Description:** `ESPInfo.h` is the canonical home for these two helper functions
within `SerialController/`. `Diagnostics/Diagnostics.ino` retains its own copies.
This is a structural consequence of Arduino sketch isolation — each sketch folder
is an independent compilation unit and cannot `#include` files from a sibling
folder. The duplication is **accepted and intentional**; `Diagnostics.ino` is a
standalone PoC and is not modified as part of this port.

---

### TODO-3 · Silent failure on background read (`SerialController.ino:78`)

**Description:** In the 30-second background polling loop, a failed Modbus
read has no `else` branch. When `readModbusRegister()` returns `false` after a
timeout, no message is printed and no error is logged. The task silently
continues to the next cycle, making it impossible to distinguish "device not
responding" from "no requests have been made yet" in the serial output.

**Fix:** Add an `else` branch that logs the failure via `ESP_LOGE` and
`Serial.println`, consistent with the write-failure handling on the lines above.

---

### TODO-6 · No input validation on `/setVoltage` (`Server.ino:67`)

**Description:** The `v` query parameter is accepted and forwarded to the
Riden without any range or sanity check:

```cpp
float voltageValue = server.arg("v").toFloat();
uint16_t rawValue = (uint16_t)(voltageValue * 100);
```

`toFloat()` silently returns `0.0` for non-numeric input (e.g. `?v=abc`).
Negative values, zero, and values exceeding the power supply's maximum (e.g.
60 V for typical Riden units, represented as `6000` in the raw centivolt
register) are all accepted without complaint and written directly to the
device.

**Fix:** Validate the parsed float against a configurable `[V_MIN, V_MAX]`
range before converting to the raw register value. Return HTTP 400 with a
descriptive message for out-of-range or unparseable inputs.

---

### TODO-9 · `/setVoltage` registered as `HTTP_POST` but uses a query parameter (`Server.ino:195`)

**Description:** REST convention uses query parameters for GET requests and a
request body for POST. The current implementation accepts the voltage value
only as a URL query string (`server.arg("v")`), which works with both verbs
via query string but is semantically inconsistent. The `curl` example in the
root page uses `-X POST` with a query string, which is technically valid but
unconventional and may confuse REST clients or API documentation tools.

**Fix:** Either change the registration to `HTTP_GET` (simpler, consistent with
query-parameter convention) or switch to reading the value from the POST body
as `application/x-www-form-urlencoded` or JSON.

---

### TODO-10 · Magic numbers for voltage values in background task (`SerialController.ino:58`)

**Description:** The background polling task alternates between two hard-coded
voltage values:

```cpp
uint16_t targetVoltage = toggleVoltage ? 500 : 330;
```

`500` represents 5.00 V and `330` represents 3.30 V in the Riden's centivolt
register encoding, but there are no comments or named constants to make this
clear. Anyone reading the code without context cannot tell what these numbers
mean or what unit they are in.

**Fix:** Replace magic numbers with named constants:

```cpp
const uint16_t VOLTAGE_5V0 = 500;  // 5.00 V in centivolt register units
const uint16_t VOLTAGE_3V3 = 330;  // 3.30 V in centivolt register units
```

Or add an inline comment at the declaration site if the values are only used
in one place.

---

## Java `volatile` vs ESP32 FreeRTOS mutex

The Java source this project is ported from declares all shared fields as `volatile`:

```java
private volatile double voltageOut;
```

In the C++ port every shared field is instead protected by a FreeRTOS mutex. This
is not extra complexity — it is the **direct equivalent** of Java `volatile` on this
hardware. The reason the two approaches differ is fundamental.

### Java: `volatile` is sufficient

In the Java Memory Model (JLS §17), `volatile` provides two guarantees:

1. **Visibility** — a write by one thread is immediately flushed to main memory and
   visible to all other threads on the next read.
2. **Atomicity for individual reads/writes** — a single read or write of any `volatile`
   field (including `double` and `long`) is atomic by specification (JLS §17.7).

No lock is needed for simple get/set operations on a `volatile` field because the JVM
and the underlying hardware memory model enforce both guarantees automatically.

### ESP32 / FreeRTOS: `volatile` is not enough

The ESP32 is a **dual-core** Xtensa LX6 processor. Its two cores have separate L1 data
caches with **no hardware cache-coherency protocol** that maps to Java's memory model.
C++ `volatile` means only *"do not optimise this access away"* — it makes no promise
about inter-core visibility or atomicity.

Two concrete risks exist:

| Risk | Java | ESP32 C++ |
|---|---|---|
| **Torn 64-bit write** | Impossible — JLS §17.7 guarantees atomicity | Possible — a `double` write may be two 32-bit stores; a task preempted between them produces a half-written value |
| **Stale cache line** | Impossible — `volatile` flushes to main memory | Possible — Core 1 may read a value still cached from before Core 0 wrote it |

The FreeRTOS mutex call pair (`xSemaphoreTake` / `xSemaphoreGive`) solves both: it
serialises access across cores **and** includes the memory-barrier instructions that
flush CPU caches, giving the same guarantee Java's `volatile` provides for free.

### Mapping

```
Java                            C++ / ESP32
──────────────────────────      ────────────────────────────────────────
private volatile double x;  →   double x = 0.0;   // inside ConverterState private section
this.x = value;             →   xSemaphoreTake(mutex, portMAX_DELAY);
                                this->x = value;
                                xSemaphoreGive(mutex);
return x;                   →   return x;          // single 32-bit read; safe without mutex
```

> **Note:** This README will be substantially rewritten once the full Java-to-ESP32
> port is complete. The sections above (Architecture, REST API, Open Issues) still
> reflect an earlier iteration of the code.

---
