// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>

// Cause of the current boot. Light-sleep GPIO/timer events use onWake().
enum class ArdPowerWakeCause : uint8_t {
  Unknown,
  PowerOn,
  Timer,
  Gpio,
  DeepSleep,
  ExternalReset,
  SoftwareReset,
  Watchdog,
  Panic,
  Brownout,
  OtherReset,
  OtherWake
};

enum class ArdPowerWakeLevel : uint8_t { Low,
                                         High };
enum class ArdPowerInterrupt : uint8_t { Rising,
                                         Falling,
                                         Change };
enum class ArdPowerWakeScope : uint8_t { LightSleep,
                                         LightAndDeepSleep };
enum class ArdPowerEventKind : uint8_t { Gpio,
                                         Interrupt,
                                         Timer,
                                         Periodic };
struct ArdPowerEvent {
  ArdPowerEventKind kind;
  uint8_t id;  // GPIO number, external interrupt source (0..15), application timer ID, or 0 for Periodic.
  uint32_t atMs;
  uint32_t missed;  // Skipped periods of a cyclic timer/Periodic schedule; repeated ISR signals coalesce.
};
#ifndef ARDPORTAL_POWER_ISR_ATTR
#define ARDPORTAL_POWER_ISR_ATTR
#endif
