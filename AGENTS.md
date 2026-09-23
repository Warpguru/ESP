# AGENTS.md - SerialController ESP32 Port

This file is the authoritative reference for any AI agent or developer working on
this codebase. Read it fully before making any change.

---

## What this project is

**SerialController** is a port of a Java desktop application to the ESP32 Arduino
platform. The Java application talks to DC/DC bench power supplies (Riden, Sinilink,
Wuzhi) over Modbus RTU and exposes a REST + WebSocket API to browser clients.

The ESP32 port replicates the same API surface and behaviour on embedded hardware,
replacing:
- the JVM with the ESP32 Arduino / FreeRTOS runtime
- Javalin HTTP+WebSocket with ESPAsyncWebServer (port 80, HTTP and WS on the same port)
- Jackson JSON with ArduinoJson v7
- Java threads with FreeRTOS tasks pinned to specific cores
- `jSerialComm` with direct `Serial2` / UART2 access via a FreeRTOS-queue Modbus task

### Java source (authoritative specification)

`./Template/` is a Windows directory junction (symlink) pointing to the Java project
tree. **The only working access method is `read_file` with a fully-explicit relative
path.** All other tools fail on this symlink:

| Tool | Result on `Template/` |
|---|---|
| `list_files` | ❌ EPERM - do not use |
| `glob` | ❌ returns no results - do not use |
| `grep` | ❌ returns no results - do not use |
| `read_file` with explicit path | ✅ works correctly |

To read a Java source file, call `read_file` with the path written out in full, e.g.:

```
read_file("Template/src/main/java/com/serial/service/ConverterState.java")
read_file("Template/src/main/java/com/serial/device/RidenRD60xx.java")
```

Do **not** try to discover files by browsing - use the known paths listed below.
Always read the Java counterpart before porting or changing any behaviour.

Key Java source paths (all under `Template/src/main/java/com/serial/`):
```
service/ConverterState.java        - shared state object (authoritative spec)
service/ConverterTopology.java     - topology enum
service/DeviceService.java         - Modbus polling thread, hysteresis, online logic
service/WebSocketService.java      - 1 s broadcaster thread
service/RestService.java           - HTTP route registration
device/ModbusDevice.java           - base class: read/write/writeVerified
device/DeviceRegister.java         - register descriptor (encode/decode)
device/RidenRD60xx.java            - Riden 60xx driver (pollAll, setters)
device/RidenRD50xx.java            - Riden 50xx driver
device/Sinilink.java               - Sinilink XY-series driver
device/Wuzhi.java                  - Wuzhi ZK-series driver
modbus/ModbusTransport.java        - low-level Modbus RTU framing
```

---

## Repository layout

```
ESP/                                   ← workspace root
├── AGENTS.md                          ← this file
├── Agent.md                           ← legacy overview (superseded by this file)
├── doc/
│   ├── esp-port-plan.md               ← step-by-step port plan (source of truth for what to do next)
│   └── SerialController.md            ← register maps, API tables, WebSocket payload fields
├── Template/                          ← symlink to Java project (read-only reference)
└── SerialController/                  ← the ESP32 Arduino sketch (the thing being built)
    ├── SerialController.ino           ← Arduino IDE anchor stub (permanent, ~10 lines)
    ├── SerialController.cpp           ← PlatformIO stub (#ifndef ARDUINO guard, permanent)
    ├── platformio.ini                 ← src_dir=., lib_dir=src
    ├── ConverterStateGlobal.h         ← extern ConverterState converterState; (singleton bridge)
    ├── ESPInfo.h                      ← ESP32 hardware diagnostics → /status endpoint (permanent)
    ├── ModBus.cpp / ModBus.h          ← TRANSITIONAL: working Modbus impl, deleted after Step 4
    ├── RidenConfig.h                  ← TRANSITIONAL: hardware pin/register #defines, deleted after Step 8
    ├── Server.cpp / Server.h          ← WiFi, HTTP handlers, WebSocket stub (permanent, refactored per step)
    ├── ReadMe.md                      ← Arduino IDE library install instructions + TODO list
    └── src/                           ← sketch-local libraries (Arduino IDE compiles recursively)
        ├── modbus/                    ← library: Modbus protocol primitives
        │   └── src/
        │       ├── ModbusConstants.h
        │       ├── ModbusFunctionCodes.h
        │       ├── ModbusCRC.h/.cpp
        │       └── ModbusTransport.h/.cpp   ← STUB (Step 4: replace ModBus.cpp)
        ├── device/                    ← library: abstract device layer + register maps
        │   └── src/
        │       ├── base/
        │       │   ├── DeviceRegister.h/.cpp    ← IMPLEMENTED
        │       │   └── ModbusDevice.h/.cpp      ← STUB (Step 4)
        │       ├── RidenRegistersRD60xx.h/.cpp  ← STUB: 4 placeholder entries (Step 4)
        │       ├── RidenRegistersRD50xx.h/.cpp  ← STUB (Step 8)
        │       ├── SinilinkRegisters.h/.cpp     ← STUB (Step 8)
        │       └── WuzhiRegisters.h/.cpp        ← STUB (Step 8)
        ├── devices/                   ← library: concrete device drivers
        │   └── src/
        │       ├── ifc/
        │       │   └── DC2DCConverter.h         ← pure abstract interface (permanent)
        │       ├── RidenRD60xx.h/.cpp           ← STUB (Step 4)
        │       ├── RidenRD50xx.h/.cpp           ← STUB (Step 8)
        │       ├── Sinilink.h/.cpp              ← STUB (Step 8)
        │       └── Wuzhi.h/.cpp                 ← STUB (Step 8)
        ├── service/                   ← library: service layer (state, polling, REST, WS)
        │   └── src/
        │       ├── ConverterTopology.h          ← enum class ConverterTopology (permanent)
        │       ├── ConverterState.h/.cpp        ← IMPLEMENTED (Step 3, fully Java-compatible)
        │       ├── DeviceService.h/.cpp         ← STUB (Step 4)
        │       ├── RestService.h/.cpp           ← STUB (Step 7)
        │       └── WebSocketService.h/.cpp      ← STUB (Step 5)
        └── SerialController/          ← library: sketch main logic
            └── src/
                ├── Application.h
                └── Application.cpp              ← main setup/loop, 1 s Modbus poller, global instances
```

