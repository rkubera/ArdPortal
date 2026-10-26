// SPDX-License-Identifier: MIT
#pragma once
#include "PowerPolicy.h"
#include "LanguageLiterals.h"
#include <Arduino.h>

// Portal decisions only: these are not notifications of physical SDK sleep/wake.
// Four bytes per portal; format text only when the decision changes.
class ArdPowerDiagnostics {
public:
  /** @brief Format a changed library saving decision; return empty when unchanged. Does not observe physical sleep/wake cycles. */
  String update(ArdPowerPolicy policy, ArdPowerSleepType sleep, uint16_t blockers, bool admitted, bool released, bool connected, bool supported) {
    const uint8_t decision = policy == ArdPowerPolicy::AlwaysOn || sleep != ArdPowerSleepType::ConnectedSleep ? 0 : !supported ? 1
                                                                                                                  : !connected ? 2
                                                                                                                  : !admitted  ? 3
                                                                                                                  : !released  ? 4
                                                                                                                               : 5;
    if (decision != 3) blockers = 0;
    // Reasons describe entry into a decision. Polling HTTP and reconnecting
    // sockets may change the blocker set without changing sleep admission.
    const uint32_t signature = (uint32_t(decision) << 16) | (uint32_t(policy) << 19) | (uint32_t(sleep) << 21);
    if (signature == _previous) return String();
    _previous = signature;
#if ARDPORTAL_MATTER_THREAD
    String message = F(ARD_LANG_NATIVE_POWER_THREAD);
#else
    String message = F(ARD_LANG_NATIVE_POWER_WIFI);
#endif
    if (decision == 0) return message + F(ARD_LANG_NATIVE_POWER_DISABLED);
    if (decision == 1) return message + F(ARD_LANG_NATIVE_POWER_NO_ICD);
    if (decision == 2) return message + F(ARD_LANG_NATIVE_POWER_NETWORK);
    if (decision == 4) return message + F(ARD_LANG_NATIVE_POWER_PENDING);
    if (decision == 5) return message + F(ARD_LANG_NATIVE_POWER_ALLOWED);
    message += F(ARD_LANG_NATIVE_POWER_PAUSED);
    if (!blockers) return message + F(ARD_LANG_NATIVE_POWER_WAITING);
    bool separator = false;
    auto reason = [&](ArdPowerBlocker bit, const __FlashStringHelper* label) {
      if (!(blockers & uint16_t(bit))) return;
      if (separator) message += F(", ");
      message += label;
      separator = true;
    };
    reason(ArdPowerBlocker::Startup, F(ARD_LANG_NATIVE_POWER_STARTUP));
    reason(ArdPowerBlocker::Storage, F(ARD_LANG_NATIVE_POWER_STORAGE));
#if ARDPORTAL_ENABLE_OTA
    reason(ArdPowerBlocker::Ota, F("OTA"));
#endif
    reason(ArdPowerBlocker::Restart, F(ARD_LANG_NATIVE_POWER_RESTART));
#if ARDPORTAL_ENABLE_WIFI
    reason(ArdPowerBlocker::AccessPoint, F(ARD_LANG_NATIVE_POWER_AP));
#endif
    reason(ArdPowerBlocker::Network, F(ARD_LANG_NATIVE_POWER_NETWORK_WORK));
#if ARDPORTAL_ENABLE_WWW
    reason(ArdPowerBlocker::Http, F("HTTP"));
#endif
#if ARDPORTAL_ENABLE_MQTT
    reason(ArdPowerBlocker::Mqtt, F("MQTT"));
#endif
#if ARDPORTAL_SUPPORT_WEBSOCKET
    reason(ArdPowerBlocker::WebSocket, F("WebSocket"));
#endif
    reason(ArdPowerBlocker::Application, F(ARD_LANG_NATIVE_POWER_APPLICATION));
#if ARDPORTAL_ENABLE_ZIGBEE
    reason(ArdPowerBlocker::Zigbee, F("Zigbee"));
#endif
    reason(ArdPowerBlocker::WakePin, F(ARD_LANG_NATIVE_POWER_WAKE_PIN));
    reason(ArdPowerBlocker::Interrupt, F(ARD_LANG_NATIVE_POWER_INTERRUPT));
    reason(ArdPowerBlocker::Event, F(ARD_LANG_NATIVE_POWER_EVENT));
    return message;
  }
private:
  uint32_t _previous = UINT32_MAX;
};
