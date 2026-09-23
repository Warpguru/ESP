#pragma once

/**
 * ActiveDevice.h - extern declaration of the single DC2DCConverter instance.
 *
 * Include this header from any translation unit (Server.cpp, Application.cpp)
 * that needs to call methods on the active device driver without pulling in
 * the full devices package include chain.
 *
 * The actual definition `DC2DCConverter* activeDevice = nullptr;` lives in
 * Application.cpp, where it is constructed and assigned in applicationSetup().
 *
 * Analogous to ConverterStateGlobal.h for the converterState singleton.
 */

#include "src/devices/src/ifc/DC2DCConverter.h"

extern DC2DCConverter* activeDevice;
