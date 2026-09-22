#pragma once

/**
 * ConverterTopology.h - Power converter topology enumeration.
 *
 * Java equivalent: com.serial.service.ConverterTopology (enum)
 *
 * Describes whether the output voltage is constrained by the input voltage.
 * Used by ConverterState and DeviceService to decide whether to enforce a
 * dynamic voltage ceiling of (voltageIn - 1V) on setpoint writes.
 */
enum class ConverterTopology {

  /** Pure step-down (buck): output cannot exceed input minus dropout voltage (~1V). */
  BUCK,

  /** Pure step-up (boost): output is always higher than input. No Vin ceiling. */
  BOOST,

  /** Combined buck/boost: output may be above or below input. No Vin ceiling. */
  BUCK_BOOST
};
