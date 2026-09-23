# SerialController — ESP32

A port of a Java desktop application to the ESP32 Arduino platform.
The Java application controls DC/DC bench power supplies (Riden RD50xx/RD60xx,
Sinilink XY-series, Wuzhi ZK-series) over Modbus RTU and exposes a REST +
WebSocket API to browser clients. The ESP32 port replicates the same API surface
and behaviour on embedded hardware.

---

## Libraries

### PlatformIO (automatic via `platformio.ini`)

All dependencies are declared in `lib_deps` and downloaded automatically on first
build. No manual installation required.

### Arduino IDE

Use **Sketch → Include Library → Manage Libraries…** and install:

| Library | Author | Version |
|---|---|---|
| WiFiManager | tzapu | ≥ 2.0.17 |
| ArduinoJson | Benoit Blanchon | ≥ 7.4.3 |
| ESPAsyncWebServer | ESP32Async | ≥ 3.12.1 |
| AsyncTCP | ESP32Async | ≥ 3.5.0 |

> `AsyncTCP` is a required dependency of `ESPAsyncWebServer` — install both.

---

## Hardware wiring

### Serial2 — Modbus RTU to DC/DC converter

| Signal | ESP32 GPIO | Wire (Riden 4-pin header) |
|---|---|---|
| UART2 RX (ESP32 receives) | 16 | Green — TxD on device |
| UART2 TX (ESP32 transmits) | 17 | White — RxD on device |
| GND | GND | Black |
| VCC | **NC — do not connect** | Red |

> The TTL header is 3.3 V. GPIO 16/17 are the default Serial2 pins on the
> ESP32-WROOM-32. No level shifter is required.

Sinilink XY-series uses the same 3.3 V TTL wiring on a 4-pin header on the
underside of the control board (Black=GND, Green=RxD, Yellow=TxD, Red=VCC NC).

### Fault indicator

| Signal | ESP32 GPIO |
|---|---|
| Onboard LED — SOS blink on WiFi or device-not-found failure | 2 |

---

## First-time WiFi setup

On first boot (or after `GET /reset`), the ESP32 starts a captive-portal access
point named **SerialController**. Connect to it from any device and navigate to
`192.168.4.1` to enter your network credentials. The device saves them to NVS
and reconnects automatically on subsequent boots.

---

## REST API

| Method | Endpoint | Description |
|---|---|---|
| GET | `/` | HTML landing page with full API reference |
| GET | `/api/state` | Full `ConverterState` snapshot (JSON) |
| GET | `/api/limits` | Device capability limits (JSON) |
| GET | `/api/measurements` | Output voltage, current, power (JSON) |
| GET | `/api/voltage` | Output voltage (JSON) |
| PUT | `/api/voltage` | Set voltage setpoint `{"voltage": 5.0}` |
| PUT | `/api/voltage/verified` | Set + Modbus read-back verify; 409 on mismatch |
| GET | `/api/current` | Output current (JSON) |
| PUT | `/api/current` | Set current setpoint `{"current": 1.0}` |
| PUT | `/api/current/verified` | Set + Modbus read-back verify; 409 on mismatch |
| GET | `/api/power` | Output power (JSON) |
| PUT | `/api/measurements` | Set voltage + current atomically (single 0x10 frame) |
| PUT | `/api/output` | Enable/disable output `{"outputEnable": true}` |
| PUT | `/api/keypad` | Lock/unlock keypad `{"keypadLock": true}` |
| POST | `/api/protection/clear` | Clear tripped protection state |
| GET | `/status` | ESP32 hardware + WiFi diagnostics (JSON) |
| GET | `/reset` | Clear WiFi credentials and reboot into portal mode |

### WebSocket

```
ws://<ip>/ws/data
```

The ESP32 broadcasts a full `ConverterState` JSON snapshot every second.
Clients may also send JSON commands:

| Field | Type | Description |
|---|---|---|
| `setVoltage` | number | Set output voltage setpoint (V) |
| `setCurrent` | number | Set output current setpoint (A) |
| `setOutput` | boolean | Enable (`true`) or disable (`false`) output |
| `setKeypad` | boolean | Lock (`true`) or unlock (`false`) keypad |

---

## Architecture

### Source layout

