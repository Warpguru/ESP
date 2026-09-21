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

| Connection | ESP32 pin |
|---|---|
| Riden RX | GPIO 16 (TX2) |
| Riden TX | GPIO 17 (RX2) |
| Fault indicator LED | GPIO 2 (onboard LED) |

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
