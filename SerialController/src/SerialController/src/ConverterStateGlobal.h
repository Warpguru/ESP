#pragma once

/**
 * ConverterStateGlobal.h - extern declaration of the single ConverterState instance.
 *
 * Include this header from any translation unit that needs to read or write the
 * shared converter state without pulling in the full service package include chain.
 *
 * The actual definition `ConverterState converterState;` lives in Application.cpp.
 *
 * Java equivalent: com.serial.service.ConverterState instance held as a field in
 * DeviceService and passed to RestService / WebSocketService by reference.
 */

#include "../../service/src/ConverterState.h"

extern ConverterState converterState;
