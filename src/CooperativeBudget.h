// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>

// Scheduler checkpoints do not invoke portal callbacks or re-enter loop().
// Call only after a core flash operation has returned with cache enabled.
class ArdCooperativeBudget {
public:
  /**
   * @brief Yield periodically during synchronous work without yielding for every unit.
   * @return No value.
   */
  void checkpoint() {
    if(uint32_t(millis()-_since)>=4) {yield();_since=millis();}
  }
private:
  uint32_t _since=millis();
};
