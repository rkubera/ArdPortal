// SPDX-License-Identifier: MIT
#pragma once
#include "PowerEvents.h"
#if defined(ESP32)
#include <driver/gpio.h>
#include <hal/gpio_ll.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#else
#include <coredecls.h>
#endif

namespace ArdPowerDetail {
/** Owns registered GPIOs and a bounded ISR latch. Call registration only from loop/setup. */
class Signals {
public:
  static constexpr uint8_t MaxPins = 4, MaxInterruptSources = 16;
  /** @brief Initialize an empty GPIO registry and ISR latch. */
  Signals() = default;
  /** @brief Prevent copying native GPIO/ISR ownership. */
  Signals(const Signals&) = delete;
  /** @brief Prevent assigning native GPIO/ISR ownership. */
  Signals& operator=(const Signals&) = delete;
  /** @brief Detach registered GPIO handlers and release the optional wait semaphore. */
  ~Signals() {
    for (auto& slot : _pins)
      if (slot.used) removePin(slot.pin);
#if defined(ESP32) && configSUPPORT_STATIC_ALLOCATION
    if (_wait) vSemaphoreDelete(_wait);
#endif
  }
  /** @brief Register a level wake GPIO; false for invalid settings, unsupported deep wake or exhausted slots. */
  bool addWakePin(uint8_t pin, ArdPowerWakeLevel level, ArdPowerWakeScope scope) {
    if (uint8_t(level) > 1 || uint8_t(scope) > 1) return false;
    if (scope == ArdPowerWakeScope::LightAndDeepSleep) {
#if defined(ESP32)
      if (!esp_sleep_is_valid_wakeup_gpio(gpio_num_t(pin))) return false;
      for (const auto& slot : _pins)
        if (slot.used && slot.deep) return false;  // One independent deep-wake pin.
#else
      return false;  // ESP8266 wakes from deep sleep through RST, not an arbitrary GPIO.
#endif
    }
    return addPin(pin, level == ArdPowerWakeLevel::Low ? ONLOW : ONHIGH, true, scope == ArdPowerWakeScope::LightAndDeepSleep);
  }
  /** @brief Register an edge GPIO event source; return whether native registration succeeded. */
  bool addInterruptPin(uint8_t pin, ArdPowerInterrupt trigger) {
    if (uint8_t(trigger) > 2) return false;
    return addPin(pin, trigger == ArdPowerInterrupt::Rising ? RISING : trigger == ArdPowerInterrupt::Falling ? FALLING
                                                                                                             : CHANGE,
                  false, false);
  }
  /** @brief Detach a GPIO, clear pending state and disable its wake source; false if not found or detach fails. */
  bool removePin(uint8_t pin) {
    for (uint8_t i = 0; i < MaxPins; ++i)
      if (_pins[i].used && _pins[i].pin == pin) {
#if defined(ESP32)
        if (_pins[i].deep) clearDeepPins();
        if (gpio_isr_handler_remove(gpio_num_t(pin)) != ESP_OK) return false;
        if (_pins[i].wake) gpio_wakeup_disable(gpio_num_t(pin));
#else
        detachInterrupt(pin);
#endif
        _pins[i].used = false;
        _held &= ~(1U << i);
        clearBit(i);
#if defined(ESP32)
        if (!hasWakePins()) esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
#endif
        return true;
      }
    return false;
  }
  /** @brief Latch a coalesced application ISR source 0–15; ignore out-of-range IDs. */
  void __attribute__((always_inline)) signalFromISR(uint8_t source) {
    if (source < MaxInterruptSources) recordFromISR(uint32_t(1) << (8 + source));
  }
  /** @brief Check the pending ISR latch without consuming it. */
  bool hasSignals() {
    return pending() != 0;
  }
  /** @brief Return whether any GPIO source is registered. */
  bool hasSources() const {
    for (const auto& slot : _pins)
      if (slot.used) return true;
    return false;
  }
  /** @brief Return whether any registered GPIO requires interrupt handling rather than wake handling. */
  bool interruptOnly() const {
    for (const auto& slot : _pins)
      if (slot.used && !slot.wake) return true;
    return false;
  }
  /** @brief Return whether a registered level wake pin is currently asserted. */
  bool wakePinsActive() const {
    for (const auto& slot : _pins)
      if (slot.used && slot.wake && active(slot)) return true;
    return false;
  }
  /** @brief Return whether every registered GPIO supports deep sleep; true when no pins exist. */
  bool deepSourcesReady() const {
    for (const auto& slot : _pins)
      if (slot.used && !slot.deep) return false;
    return true;
  }
  /** @brief Consume pending signal bits, coalescing held wake levels and rearming released pins. */
  uint32_t takeSignals() {
    uint32_t bits = takePending();
    for (uint8_t i = 0; i < MaxPins; ++i) {
      auto& slot = _pins[i];
      if (!slot.used || !slot.wake) continue;
      if (active(slot)) {
        if (!(_held & (1U << i))) bits |= 1U << i;
        else bits &= ~(1U << i);
        _held |= 1U << i;
      } else {
        _held &= ~(1U << i);
        if (slot.masked) rearm(slot);
      }
    }
    return bits;
  }
  /** @brief Map a signal bit to its GPIO/application event identity; false for unregistered bits. */
  bool describeSignal(uint8_t bit, ArdPowerEvent& event) const {
    if (bit >= 8 && bit < 24) {
      event.kind = ArdPowerEventKind::Interrupt;
      event.id = bit - 8;
      return true;
    }
    if (bit < MaxPins && _pins[bit].used) {
      event.kind = ArdPowerEventKind::Gpio;
      event.id = _pins[bit].pin;
      return true;
    }
    return false;
  }
  /** @brief Wait cooperatively for a latched signal or the requested millisecond timeout. */
  void waitForSignal(uint32_t ms) {
#if defined(ESP32) && configSUPPORT_STATIC_ALLOCATION
    if (!_wait) {
      auto semaphore = xSemaphoreCreateBinaryStatic(&_waitStorage);
      portENTER_CRITICAL(&_mux);
      _wait = semaphore;
      portEXIT_CRITICAL(&_mux);
    }
    if (hasSignals()) return;
    TickType_t ticks = pdMS_TO_TICKS(ms);
    if (ticks) xSemaphoreTake(_wait, ticks);
    else taskYIELD();
#elif defined(ESP8266)
    if (!hasSignals()) esp_delay(ms, [this]() {
      return !hasSignals();
    });
#else
    if (!hasSignals()) delay(ms);
#endif
  }
  /** @brief Arm native deep-wake GPIO after checking sources and pending events; false if unsafe or unsupported. */
  bool prepareDeepPins() {
    if (!deepSourcesReady() || hasSignals() || wakePinsActive()) return false;
#if defined(ESP32)
    for (const auto& slot : _pins)
      if (slot.used && slot.deep) {
        esp_err_t result;
#if SOC_PM_SUPPORT_EXT0_WAKEUP
        result = esp_sleep_enable_ext0_wakeup(gpio_num_t(slot.pin), slot.mode == ONHIGH ? 1 : 0);
#elif SOC_PM_SUPPORT_EXT1_WAKEUP
        result = esp_sleep_enable_ext1_wakeup(uint64_t(1) << slot.pin, slot.mode == ONHIGH ? ESP_EXT1_WAKEUP_ANY_HIGH : ESP_EXT1_WAKEUP_ANY_LOW);
#elif SOC_GPIO_SUPPORT_DEEPSLEEP_WAKEUP
        result = esp_deep_sleep_enable_gpio_wakeup(uint64_t(1) << slot.pin, slot.mode == ONHIGH ? ESP_GPIO_WAKEUP_GPIO_HIGH : ESP_GPIO_WAKEUP_GPIO_LOW);
#else
        return false;
#endif
        _deepPinArmed = result == ESP_OK;
        return _deepPinArmed;
      }
#endif
    return true;
  }
  /** @brief Disarm an armed deep-wake GPIO and restore light-wake handling as needed. */
  void clearDeepPins() {
#if defined(ESP32)
    if (!_deepPinArmed) return;
#if SOC_PM_SUPPORT_EXT0_WAKEUP
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT0);
#elif SOC_PM_SUPPORT_EXT1_WAKEUP
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT1);
#elif SOC_GPIO_SUPPORT_DEEPSLEEP_WAKEUP
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
    if (hasWakePins()) esp_sleep_enable_gpio_wakeup();
#endif
    _deepPinArmed = false;
#endif
  }

