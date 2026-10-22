#pragma once
inline int coexistResult = ESP_OK;
inline unsigned coexistCalls = 0;
/** @brief Simulate radio coexistence setup. @return Configured SDK result. */
inline esp_err_t esp_coex_wifi_i154_enable() {
  ++coexistCalls;
  coexistEnabled = coexistResult == ESP_OK;
  return coexistResult;
}
