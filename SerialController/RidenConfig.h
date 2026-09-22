#pragma once

/**
 * RidenConfig.h - Riden power supply hardware configuration constants.
 *
 * Shared by Application.cpp (main loop) and Server.cpp (HTTP handlers)
 * so both translation units see the same register map and device ID.
 */

// Serial Pins & UART Configuration
#define RX_PIN 16
#define TX_PIN 17
#define BAUDRATE 9600
#define RIDEN_ID 1

// Riden Registers (Hex)
#define REG_V_SET 0x0002
#define REG_I_SET 0x0003
#define REG_V_OUT 0x0008
#define REG_I_OUT 0x0009
#define REG_OUTPUT 0x0012