private:
  struct Pin {
    Signals* owner = nullptr;
    uint8_t pin = 0, mode = 0, index = 0;
    bool used = false, wake = false, deep = false;
    volatile bool masked = false;
  };
  Pin _pins[MaxPins];
  uint8_t _held = 0;
  volatile uint32_t _pendingSignals = 0;
#if defined(ESP32)
  bool _deepPinArmed = false;
  portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
#if configSUPPORT_STATIC_ALLOCATION
  StaticSemaphore_t _waitStorage;
  SemaphoreHandle_t _wait = nullptr;
#endif
#endif
  bool hasWakePins() const {
    for (const auto& slot : _pins)
      if (slot.used && slot.wake) return true;
    return false;
  }
  static bool active(const Pin& slot) {
    return digitalRead(slot.pin) == (slot.mode == ONHIGH ? HIGH : LOW);
  }
  bool addPin(uint8_t pin, uint8_t mode, bool wake, bool deep) {
#if defined(ESP32)
    if (!GPIO_IS_VALID_GPIO(pin)) return false;
#if defined(CONFIG_IDF_TARGET_ESP32)
    if (pin >= 6 && pin <= 11) return false;
#endif
#else
    if (pin > 15 || (pin >= 6 && pin <= 11)) return false;
#endif
    Pin* slot = nullptr;
    for (auto& entry : _pins) {
      if (entry.used && entry.pin == pin) return false;
      if (!entry.used && !slot) slot = &entry;
    }
    if (!slot) return false;
    slot->owner = this;
    slot->pin = pin;
    slot->mode = mode;
    slot->index = uint8_t(slot - _pins);
    slot->wake = wake;
    slot->deep = deep;
    slot->masked = false;
    slot->used = true;
#if defined(ESP32)
    esp_err_t result = gpio_install_isr_service(ARDUINO_ISR_FLAG);
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
      slot->used = false;
      clearBit(slot->index);
      return false;
    }
    if (gpio_set_intr_type(gpio_num_t(pin), gpio_int_type_t(mode)) != ESP_OK || gpio_isr_handler_add(gpio_num_t(pin), pinIsr, slot) != ESP_OK) {
      slot->used = false;
      clearBit(slot->index);
      return false;
    }
    if (wake && (gpio_wakeup_enable(gpio_num_t(pin), gpio_int_type_t(mode)) != ESP_OK || esp_sleep_enable_gpio_wakeup() != ESP_OK)) {
      gpio_isr_handler_remove(gpio_num_t(pin));
      gpio_wakeup_disable(gpio_num_t(pin));
      slot->used = false;
      clearBit(slot->index);
      return false;
    }
