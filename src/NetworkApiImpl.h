// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#if ARDPORTAL_ENABLE_WIFI
bool ArdPortal::suspendPortal() {
#if ARDPORTAL_ENABLE_MATTER && !ARDPORTAL_MATTER_THREAD
  // Wi-Fi is the Matter transport, including before commissioning finishes.
  return false;
#else
  if (_portalSuspended) return true;
  if (!_configurationReady || portalAndAppConfigBusy() || otaActive() || _rebootPending || mqttTrialActive() || wifiScanBusy()) return false;
  // Commit suspension only after the driver accepts radio shutdown.
  if (!WiFi.mode(WIFI_OFF)) return false;
  _resumeWifiAutoReconnect = _wifiAutoReconnect;
  _wifiAutoReconnect = false;
  _portalSuspended = true;
  closeMqtt();
#if ARDPORTAL_ENABLE_WWW
  closeHttp();
  _server.stop();
  _dns.stop();
#endif
#if ARDPORTAL_SUPPORT_WEBSOCKET
  closeWebSocket(_eventWs);
#if ARDPORTAL_ENABLE_MATTER
  closeWebSocket(_matterWs);
#endif
#if ARDPORTAL_ENABLE_ZIGBEE
  closeWebSocket(_zigbeeWs);
#endif
#if ARDPORTAL_ENABLE_CONSOLE && (ARDPORTAL_ENABLE_MQTT || ARDPORTAL_ENABLE_CONSOLE_MESSAGES)
  closeWebSocket(_consoleWs);
#endif
#endif
  _apActive = _attempting = _wasConnected = false;
  _wifiState = WifiState::Disconnected;
  _wifiResult = 2;
  ++_wifiRevision;
  return true;
#endif
}

bool ArdPortal::resumePortal() {
  if (!_portalSuspended) return _configurationReady;
  if (_rebootPending || otaActive() || portalAndAppConfigBusy()) return false;
  if (!WiFi.mode(WIFI_STA)) return false;
  _portalSuspended = false;
  _wifiAutoReconnect = _resumeWifiAutoReconnect;
  _retrySince = millis();
  _wifiResult = 0;
#if ARDPORTAL_ENABLE_WWW
  _server.begin();
#endif
  if (_apPolicy == ApPolicy::AlwaysOn || (_apPolicy == ApPolicy::Automatic && (!_wifiAutoReconnect || !_config.ssid.length()))) startAP();
  if (_wifiAutoReconnect && _config.ssid.length()) connectWifi();
  ++_wifiRevision;
  return true;
}
#endif

/** @brief Resume station association with persisted credentials. @return True when initiated. */
#if ARDPORTAL_ENABLE_WIFI
bool ArdPortal::connectWifiStation() {
#if ARDPORTAL_ENABLE_MATTER && !ARDPORTAL_MATTER_THREAD
  if (_matter.started()) return false;
#endif
#if !ARDPORTAL_ENABLE_WIFI
  return false;
#endif
  if (_portalSuspended || !_configurationReady || portalAndAppConfigBusy() || otaActive() || _rebootPending || mqttTrialActive() || _scanning || !_config.ssid.length()) return false;
  _wifiAutoReconnect = true;
  _wifiResult = 0;
  ++_wifiRevision;
  connectWifi();
  return true;
}
/** @brief Stop association and prevent automatic reconnection. @return True when accepted. */
bool ArdPortal::disconnectWifiStation() {
#if ARDPORTAL_ENABLE_MATTER && !ARDPORTAL_MATTER_THREAD
  if (_matter.started()) return false;
#endif
#if !ARDPORTAL_ENABLE_WIFI
  return false;
#endif
  if (_portalSuspended || !_configurationReady || portalAndAppConfigBusy() || otaActive() || _rebootPending || mqttTrialActive()) return false;
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
  if (_portalSuspended || !_configurationReady || wifiScanBusy() || otaActive() || _rebootPending || mqttTrialActive()) return false;
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
  if (_portalSuspended || !_configurationReady || portalAndAppConfigBusy() || otaActive() || _rebootPending || mqttTrialActive()) return false;
  if (policy != ApPolicy::Automatic && policy != ApPolicy::AlwaysOn && policy != ApPolicy::Disabled) return false;
  if (policy == ApPolicy::Disabled && _apActive && !WiFi.enableAP(false)) return false;
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