---

## Dual-toolchain compatibility (CRITICAL)

The sketch must compile and upload correctly under **both**:
- **Arduino IDE 2.x** - opens `SerialController.ino`, compiles all `.cpp`/`.h` files in
  the sketch folder and `src/` recursively.
- **PlatformIO / VSCode** - uses `platformio.ini` with `src_dir = .` and `lib_dir = src`;
  compiles all `.cpp` files it finds, including `SerialController.cpp`.

### The duplicate-symbol problem

Arduino IDE silently converts `SerialController.ino` into a `.cpp` translation unit and
provides its own `setup()` / `loop()` definitions. PlatformIO does **not** do this - it
compiles `SerialController.cpp` as a plain C++ file alongside everything else.

If both `SerialController.ino` (as seen by Arduino IDE) and `SerialController.cpp` (as
seen by PlatformIO) defined `setup()` and `loop()`, each toolchain would compile cleanly
but the **other** would get a duplicate-symbol linker error.

### The solution: `#ifndef ARDUINO` guard

The two entry-point files are kept permanently and work as a matched pair:

**`SerialController.ino`** (Arduino IDE entry point, ~10 lines):
```cpp
#include "src/SerialController/src/Application.h"
void setup() { applicationSetup(); }
void loop()  { applicationLoop(); }
```
Arduino IDE compiles this. PlatformIO also sees it but `.ino` files are **ignored** by
PlatformIO when `src_dir = .` is set - they are not fed to the compiler.

**`SerialController.cpp`** (PlatformIO entry point, ~10 lines):
```cpp
#ifndef ARDUINO
#include "src/SerialController/src/Application.h"
void setup() { applicationSetup(); }
void loop()  { applicationLoop(); }
#endif
```
PlatformIO compiles this (the `#ifndef ARDUINO` block is active because the Arduino
framework does not define `ARDUINO` as a macro in the PlatformIO build for this board
configuration). Arduino IDE also sees this file but the `#ifndef ARDUINO` guard makes
the entire body invisible - `ARDUINO` **is** defined in the Arduino IDE build - so no
duplicate symbols occur.

### Rules that follow from this

- **Never delete** `SerialController.ino` or `SerialController.cpp`.
- **Never add** `setup()` or `loop()` definitions anywhere else.
- **Never create** any other `.ino` file - Arduino IDE merges all `.ino` files in the
  sketch folder into one translation unit, causing redefinition errors.
- All real logic lives in `Application.cpp` (and the `src/` package tree); both stubs
  just delegate to `applicationSetup()` / `applicationLoop()`.

---

## Package layout rules (MUST follow)

Arduino IDE and PlatformIO share a single layout. Both tools compile `src/` recursively.

