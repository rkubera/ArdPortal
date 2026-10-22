// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
/** @brief Resume station association with persisted credentials. @return True when initiated. */
#if ARDPORTAL_ENABLE_WIFI
bool ArdPortal::connectWifiStation() {
#if !ARDPORTAL_ENABLE_WIFI
  return false;
#endif
  if (!_configurationReady || portalAndAppConfigBusy() || otaActive() || _rebootPending || mqttTrialActive() || _scanning || !_config.ssid.length()) return false;
  _wifiAutoReconnect = true;
  _wifiResult = 0;
  ++_wifiRevision;
  connectWifi();
  return true;
}
/** @brief Stop association and prevent automatic reconnection. @return True when accepted. */
bool ArdPortal::disconnectWifiStation() {
#if !ARDPORTAL_ENABLE_WIFI
  return false;
#endif
  if (!_configurationReady || portalAndAppConfigBusy() || otaActive() || _rebootPending || mqttTrialActive()) return false;
  _wifiAutoReconnect = false;
  closeMqtt();
  WiFi.disconnect(false, false);
  _attempting = false;
  _wasConnected = false;
  _wifiState = _apActive ? WifiState::FallbackAP : WifiState::Disconnected;
  _wifiResult = 2;
  ++_wifiRevision;
  return true;
}
/** @brief Queue the same asynchronous scan used by WWW. @return True when queued. */
bool ArdPortal::scanWifiNetworks() {
#if !ARDPORTAL_ENABLE_WIFI
  return false;
#endif
  if (!_configurationReady || wifiScanBusy() || otaActive() || _rebootPending || mqttTrialActive()) return false;
  _scanRequested = true;
  ++_scanId;
  return true;
}
/** @brief Apply AP policy to shared WWW/network state. @param policy Requested lifetime. @return True when applied. */
bool ArdPortal::setApPolicy(ApPolicy policy) {
#if !ARDPORTAL_ENABLE_WIFI
  (void)policy;
  return false;
#endif
  if (!_configurationReady || portalAndAppConfigBusy() || otaActive() || _rebootPending || mqttTrialActive()) return false;
  if (policy != ApPolicy::Automatic && policy != ApPolicy::AlwaysOn && policy != ApPolicy::Disabled) return false;
  if (policy == ApPolicy::Disabled && _apActive && !WiFi.softAPdisconnect(true)) return false;
  _apPolicy = policy;
  if (policy == ApPolicy::Disabled) {
    _apActive = false;
#if ARDPORTAL_ENABLE_WWW
    _dns.stop();
#endif
    WiFi.mode(WIFI_STA);
    if (!wifiConnected() && !_attempting) _wifiState = WifiState::Disconnected;
  } else if (policy == ApPolicy::AlwaysOn || !wifiConnected()) startAP();
  ++_wifiRevision;
  return policy != ApPolicy::AlwaysOn || _apActive;
}
/** @brief Deliver state and scan completion once from loop. @return No value. */
#endif
void ArdPortal::serviceNetworkApi() {
#if ARDPORTAL_ENABLE_WIFI
  if (_apiApActive != _apActive) {
    _apiApActive = _apActive;
    auto callback = _apStateCallback;
    if (callback) callback(_apActive);
  }
  if (_apiWifiState != _wifiState) {
    _apiWifiState = _wifiState;
    auto callback = _wifiStateCallback;
    if (callback) callback(_wifiState);
  }
  if (_apiScanCompletedId != _scanCompletedId) {
    _apiScanCompletedId = _scanCompletedId;
    auto callback = _wifiScanCallback;
    if (callback) callback(_scanSucceeded, _networks);
  }
#endif
#if ARDPORTAL_ENABLE_MQTT
  _mqttClient.serviceApiCallbacks();
#endif
}
