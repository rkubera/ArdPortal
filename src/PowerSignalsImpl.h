// SPDX-License-Identifier: MIT
#pragma once
#include "ArdPower.h"
namespace ArdPowerDetail {
void ARDPORTAL_POWER_ISR_ATTR Signals::pinIsr(void* argument) {
  auto* slot = static_cast<Pin*>(argument);
  if (slot->wake) {
    slot->masked = true;  // Level interrupts are rearmed from loop only after release.
#if defined(ESP32)
    gpio_ll_intr_disable(GPIO_LL_GET_HW(GPIO_PORT_0), slot->pin);
#else
    GPC(slot->pin) &= ~(0xFU << GPCI);
#endif
  }
  slot->owner->recordFromISR(uint32_t(1) << slot->index);
}
void ARDPORTAL_POWER_ISR_ATTR Signals::recordFromISR(uint32_t mask) {
#if defined(ESP32)
  portENTER_CRITICAL_ISR(&_mux);
  _pendingSignals |= mask;
#if configSUPPORT_STATIC_ALLOCATION
  SemaphoreHandle_t wake = _wait;
#endif
  portEXIT_CRITICAL_ISR(&_mux);
#if configSUPPORT_STATIC_ALLOCATION
  if (wake) {
    BaseType_t higher = pdFALSE;
    xSemaphoreGiveFromISR(wake, &higher);
    if (higher) portYIELD_FROM_ISR();
  }
#endif
#else
  uint32_t saved = xt_rsil(15);
  _pendingSignals |= mask;
  xt_wsr_ps(saved);
  esp_schedule();
#endif
}
}