- Each package lives at `src/<name>/` with a `library.properties` file.
- Source files live in `src/<name>/src/`.
- Relative includes from `src/<pkg>/src/` to sketch root: `../../../<file>.h`
- Relative includes from `src/<pkg>/src/base/` (one level deeper): `../../../../<file>.h`
- Cross-package includes: `../../<other-pkg>/src/<file>.h`
- **Never** use angle-bracket includes for sketch-local files.
- **Never** create `.ino` files for new code - only `SerialController.ino` may exist.

---

## Global instances (singleton pattern)

| Instance | Type | Defined in | Declared extern in |
|---|---|---|---|
| `converterState` | `ConverterState` | `Application.cpp` | `ConverterStateGlobal.h` |
| `activeDevice` | `DC2DCConverter*` | `Application.cpp` (Step 4) | `ActiveDevice.h` (Step 4) |

Include the `*Global.h` / `*Global.h` header from any file that needs the instance.
Never include the package header directly from sketch-root files - use the bridge header.

---

## Java → C++ mapping

| Java construct | C++ equivalent |
|---|---|
| `interface` | Pure abstract class (`= 0` methods, `virtual ~Foo() = default`) |
| `abstract class` | Base class with concrete methods |
| `final class` with static constants | `namespace` with `constexpr` |
| `enum` | `enum class` |
| `volatile` field | Regular field protected by `SemaphoreHandle_t` mutex |
| `synchronized` compound op | `xSemaphoreTake` / `xSemaphoreGive` pair |
| `throws Exception` | `bool` return value (no C++ exceptions on ESP32) |
| `Thread` / `Runnable` | `xTaskCreatePinnedToCore` FreeRTOS task |
| `volatile boolean deviceOnline` | `bool _deviceOnline` under mutex |

---

## Architecture

| Java component | ESP32 equivalent | Status |
|---|---|---|
| `ConverterState` | `ConverterState` C++ class, mutex-protected | ✅ Implemented |
| `ConverterTopology` enum | `enum class ConverterTopology` | ✅ Implemented |
| `DeviceRegister` | `DeviceRegister` C++ class | ✅ Implemented |
| `DC2DCConverter` interface | Pure abstract `DC2DCConverter` class | ✅ Stub (all virtuals declared) |
| `ModbusDevice` abstract base | `ModbusDevice` base class | ⬜ Stub (Step 4) |
| `ModbusTransport` | `ModbusTransport` (stub) + `ModBus.cpp` (working) | ⬜ Step 4 |
| `RidenRD60xx` driver | `RidenRD60xx` class | ⬜ Stub (Step 4) |
| `RidenRD50xx`, `Sinilink`, `Wuzhi` | Concrete driver classes | ⬜ Stubs (Step 8) |
| `DeviceService` (polling thread) | `Application.cpp` poll loop + `ModBus.cpp` task | ⬜ Partial (Step 4) |
| `WebSocketService` (broadcaster) | `wsBroadcastTask` FreeRTOS task | ⬜ Stub (Step 5) |
| `RestService` (Javalin routes) | `Server.cpp` HTTP handlers | ⬜ Partial (Step 7) |
| Javalin HTTP+WS server | ESPAsyncWebServer, port 80 | ✅ Done |
| Jackson JSON | ArduinoJson v7 | ✅ Done |
| Browser UI (`index.html`) | PROGMEM HTML in `Server.cpp` | ⬜ Step 9 |

---

## Transitional files (will be deleted)

| File | Deleted in | Reason |
|---|---|---|
| `ModBus.cpp` / `ModBus.h` | Step 4 | Replaced by `src/modbus/src/ModbusTransport.cpp` |
| `RidenConfig.h` (register `#defines`) | Step 8 | Replaced by `RidenRegistersRD60xx` + per-driver constructors |
| `RidenConfig.h` (pin/baud `#defines`) | Step 8 | `RX_PIN`/`TX_PIN`/`BAUDRATE` move into `ModbusTransport` constructor |

⚠ `ConverterState.cpp` currently includes `ModBus.h` and uses `RIDEN_ID`/`REG_V_SET`/
`REG_I_SET` in `applyVoltageSetpoint` / `applyCurrentSetpoint`. Step 4 must refactor
these two methods to delegate to `activeDevice->setVoltage()` / `setCurrent()` instead,
after which the `ModBus.h` and `RidenConfig.h` includes can be removed from
`ConverterState.cpp`.

---

## Permanent sketch-root files

| File | Purpose |
|---|---|
| `SerialController.ino` | Arduino IDE entry point (stub, permanent) |
| `SerialController.cpp` | PlatformIO entry point (`#ifndef ARDUINO` guard, permanent) |
| `ConverterStateGlobal.h` | extern bridge for the `converterState` singleton |
| `ESPInfo.h` | ESP32 hardware diagnostics for `GET /status` (no planned replacement) |
| `Server.cpp` / `Server.h` | WiFi, HTTP, WebSocket - refactored per step, never deleted |

