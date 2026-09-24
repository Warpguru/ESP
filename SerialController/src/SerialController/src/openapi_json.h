#pragma once
#include <pgmspace.h>

/**
 * openapi_json.h - OpenAPI 3.0.0 specification for the SerialController REST API.
 *
 * Stored in flash (PROGMEM) and served at GET /openapi.json via send_P().
 * The Swagger UI at GET /openapi/ui fetches this to render the interactive explorer.
 *
 * Covers all endpoints registered in RestService.cpp and Server.cpp.
 * Field names, types, and descriptions match doc/SerialController.md exactly.
 *
 * Java equivalent: Javalin auto-generates /openapi from Javalin-OpenAPI annotations.
 * On ESP32 the spec is hand-authored and embedded in flash.
 */
static const char OPENAPI_JSON[] PROGMEM = R"rawjson(
{
  "openapi": "3.0.0",
  "info": {
    "title": "SerialController API",
    "version": "1.0.0",
    "description": "REST API for controlling DC/DC bench power supplies (Riden, Sinilink, Wuzhi) via Modbus RTU on ESP32. Interactive UI at /openapi/ui."
  },
  "servers": [
    { "url": "/", "description": "This device" }
  ],
  "tags": [
    { "name": "State & Limits",  "description": "Read-only device state and capability limits" },
    { "name": "Measurements",    "description": "Read-only live measurements" },
    { "name": "Setpoints",       "description": "Write voltage, current, and both together" },
    { "name": "Device Control",  "description": "Output enable, keypad lock, protection clear" },
    { "name": "Diagnostics",     "description": "Logging and ESP32 hardware status" }
  ],
  "paths": {
    "/api/state": {
      "get": {
        "tags": ["State & Limits"],
        "summary": "Full converter state snapshot",
        "description": "Returns the complete ConverterState: measured values, setpoints, device identity, limits, and status flags.",
        "responses": {
          "200": {
            "description": "OK",
            "content": {
              "application/json": {
                "schema": { "$ref": "#/components/schemas/StateResponse" }
              }
            }
          }
        }
      }
    },
    "/api/limits": {
      "get": {
        "tags": ["State & Limits"],
        "summary": "Device capability limits",
        "description": "Returns the min/max voltage, current, and power the connected device can handle.",
        "responses": {
          "200": {
            "description": "OK",
            "content": {
              "application/json": {
                "schema": { "$ref": "#/components/schemas/LimitsResponse" }
              }
            }
          }
        }
      }
    },
    "/api/measurements": {
      "get": {
        "tags": ["Measurements"],
        "summary": "Live voltage, current and power",
        "responses": {
          "200": {
            "description": "OK",
            "content": {
              "application/json": {
                "schema": { "$ref": "#/components/schemas/MeasurementsResponse" }
              }
            }
          }
        }
      },
      "put": {
        "tags": ["Setpoints"],
        "summary": "Set voltage and current setpoints together",
        "requestBody": {
          "required": true,
          "content": {
            "application/json": {
              "schema": { "$ref": "#/components/schemas/MeasurementsRequest" }
            }
          }
        },
        "responses": {
          "204": { "description": "Setpoints applied" },
          "400": { "description": "Value out of range or malformed body", "content": { "text/plain": { "schema": { "type": "string" } } } },
          "503": { "description": "No device connected",                  "content": { "text/plain": { "schema": { "type": "string" } } } },
          "500": { "description": "Device write failed",                  "content": { "text/plain": { "schema": { "type": "string" } } } }
        }
      }
    },
    "/api/voltage": {
      "get": {
        "tags": ["Measurements"],
        "summary": "Measured output voltage",
        "responses": {
          "200": {
            "description": "OK",
            "content": {
              "application/json": {
                "schema": { "$ref": "#/components/schemas/VoltageResponse" }
              }
            }
          }
        }
      },
      "put": {
        "tags": ["Setpoints"],
        "summary": "Set voltage setpoint",
        "requestBody": {
          "required": true,
          "content": {
            "application/json": {
              "schema": { "$ref": "#/components/schemas/VoltageRequest" }
            }
          }
        },
        "responses": {
          "204": { "description": "Setpoint applied" },
          "400": { "description": "Value out of range or malformed body", "content": { "text/plain": { "schema": { "type": "string" } } } },
          "503": { "description": "No device connected",                  "content": { "text/plain": { "schema": { "type": "string" } } } },
          "500": { "description": "Device write failed",                  "content": { "text/plain": { "schema": { "type": "string" } } } }
        }
      }
    },
    "/api/voltage/verified": {
      "put": {
        "tags": ["Setpoints"],
        "summary": "Set voltage setpoint with read-back verification",
        "description": "Writes the voltage setpoint and reads it back. Returns 409 if the device did not accept the value.",
        "requestBody": {
          "required": true,
          "content": {
            "application/json": {
              "schema": { "$ref": "#/components/schemas/VoltageRequest" }
            }
          }
        },
        "responses": {
          "200": {
            "description": "Setpoint accepted and confirmed",
            "content": {
              "application/json": {
                "schema": { "$ref": "#/components/schemas/VerifiedVoltageResponse" }
              }
            }
          },
          "400": { "description": "Value out of range or malformed body",       "content": { "text/plain": { "schema": { "type": "string" } } } },
          "409": { "description": "Voltage setpoint not accepted by device",    "content": { "text/plain": { "schema": { "type": "string" } } } },
          "503": { "description": "No device connected",                        "content": { "text/plain": { "schema": { "type": "string" } } } }
        }
      }
    },
    "/api/current": {
      "get": {
        "tags": ["Measurements"],
        "summary": "Measured output current",
        "responses": {
          "200": {
            "description": "OK",
            "content": {
              "application/json": {
                "schema": { "$ref": "#/components/schemas/CurrentResponse" }
              }
            }
          }
        }
      },
      "put": {
        "tags": ["Setpoints"],
        "summary": "Set current setpoint",
        "requestBody": {
          "required": true,
          "content": {
            "application/json": {
              "schema": { "$ref": "#/components/schemas/CurrentRequest" }
            }
          }
        },
        "responses": {
          "204": { "description": "Setpoint applied" },
          "400": { "description": "Value out of range or malformed body", "content": { "text/plain": { "schema": { "type": "string" } } } },
          "503": { "description": "No device connected",                  "content": { "text/plain": { "schema": { "type": "string" } } } },
          "500": { "description": "Device write failed",                  "content": { "text/plain": { "schema": { "type": "string" } } } }
        }
      }
    },
    "/api/current/verified": {
      "put": {
        "tags": ["Setpoints"],
        "summary": "Set current setpoint with read-back verification",
        "description": "Writes the current setpoint and reads it back. Returns 409 if the device did not accept the value.",
        "requestBody": {
          "required": true,
          "content": {
            "application/json": {
              "schema": { "$ref": "#/components/schemas/CurrentRequest" }
            }
          }
        },
        "responses": {
          "200": {
            "description": "Setpoint accepted and confirmed",
            "content": {
              "application/json": {
                "schema": { "$ref": "#/components/schemas/VerifiedCurrentResponse" }
              }
            }
          },
          "400": { "description": "Value out of range or malformed body",       "content": { "text/plain": { "schema": { "type": "string" } } } },
          "409": { "description": "Current setpoint not accepted by device",    "content": { "text/plain": { "schema": { "type": "string" } } } },
          "503": { "description": "No device connected",                        "content": { "text/plain": { "schema": { "type": "string" } } } }
        }
      }
    },
    "/api/power": {
      "get": {
        "tags": ["Measurements"],
        "summary": "Measured output power",
        "responses": {
          "200": {
            "description": "OK",
            "content": {
              "application/json": {
                "schema": { "$ref": "#/components/schemas/PowerResponse" }
              }
            }
          }
        }
      }
    },
    "/api/output": {
      "put": {
        "tags": ["Device Control"],
        "summary": "Enable or disable converter output",
        "requestBody": {
          "required": true,
          "content": {
            "application/json": {
              "schema": { "$ref": "#/components/schemas/OutputRequest" }
            }
          }
        },
        "responses": {
          "204": { "description": "Output state applied" },
          "400": { "description": "Malformed body",        "content": { "text/plain": { "schema": { "type": "string" } } } },
          "503": { "description": "No device connected",   "content": { "text/plain": { "schema": { "type": "string" } } } },
          "500": { "description": "Device write failed",   "content": { "text/plain": { "schema": { "type": "string" } } } }
        }
      }
    },
    "/api/keypad": {
      "put": {
        "tags": ["Device Control"],
        "summary": "Lock or unlock the device keypad",
        "requestBody": {
          "required": true,
          "content": {
            "application/json": {
              "schema": { "$ref": "#/components/schemas/KeypadRequest" }
            }
          }
        },
        "responses": {
          "204": { "description": "Keypad state applied" },
          "400": { "description": "Malformed body",        "content": { "text/plain": { "schema": { "type": "string" } } } },
          "503": { "description": "No device connected",   "content": { "text/plain": { "schema": { "type": "string" } } } },
          "500": { "description": "Device write failed",   "content": { "text/plain": { "schema": { "type": "string" } } } }
        }
      }
    },
    "/api/protection/clear": {
      "post": {
        "tags": ["Device Control"],
        "summary": "Clear protection state",
        "description": "Clears any active over-voltage, over-current, or over-temperature protection trip.",
        "responses": {
          "204": { "description": "Protection cleared" },
          "503": { "description": "No device connected", "content": { "text/plain": { "schema": { "type": "string" } } } },
          "500": { "description": "Device write failed", "content": { "text/plain": { "schema": { "type": "string" } } } }
        }
      }
    },
    "/api/log": {
      "get": {
        "tags": ["Diagnostics"],
        "summary": "Recent log entries",
        "description": "Returns the last N log lines as a JSON array. Add ?clear=1 to flush the buffer.",
        "parameters": [
          {
            "name": "clear",
            "in": "query",
            "required": false,
            "schema": { "type": "integer", "enum": [1] },
            "description": "Pass 1 to clear the log buffer after reading"
          }
        ],
        "responses": {
          "200": {
            "description": "OK",
            "content": {
              "application/json": {
                "schema": {
                  "type": "object",
                  "properties": {
                    "lines": {
                      "type": "array",
                      "items": { "type": "string" }
                    }
                  }
                }
              }
            }
          }
        }
      }
    },
    "/api/log/level": {
      "put": {
        "tags": ["Diagnostics"],
        "summary": "Change log level at runtime",
        "description": "Sets the active log level. Valid values: ERROR, WARN, INFO, DEBUG, TRACE.",
        "requestBody": {
          "required": true,
          "content": {
            "application/json": {
              "schema": {
                "type": "object",
                "required": ["level"],
                "properties": {
                  "level": {
                    "type": "string",
                    "enum": ["ERROR", "WARN", "INFO", "DEBUG", "TRACE"],
                    "description": "New log level"
                  }
                }
              }
            }
          }
        },
        "responses": {
          "204": { "description": "Log level updated" },
          "400": { "description": "Unknown level", "content": { "text/plain": { "schema": { "type": "string" } } } }
        }
      }
    },
    "/status": {
      "get": {
        "tags": ["Diagnostics"],
        "summary": "ESP32 hardware and WiFi diagnostics",
        "description": "Returns heap, flash, chip info, WiFi RSSI, and uptime.",
        "responses": {
          "200": {
            "description": "OK",
            "content": { "application/json": { "schema": { "type": "object" } } }
          }
        }
      }
    },
    "/reset": {
      "get": {
        "tags": ["Diagnostics"],
        "summary": "Clear WiFi credentials and reboot",
        "description": "Erases the stored WiFi SSID/password from NVS and immediately reboots the ESP32 into WiFiManager captive-portal (configuration) mode. The log level setting is NOT cleared. Use with caution - the device will be unreachable until WiFi credentials are re-entered via the captive portal.",
        "responses": {
          "200": {
            "description": "Credentials cleared; ESP32 is rebooting",
            "content": { "text/plain": { "schema": { "type": "string" } } }
          }
        }
      }
    }
  },
  "components": {
    "schemas": {
      "StateResponse": {
        "type": "object",
        "properties": {
          "deviceName":         { "type": "string",  "description": "Device model, e.g. RD6006" },
          "manufacturer":       { "type": "string",  "description": "Manufacturer name" },
          "firmwareVersion":    { "type": "string",  "description": "Firmware version string" },
          "deviceOnline":       { "type": "boolean", "description": "true when Modbus communication is healthy" },
          "converterTopology":  { "type": "integer", "description": "0=BUCK, 1=BOOST, 2=BUCK_BOOST" },
          "voltageOut":         { "type": "number",  "description": "Measured output voltage (V)" },
          "currentOut":         { "type": "number",  "description": "Measured output current (A)" },
          "powerOut":           { "type": "number",  "description": "Measured output power (W)" },
          "voltageIn":          { "type": "number",  "description": "Measured input voltage (V)" },
          "temperatureCelsius": { "type": "number",  "description": "Device temperature (C)" },
          "voltageSet":         { "type": "number",  "description": "Current voltage setpoint (V)" },
          "currentSet":         { "type": "number",  "description": "Current current setpoint (A)" },
          "outputEnabled":      { "type": "boolean", "description": "Output on/off state" },
          "keypadLocked":       { "type": "boolean", "description": "Keypad lock state" },
          "cvMode":             { "type": "boolean", "description": "true = CV mode, false = CC mode" },
          "protectionState":    { "type": "integer", "description": "0 = normal, non-zero = protection tripped" },
          "maxVoltage":         { "type": "number",  "description": "Device maximum voltage (V)" },
          "minVoltage":         { "type": "number",  "description": "Device minimum voltage (V)" },
          "maxCurrent":         { "type": "number",  "description": "Device maximum current (A)" },
          "minCurrent":         { "type": "number",  "description": "Device minimum current (A)" },
          "maxPower":           { "type": "number",  "description": "Device maximum power (W)" },
          "configMaxVoltage":   { "type": "number",  "description": "Operator voltage cap (V); 0 = no cap" },
          "configMaxCurrent":   { "type": "number",  "description": "Operator current cap (A); 0 = no cap" }
        }
      },
      "LimitsResponse": {
        "type": "object",
        "properties": {
          "manufacturer": { "type": "string", "description": "Manufacturer name" },
          "deviceName":   { "type": "string", "description": "Device model" },
          "minVoltage":   { "type": "number", "description": "Minimum voltage setpoint (V)" },
          "maxVoltage":   { "type": "number", "description": "Maximum voltage setpoint (V)" },
          "minCurrent":   { "type": "number", "description": "Minimum current setpoint (A)" },
          "maxCurrent":   { "type": "number", "description": "Maximum current setpoint (A)" },
          "maxPower":     { "type": "number", "description": "Maximum power (W)" }
        }
      },
      "MeasurementsResponse": {
        "type": "object",
        "properties": {
          "voltage": { "type": "number", "description": "Measured output voltage (V)" },
          "current": { "type": "number", "description": "Measured output current (A)" },
          "power":   { "type": "number", "description": "Measured output power (W)" }
        }
      },
      "MeasurementsRequest": {
        "type": "object",
        "required": ["voltage", "current"],
        "properties": {
          "voltage": { "type": "number", "description": "Voltage setpoint (V)" },
          "current": { "type": "number", "description": "Current setpoint (A)" },
          "power":   { "type": "number", "description": "Reserved; send 0" }
        }
      },
      "VoltageResponse": {
        "type": "object",
        "properties": {
          "voltage": { "type": "number", "description": "Measured output voltage (V)" }
        }
      },
      "VoltageRequest": {
        "type": "object",
        "required": ["voltage"],
        "properties": {
          "voltage": { "type": "number", "description": "Voltage setpoint (V)" }
        }
      },
      "VerifiedVoltageResponse": {
        "type": "object",
        "properties": {
          "voltageSet": { "type": "number", "description": "Confirmed voltage setpoint read back from device (V)" }
        }
      },
      "CurrentResponse": {
        "type": "object",
        "properties": {
          "current": { "type": "number", "description": "Measured output current (A)" }
        }
      },
      "CurrentRequest": {
        "type": "object",
        "required": ["current"],
        "properties": {
          "current": { "type": "number", "description": "Current setpoint (A)" }
        }
      },
      "VerifiedCurrentResponse": {
        "type": "object",
        "properties": {
          "currentSet": { "type": "number", "description": "Confirmed current setpoint read back from device (A)" }
        }
      },
      "PowerResponse": {
        "type": "object",
        "properties": {
          "power": { "type": "number", "description": "Measured output power (W)" }
        }
      },
      "OutputRequest": {
        "type": "object",
        "required": ["outputEnable"],
        "properties": {
          "outputEnable": { "type": "boolean", "description": "true to enable output, false to disable" }
        }
      },
      "KeypadRequest": {
        "type": "object",
        "required": ["keypadLock"],
        "properties": {
          "keypadLock": { "type": "boolean", "description": "true to lock keypad, false to unlock" }
        }
      }
    }
  }
}
)rawjson";
