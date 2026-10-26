// SPDX-License-Identifier: MIT
#pragma once
#include "ArdPortalFeatures.h"
#define ARDPORTAL_POWER_ISR_ATTR IRAM_ATTR
#include "PowerPolicy.h"
#if ARDPORTAL_ENABLE_WIFI
#if defined(ESP32)
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#endif
#if defined(ESP32)
#include <esp_sleep.h>
#include <esp_system.h>
#include <sdkconfig.h>
#if defined(CONFIG_PM_ENABLE) && CONFIG_PM_ENABLE && defined(CONFIG_FREERTOS_USE_TICKLESS_IDLE) && CONFIG_FREERTOS_USE_TICKLESS_IDLE
#include <esp_pm.h>
#define ARDPORTAL_POWER_AUTO_LIGHT_SLEEP 1
#else
#define ARDPORTAL_POWER_AUTO_LIGHT_SLEEP 0
#endif
#endif

#if defined(ESP8266)
extern "C" {
#include <user_interface.h>
}
#endif
#include "PowerSignals.h"

namespace ArdPowerDetail {
class Platform : public Signals {
public:
  /** @brief Capture the immutable native boot wake cause. */
  Platform()
    : _wakeCause(readWakeCause()) {}
  /** @brief Return the boot wake cause captured during construction. */
  ArdPowerWakeCause wakeCause() const {
    return _wakeCause;
  }
  /** @brief Return the wrapping millisecond clock used by power scheduling. */
  static uint32_t now() {
    return millis();
  }
  /** @brief Validate the platform timer range for a deep-sleep duration in milliseconds. */
  static bool validDeepSleep(uint32_t ms) {
#if defined(ESP8266)
    return uint64_t(ms) * 1000ULL <= ESP.deepSleepMax();
#else
    return ms != 0;
#endif
  }
  /** @brief Restore native power-management settings before releasing the backend. */
  ~Platform() {
    restore();
  }
  /** @brief Check native support for the requested sleep type. */
  bool supportsSleep(ArdPowerSleepType type) const {
#if defined(ESP8266)
    return type != ArdPowerSleepType::LightSleep;
#else
    (void)type;
    return true;
#endif
  }
  /** @brief Apply native connected-saving settings and return the resulting state or Error. */
  ArdPowerState apply(ArdPowerSleepType type, bool wifiActive, bool connected) {
    using State = ArdPowerState;
    using Sleep = ArdPowerSleepType;
    bool light = false;
#if defined(ESP32) && ARDPORTAL_POWER_AUTO_LIGHT_SLEEP
    if (connected) {
      if (!_pmOwned) {
        if (esp_pm_get_configuration(&_previousPm) != ESP_OK) return State::Error;
        esp_pm_config_t config = _previousPm;
        config.max_freq_mhz = getCpuFrequencyMhz();
        config.min_freq_mhz = config.max_freq_mhz;
        config.light_sleep_enable = true;
        if (esp_pm_configure(&config) != ESP_OK) return State::Error;
        _pmOwned = true;
      }
      light = true;
    } else if (!restorePm()) return State::Error;
#elif defined(ESP8266)
    light = connected;
#endif
    (void)connected;
#if ARDPORTAL_ENABLE_WIFI
    if (wifiActive) {
#if defined(ESP32)
      if (!_wifiOwned) {
        _previousWifi = WiFi.getSleep();
        _wifiOwned = true;
      }
      auto desired = type == Sleep::None ? WIFI_PS_NONE : WIFI_PS_MIN_MODEM;
      if (WiFi.getSleep() != desired && !WiFi.setSleep(desired)) return State::Error;
#else
      if (!_wifiOwned) {
        _previousWifi = WiFi.getSleepMode();
        _previousListen = WiFi.getListenInterval();
        _wifiOwned = true;
      }
      auto desired = type == Sleep::None ? WIFI_NONE_SLEEP : (light ? WIFI_LIGHT_SLEEP : WIFI_MODEM_SLEEP);
      if ((WiFi.getSleepMode() != desired || WiFi.getListenInterval() != 0) && !WiFi.setSleepMode(desired)) return State::Error;
#endif
    }
#endif
    if (type == Sleep::None || (!wifiActive && !connected) || (type == Sleep::LightSleep && !connected)) return State::Awake;
    if (type == Sleep::DeepSleep) return State::ModemSleep;
    if (!connected) return State::ModemSleep;
    return light ? (type == Sleep::LightSleep ? State::LightSleep : State::ConnectedSleep) : State::ModemSleepFallback;
  }
  /** @brief Yield while waiting for a signal or the bounded idle timeout. */
  void idle(uint32_t ms) {
    waitForSignal(ms);
  }
  /** @brief Enter timed native light sleep when supported and report the resulting state. */
  ArdPowerState lightSleep(uint32_t ms) {
#if defined(ESP32)
    if (hasSignals() || wakePinsActive()) return ArdPowerState::Awake;
#if ARDPORTAL_POWER_AUTO_LIGHT_SLEEP
    esp_pm_config_t before;
    if (esp_pm_get_configuration(&before) != ESP_OK) return ArdPowerState::Error;
    esp_pm_config_t paused = before;
    paused.light_sleep_enable = false;
    if (esp_pm_configure(&paused) != ESP_OK) return ArdPowerState::Error;
#endif
    esp_err_t result = esp_sleep_enable_timer_wakeup(uint64_t(ms) * 1000ULL);
    if (result == ESP_OK) {
      result = esp_light_sleep_start();
      esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    }
#if ARDPORTAL_POWER_AUTO_LIGHT_SLEEP
    if (esp_pm_configure(&before) != ESP_OK) return ArdPowerState::Error;
#endif
    if (result == ESP_ERR_SLEEP_REJECT) return ArdPowerState::Awake;
    return result == ESP_OK ? ArdPowerState::LightSleep : ArdPowerState::Error;
#else
    (void)ms;
    return ArdPowerState::Error;
#endif
  }
  /** @brief Prepare sources and enter native deep sleep; return false on failure. Successful entry restarts the application on wake. */
  bool deepSleep(uint32_t ms) {
    if (!prepareDeepPins()) return false;
#if defined(ESP32) && ARDPORTAL_POWER_AUTO_LIGHT_SLEEP
    // Automatic light-sleep owns the timer wake source. Pause it before assigning
    // the explicit deep-sleep deadline; restore it if admission fails.
    esp_pm_config_t before;
    if (esp_pm_get_configuration(&before) != ESP_OK) {
      clearDeepPins();
      return false;
    }
    esp_pm_config_t paused = before;
    paused.light_sleep_enable = false;
    if (esp_pm_configure(&paused) != ESP_OK) {
      clearDeepPins();
      return false;
    }
#endif
#if defined(ESP32)
    if (esp_sleep_enable_timer_wakeup(uint64_t(ms) * 1000ULL) != ESP_OK) {
#if ARDPORTAL_POWER_AUTO_LIGHT_SLEEP
      esp_pm_configure(&before);
#endif
      clearDeepPins();
      return false;
    }
#endif
#if ARDPORTAL_ENABLE_WIFI
    if (!WiFi.mode(WIFI_OFF)) {
#if defined(ESP32)
      esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
#if ARDPORTAL_POWER_AUTO_LIGHT_SLEEP
      esp_pm_configure(&before);
#endif
#endif
      clearDeepPins();
      return false;
    }
#endif
#if defined(ESP32)
    esp_deep_sleep_start();
#else
    ESP.deepSleep(uint64_t(ms) * 1000ULL);
#endif
    clearDeepPins();
    return false;  // A successful deep sleep never returns.
  }
private:
  const ArdPowerWakeCause _wakeCause;
  static ArdPowerWakeCause readWakeCause() {
    using Cause = ArdPowerWakeCause;
#if defined(ESP32)
    // Only interpret sleep wake sources after a deep-sleep reset. Subsequent
    // light sleep can change the SDK value, so snapshot it during construction.
    switch (esp_reset_reason()) {
      case ESP_RST_POWERON: return Cause::PowerOn;
      case ESP_RST_EXT: return Cause::ExternalReset;
      case ESP_RST_SW: return Cause::SoftwareReset;
      case ESP_RST_PANIC: return Cause::Panic;
      case ESP_RST_INT_WDT:
      case ESP_RST_TASK_WDT:
      case ESP_RST_WDT: return Cause::Watchdog;
      case ESP_RST_BROWNOUT: return Cause::Brownout;
      case ESP_RST_DEEPSLEEP:
        switch (esp_sleep_get_wakeup_cause()) {
          case ESP_SLEEP_WAKEUP_TIMER: return Cause::Timer;
          case ESP_SLEEP_WAKEUP_EXT0:
          case ESP_SLEEP_WAKEUP_EXT1:
          case ESP_SLEEP_WAKEUP_GPIO: return Cause::Gpio;
          case ESP_SLEEP_WAKEUP_UNDEFINED: return Cause::DeepSleep;
          default: return Cause::OtherWake;
        }
      case ESP_RST_UNKNOWN: return Cause::Unknown;
      default: return Cause::OtherReset;
    }
#else
    const auto* info = ESP.getResetInfoPtr();
    if (!info) return Cause::Unknown;
    switch (info->reason) {
      case REASON_DEFAULT_RST: return Cause::PowerOn;
      case REASON_WDT_RST:
      case REASON_SOFT_WDT_RST: return Cause::Watchdog;
      case REASON_EXCEPTION_RST: return Cause::Panic;
      case REASON_SOFT_RESTART: return Cause::SoftwareReset;
      // RST is shared by the timer wire and external circuitry: do not guess
      // Timer/Gpio when this SDK only reports a generic deep-sleep wake.
      case REASON_DEEP_SLEEP_AWAKE: return Cause::DeepSleep;
      case REASON_EXT_SYS_RST: return Cause::ExternalReset;
      default: return Cause::Unknown;
    }
#endif
  }
#if defined(ESP32) && ARDPORTAL_POWER_AUTO_LIGHT_SLEEP
  esp_pm_config_t _previousPm{};
  bool _pmOwned = false;
  bool restorePm() {
    if (!_pmOwned) return true;
    if (esp_pm_configure(&_previousPm) != ESP_OK) return false;
    _pmOwned = false;
    return true;
  }
#endif
#if ARDPORTAL_ENABLE_WIFI
  bool _wifiOwned = false;
#if defined(ESP32)
  wifi_ps_type_t _previousWifi = WIFI_PS_NONE;
#else
  WiFiSleepType_t _previousWifi = WIFI_NONE_SLEEP;
  uint8_t _previousListen = 0;
#endif
#endif
  bool restore() {
    bool ok = true;
#if defined(ESP32) && ARDPORTAL_POWER_AUTO_LIGHT_SLEEP
    if (!restorePm()) ok = false;
#endif
#if ARDPORTAL_ENABLE_WIFI
    if (_wifiOwned && WiFi.getMode() != WIFI_OFF) {
#if defined(ESP32)
      if (WiFi.setSleep(_previousWifi)) _wifiOwned = false;
      else ok = false;
#else
      if (WiFi.setSleepMode(_previousWifi, _previousListen)) _wifiOwned = false;
      else ok = false;
#endif
    }
#endif
    return ok;
  }
};
}
using ArdPower = ArdPowerDetail::Manager<ArdPowerDetail::Platform>;