```
SerialController/
├── SerialController.ino          Arduino IDE entry point (stub — delegates to Application)
├── SerialController.cpp          PlatformIO entry point (#ifndef ARDUINO guard)
├── platformio.ini
└── src/
    ├── modbus/src/
    │   ├── ModbusTransport.h/.cpp    Low-level Modbus RTU framing + FreeRTOS queue
    │   ├── ModbusCRC.h/.cpp
    │   ├── ModbusFunctionCodes.h
    │   └── ModbusConstants.h
    ├── device/src/
    │   ├── base/
    │   │   ├── ModbusDevice.h/.cpp   Abstract base — read/write helpers
    │   │   └── DeviceRegister.h/.cpp Register descriptor (address, scale, name)
    │   ├── RidenRegistersRD60xx.h    Register address namespace for RD60xx
    │   ├── RidenRegistersRD50xx.h    Register address namespace for RD50xx
    │   ├── SinilinkRegisters.h       Register address namespace for Sinilink XY
    │   └── WuzhiRegisters.h          Register address namespace for Wuzhi ZK
    ├── devices/src/
    │   ├── ifc/DC2DCConverter.h      Pure abstract interface (all four drivers)
    │   ├── RidenRD60xx.h/.cpp        Riden RD60xx driver
    │   ├── RidenRD50xx.h/.cpp        Riden RD50xx driver
    │   ├── Sinilink.h/.cpp           Sinilink XY-series driver
    │   └── Wuzhi.h/.cpp              Wuzhi ZK-series driver
    ├── service/src/
    │   ├── ConverterState.h/.cpp     Shared state (mutex-protected)
    │   ├── ConverterTopology.h       enum class BUCK / BOOST / BUCK_BOOST
    │   ├── DeviceDetection.h/.cpp    Auto-detection probing loop
    │   ├── DeviceService.h/.cpp      Polling task + validated write operations
    │   ├── RestService.h/.cpp        All /api/* HTTP handlers
    │   └── WebSocketService.h/.cpp   1 s broadcast task + command queue
    └── SerialController/src/
        ├── Application.h/.cpp        setup()/loop() — global singletons, wiring
        ├── Server.h/.cpp             WiFiManager, route registration, WS handler
        ├── ActiveDevice.h            extern DC2DCConverter* activeDevice
        ├── ConverterStateGlobal.h    extern ConverterState converterState
        └── ESPInfo.h                 GET /status diagnostics helper
```

### Java → C++ mapping

This sketch is a direct port of a Java application. The table below maps each
Java class to its C++ equivalent.

| Java class | C++ equivalent |
|---|---|
| `ModbusTransport` | `ModbusTransport` — same framing logic; Serial2 I/O runs in a dedicated FreeRTOS task (see Threading below) |
| `ModbusDevice` | `ModbusDevice` — same read/write/writeVerified helpers |
| `DeviceRegister` | `DeviceRegister` — same address + scale descriptor |
| `DC2DCConverter` (interface) | `DC2DCConverter` — pure abstract class |
| `RidenRD60xx` / `RidenRD50xx` / `Sinilink` / `Wuzhi` | Same-named concrete drivers |
| `ConverterState` (volatile fields) | `ConverterState` — mutex-protected (see Mutex note below) |
| `ConverterTopology` (enum) | `enum class ConverterTopology` |
| `DeviceService` (polling thread) | `DeviceService` — FreeRTOS polling task |
| `WebSocketService` (broadcaster thread) | `WebSocketService` — FreeRTOS broadcast task |
| `RestService` (Javalin routes) | `RestService` — ESPAsyncWebServer handlers |
| `SerialControllerApplication.main` | `Application.cpp` — `applicationSetup()` / `applicationLoop()` |

---

## Threading model

### Overview

```
Core 0                                  Core 1
──────────────────────────────────────  ──────────────────────────────────────
WiFi / lwIP stack (system)              Arduino main task: setup() → loop()
ESPAsyncWebServer request callbacks       applicationSetup() — wiring
modbusTransportTask (priority 2)          applicationLoop() — WS command drain
  └─ sole owner of Serial2             DeviceService pollingTask (priority 2)
       Serial2.read()                    WebSocketService broadcastTask (priority 1)
       Serial2.write()
       Serial2.available()
       Serial2.end() / begin()
```

### Why a dedicated FreeRTOS task for Serial2?

The Java `ModbusTransport` calls `out.write()` / `in.read()` directly on the
caller's thread. In Java, `DeviceService`'s `synchronized` keyword is the only
concurrency guard — it prevents two threads from entering any method at the same
time, so serial I/O is always single-threaded.

On ESP32, the `ESPAsyncWebServer` delivers HTTP and WebSocket callbacks on
`Core 0` alongside the WiFi/lwIP stack. If Modbus I/O happened directly on the
caller's task it could run concurrently with WiFi processing on Core 0, or race
with another caller on Core 1.

The chosen solution is a **single-owner task pattern**:

- `modbusTransportTask` is the **sole** owner of `Serial2`. No other task ever
  calls `Serial2.read()`, `Serial2.write()`, or `Serial2.end()`/`Serial2.begin()`.
- All callers (DeviceService, DetectDevice, REST handlers via DeviceService)
  call `transport->readRegister()` / `writeRegister()` etc., which enqueue a
  `ModbusRequest` on `_requestQueue` and block until `modbusTransportTask`
  returns the result via a per-call response queue.
- The queue **is** the synchronisation mechanism — it serialises all Modbus
  operations and guarantees exactly one is in flight at a time, matching the
  Java `synchronized` guarantee.
- `modbusTransportTask` is pinned to **Core 0** so its `readBytes()` busy-wait
  loop (`vTaskDelay(1)` between `Serial2.available()` checks) runs in the gaps
  between WiFi bursts on the same core, without starving Core 1 tasks.
