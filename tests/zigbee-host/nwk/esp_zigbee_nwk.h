#pragma once
#include <esp_zigbee_core.h>
inline bool rxOnWhenIdle = false;
inline void esp_zb_set_rx_on_when_idle(bool enabled) {
  rxOnWhenIdle = enabled;
}

inline bool esp_zb_get_rx_on_when_idle() {
  return rxOnWhenIdle;
}
