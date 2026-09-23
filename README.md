# ESP - SerialController

An ESP32-based controller for the **Riden RD5020 (DPS5020)** DC/DC power supply.
The ESP32 speaks **Modbus RTU** over a hardware UART to the converter and exposes a
**RESTful API** and a **web control panel** over WiFi.

---

## Architecture

```mermaid
graph TD
    subgraph ESP32
        subgraph Core0["Core 0 - taskWebServer"]
            WS["WebServer :80\nREST API + HTML UI"]
        end

        subgraph Core1["Core 1 - taskModbus"]
            MB["ModBus.ino\nSerial2 UART2\nModbus RTU poll"]
        end

        DS["DeviceState\nmutex-protected shared state"]
        LOG["SerialStdout\nserialLog() - thread-safe\nCore x prefixed output"]

        WS -- "deviceState_read\ndeviceState_setPending" --> DS
        MB -- "deviceState_write\ndeviceState_takePending" --> DS
        Core0 -. serialLog .-> LOG
        Core1 -. serialLog .-> LOG
    end

    Browser["Browser / curl"]
    Riden["Riden RD5020\nUART2 GPIO 16/17"]

    Browser -- "HTTP GET / POST" --> WS
    MB -- "Modbus RTU 9600 baud" --> Riden
```

| Layer | File(s) | Responsibility |
|---|---|---|
| Main / Tasks | `SerialController.ino` | `setup()`, `loop()`, FreeRTOS task creation, register defines |
| Modbus protocol | `ModBus.ino` | CRC-16, `readModbusRegister()`, `writeModbusRegister()` |
| Shared state | `DeviceState.h` / `DeviceState.ino` | Mutex-protected struct, pending-command pattern |
| Web server & API | `Server.ino` | WiFiManager, HTTP route registration, all request handlers |
| HTML rendering | `HtmlService.ino` | Shared header/footer, control panel page |
| Diagnostics | `DiagnosticsService.ino` | Hardware diagnostics JSON + diagnostics web page |
| Logging | `SerialStdout.ino` | `serialLog()` - thread-safe, `[Core x]` prefixed Serial output |

---

## REST API

| Endpoint | Method | Description |
|---|---|---|
| `GET /` | GET | HTML control panel |
| `GET /api/state` | GET | Full `DeviceState` as JSON |
| `POST /api/voltage?v=12.50` | POST | Set voltage set-point |
| `POST /api/current?i=2.00` | POST | Set current set-point |
| `POST /api/ovp?v=13.00` | POST | Set over-voltage protection |
| `POST /api/ocp?i=3.00` | POST | Set over-current protection |
| `POST /api/output?on=1` | POST | Turn output ON / OFF |
| `POST /api/keylock?lock=1` | POST | Lock / unlock keypad |
| `GET /diagnostics` | GET | HTML diagnostics page |
| `GET /api/diagnostics` | GET | Full hardware diagnostics as JSON |
| `GET /reset` | GET | Clear WiFi credentials and reboot to AP mode |

---

## WiFi Setup

On first boot the ESP32 creates a WiFi access point named **`SerialController`**.
Connect to it and enter your network credentials. They are saved to flash and reused on
every subsequent boot. Call `GET /reset` to clear credentials and return to AP mode.

---

## Hardware Wiring

```mermaid
graph LR
    subgraph ESP32["ESP32"]
        GND_ESP["GND"]
        RX["GPIO 16 - RX2"]
        TX["GPIO 17 - TX2"]
    end

    subgraph Riden["Riden RD5020"]
        GND_RID["GND"]
        RID_TX["TX"]
        RID_RX["RX"]
    end

    GND_ESP -- "common ground" --- GND_RID
    TX      -- "data out  9600 8N1" --> RID_RX
    RID_TX  -- "data in   9600 8N1" --> RX
```

---

## Development Iterations

| # | File | What it builds |
|---|---|---|
| 1 | `Iteration 1.md` | Dev environment, Hello World |
| 2 | `Iteration 2.md` | Serial port + heartbeat |
| 3 | `Iteration 3.md` | Raw Modbus RTU implementation |
| 4 | `Iteration 4.md` | WiFi + basic REST API |
| 5 | `Iteration 5.md` | WiFiManager + persistent credentials + `/reset` |
| 6 | `Iteration 6.md` | Basic web UI |
| 7 | `Iteration 7.md` | Shared `DeviceState` struct + mutex accessors |
| 8 | `Iteration 8.md` | Full web control panel + complete REST API + `HtmlService` |
| 9 | `Iteration 9.md` | Dual-core split - WebServer on Core 0, Modbus on Core 1 + `SerialStdout` |
| 10 | `Iteration 10.md` | Hardware diagnostics API + diagnostics web page |

---

## Repository

[github.com/Warpguru/ESP](https://github.com/Warpguru/ESP)