---

## Step status (see `doc/esp-port-plan.md` for full details)

| Step | Title | Status |
|---|---|---|
| 1 | Expand `/status` with full diagnostics | ✅ Done |
| 2 | Add ESPAsyncWebServer + ArduinoJson | ✅ Done |
| 3 | Introduce ConverterState + wire to Modbus | ✅ Done |
| 4 | C++ device class hierarchy | ⬜ Pending |
| 5 | WebSocket broadcast (server → client) | ⬜ Pending |
| 6 | WebSocket command handling (client → server) | ⬜ Pending |
| 7 | REST API alignment with Java spec | ⬜ Pending |
| 8 | Multi-device support + auto-detection | ⬜ Pending |
| 9 | Browser UI (single-page HTML) | ⬜ Pending |

---

## Coding rules

### Java compatibility (highest priority)

Java compatibility is the **single most important constraint** in this codebase.
Every C++ class, method, field, and package is a direct translation of its Java
counterpart. Deviating from Java naming or structure requires an explicit reason.

- **Class names** must match the Java class name exactly:
  `ConverterState`, `ModbusTransport`, `DeviceRegister`, `RidenRD60xx`, etc.
- **Method names** must match the Java method name exactly:
  `getVoltageOut()`, `setVoltageOut()`, `pollAll()`, `applyVoltageSetpoint()`, etc.
- **Field names** follow the C++ convention of a leading underscore on private fields
  (`_voltageOut`) but otherwise match the Java field name (`voltageOut` → `_voltageOut`).
- **Package structure** maps directly: Java package `com.serial.service` →
  C++ library `src/service/src/`; `com.serial.device` → `src/device/src/`; etc.
- **Enumerator names** match Java exactly: `ConverterTopology::BUCK`,
  `ConverterTopology::BOOST`, `ConverterTopology::BUCK_BOOST`.
- **Do not rename** anything to suit C++ conventions or personal preference.
  If a Java name is awkward in C++, keep it anyway and add a comment explaining why.

### Code style

- **Every method body starts on a new line after `{`.** This applies to all methods
  without exception - including trivial getters and setters.

  ✅ Correct:
  ```cpp
  double ConverterState::getVoltageOut() const {
      return voltageOut;
  }

  void ConverterState::setVoltageOut(double voltageOut) {
      xSemaphoreTake(mutex, portMAX_DELAY);
      this->voltageOut = voltageOut;
      xSemaphoreGive(mutex);
  }
  ```
  ❌ Wrong:
  ```cpp
  double ConverterState::getVoltageOut() const { return voltageOut; }
  void ConverterState::setVoltageOut(double v) { xSemaphoreTake(...); _voltageOut = v; xSemaphoreGive(...); }
  ```

- **No single-line `if` / `for` / `while` bodies.** After every `{` the body starts
  on a new line. Always use braces - never omit them.

  ✅ Correct:
  ```cpp
  if (ok) {
      converterState.setDeviceOnline(true);
  }
  ```
  ❌ Wrong:
  ```cpp
  if (ok) { converterState.setDeviceOnline(true); }
  if (ok) converterState.setDeviceOnline(true);
  ```

- **Braces on the same line** as the control keyword (`if (x) {`, `for (...) {`).
- **Indentation:** 2 spaces.

### General rules

- **Read before writing.** Never speculate about code you have not opened.
- **Read the Java counterpart** before porting any class or method.
  Use `read_file` with the full explicit path, e.g.
  `read_file("Template/src/main/java/com/serial/service/ConverterState.java")`.
  `list_files`, `glob`, and `grep` all fail silently on the `Template/` symlink -
  `read_file` with an explicit path is the **only** tool that works.
- **Minimal change.** Only modify lines directly required. Never reformat, rename, or
  refactor unrelated code.
- **No new `.ino` files.** All new code is `.cpp` / `.h`.
- **No duplicate globals.** The `converterState` instance is defined exactly once in
  `Application.cpp`. All other files use `ConverterStateGlobal.h`.
- **Relative includes only** for sketch-local files - no angle brackets.
- **No C++ exceptions.** Use `bool` return values for fallible operations.
- **Thread safety.** Every mutable field in `ConverterState` is protected by `_mutex`.
  Getters for primitive types return by value without the mutex (matches Java
  `volatile` semantics - single-word reads are atomic on Xtensa LX6).
- **Step discipline.** Implement one step at a time. Do not implement a future step's
  work speculatively. Ask before starting any step.