- `setBaud()` and `reconnect()` also route through the queue (`MB_OP_SET_BAUD`)
  so that `Serial2.end()`/`Serial2.begin()` always executes **inside**
  `modbusTransportTask`, never racing with an in-progress `readBytes()`.

### Device detection and the Task Watchdog

`detectDevice()` probes four drivers × up to five baud rates before concluding
no device is present. Each failed probe blocks the calling task for up to
`READ_TIMEOUT_MS` while `modbusTransportTask` waits for a serial response.

`READ_TIMEOUT_MS` is set to **100 ms** — well above the <50 ms actual device
response time, and low enough that a full five-baud scan completes in ~2 seconds.

`esp_task_wdt_reset()` is called at the start of each baud-rate iteration to
prevent the FreeRTOS Task Watchdog Timer from firing during the scan. The TWDT
default threshold on Arduino ESP32 is ~5 seconds; without the reset a full
secondary-pass scan (3 baud rates × 4 drivers × 100 ms = 1.2 s) is fine, but
explicit resets are kept as a safety margin for future driver additions.

---

## Java `volatile` vs FreeRTOS mutex

The Java source declares all shared `ConverterState` fields as `volatile`:

```java
private volatile double voltageOut;
```

In the C++ port every shared field is protected by a FreeRTOS mutex instead.
This is not extra complexity — it is the direct equivalent of Java `volatile`
on this hardware.

### Why `volatile` is sufficient in Java

The Java Memory Model (JLS §17) gives `volatile` two guarantees:

1. **Visibility** — a write is immediately flushed to main memory and visible to
   all threads on the next read.
2. **Atomicity** — a single read or write of any `volatile` field (including
   `double` and `long`) is atomic by specification (JLS §17.7).

### Why `volatile` is not enough on ESP32

The ESP32 is a dual-core Xtensa LX6 processor. Its two cores have **separate L1
data caches** with no hardware cache-coherency protocol mapping to Java's memory
model. C++ `volatile` means only *"do not optimise this access away"* — it makes
no promise about inter-core visibility or atomicity of 64-bit values.

| Risk | Java | ESP32 C++ |
|---|---|---|
| **Torn 64-bit write** | Impossible — JLS §17.7 | Possible — a `double` write is two 32-bit stores; a task preempted between them produces a half-written value |
| **Stale cache line** | Impossible — `volatile` flushes | Possible — Core 1 may read a value still cached from before Core 0 wrote it |

`xSemaphoreTake` / `xSemaphoreGive` solve both: they serialise access across
cores **and** include the memory-barrier instructions that flush CPU caches.

### Mapping

```
Java                             C++ / ESP32
────────────────────────         ────────────────────────────────────
private volatile double x;  →    double _x = 0.0;
this.x = value;             →    xSemaphoreTake(_mutex, portMAX_DELAY);
                                  _x = value;
                                  xSemaphoreGive(_mutex);
return x;                   →    return _x;  // single 32-bit load; safe without mutex
```

---

## Startup sequence

```
applicationSetup()
  │
  ├─ Serial.begin(115200)
  ├─ new ModbusTransport(RX=16, TX=17, 9600)
  │    └─ starts modbusTransportTask on Core 0
  │
  ├─ detectDevice(transport, slave=1)
  │    ├─ Pass 1: probe at 115200, 9600 baud
  │    │    for each baud: Sinilink → Wuzhi → RD50xx → RD60xx
  │    └─ Pass 2 (if needed): probe at 19200, 38400, 57600 baud
  │         returns DC2DCConverter* or nullptr
  │
  ├─ [nullptr → WARN logged, server starts without a device]
  │
  └─ setupServer()
       ├─ WiFiManager.autoConnect("SerialController")
       ├─ new DeviceService(state, activeDevice)
       ├─ RestService.registerRoutes()
       ├─ WebSocketService.begin()         starts broadcastTask on Core 1
       ├─ server.begin()                   starts ESPAsyncWebServer
       └─ DeviceService.begin()            starts pollingTask on Core 1

applicationLoop()
  └─ drain WebSocket command queue → dispatch to DeviceService
```

---

## Serial console (battery / headless operation)

All `ESP_LOG*` messages are transmitted on **UART0** (GPIO 1 TX). To read them
without USB:

1. Connect a **3.3 V USB-to-UART adapter** (CP2102, CH340, FT232, etc.).
   **Do not use a 5 V adapter — GPIO 1/3 are not 5 V tolerant.**

| Signal | ESP32 GPIO | Adapter pin |
|---|---|---|
| UART0 TX (log output) | 1 (TX0) | RXD |
| UART0 RX (optional) | 3 (RX0) | TXD |
| GND | GND | GND |

2. Open a terminal at **115200 baud, 8-N-1**.

> While the adapter is connected, do not simultaneously connect USB — the two
> drivers conflict. Disconnect the adapter before uploading firmware.