#else
    attachInterruptArg(pin, pinIsr, slot, wake ? (mode | 0x08) : mode);
#endif
    return true;
  }
  void rearm(Pin& slot) {
    slot.masked = false;
#if defined(ESP32)
    gpio_intr_enable(gpio_num_t(slot.pin));
#else
    attachInterruptArg(slot.pin, pinIsr, &slot, slot.mode | 0x08);
#endif
  }
  static void ARDPORTAL_POWER_ISR_ATTR pinIsr(void* argument);
  void ARDPORTAL_POWER_ISR_ATTR recordFromISR(uint32_t mask);
  uint32_t pending() {
#if defined(ESP32)
    portENTER_CRITICAL(&_mux);
    uint32_t bits = _pendingSignals;
    portEXIT_CRITICAL(&_mux);
#else
    uint32_t saved = xt_rsil(15);
    uint32_t bits = _pendingSignals;
    xt_wsr_ps(saved);
#endif
    return bits;
  }
  uint32_t takePending() {
#if defined(ESP32)
    portENTER_CRITICAL(&_mux);
    uint32_t bits = _pendingSignals;
    _pendingSignals = 0;
    portEXIT_CRITICAL(&_mux);
#else
    uint32_t saved = xt_rsil(15);
    uint32_t bits = _pendingSignals;
    _pendingSignals = 0;
    xt_wsr_ps(saved);
#endif
    return bits;
  }
  void clearBit(uint8_t bit) {
#if defined(ESP32)
    portENTER_CRITICAL(&_mux);
    _pendingSignals &= ~(uint32_t(1) << bit);
    portEXIT_CRITICAL(&_mux);
#else
    uint32_t saved = xt_rsil(15);
    _pendingSignals &= ~(uint32_t(1) << bit);
    xt_wsr_ps(saved);
#endif
  }
};
}
