#pragma once

/**
 * ConverterStateGlobal.h - extern declaration of the single ConverterState instance.
 *
 * Include this header from any translation unit (Server.cpp, future service
 * classes) that needs to read or write the shared converter state without
 * pulling in the full src/service package include chain.
 *
 * The actual definition `ConverterState converterState;` lives in Application.cpp.
 */

#include "src/service/src/ConverterState.h"

extern ConverterState converterState;
