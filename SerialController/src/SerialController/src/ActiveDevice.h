#pragma once

/**
 * ActiveDevice.h - extern declaration of the single DC2DCConverter instance.
 *
 * Include this header from any translation unit that needs to call methods on the
 * active device driver without pulling in the full devices package include chain.
 *
 * The actual definition `DC2DCConverter* activeDevice = nullptr;` lives in
 * Application.cpp, where it is constructed and assigned in applicationSetup().
 *
 * Java equivalent: the DeviceService#converter field — the active device driver
 * instance owned by DeviceService. Here it is a global because Application.cpp
 * constructs it before DeviceService (which needs it as a constructor argument).
 */

#include "../../devices/src/ifc/DC2DCConverter.h"

extern DC2DCConverter* activeDevice;
