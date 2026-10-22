
#pragma once
#include <Arduino.h>
inline void esp_schedule() {}
template<class Predicate> void esp_delay(uint32_t ms, Predicate blocked) {
  if (blocked()) delay(ms);
}
