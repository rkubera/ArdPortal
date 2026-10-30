// SPDX-License-Identifier: MIT
#pragma once
#include "ArdPortalDeclarations.h"
#if ARDPORTAL_ENABLE_ZIGBEE
void ArdPortal::prepareZigbeePower() {
  const bool selected = _power.policy() != ArdPower::Policy::AlwaysOn && _power.sleepType() != ArdPower::SleepType::None;
  _zigbee.powerSaving(selected, selected && _power.sleepType() == ArdPower::SleepType::DeepSleep);
}
#endif
void ArdPortal::servicePower() {
  _power.dispatchEvents();
  ArdPowerDetail::Context context;
  auto block = [&](ArdPowerBlocker reason) {
    context.blockers |= uint16_t(reason);
  };
  if (!_started || !_configurationReady) block(ArdPowerBlocker::Startup);
  if (portalAndAppConfigBusy() || _saveQueued || _saveRetryPending || _httpWaitingStorage) block(ArdPowerBlocker::Storage);
  if (otaActive()) block(ArdPowerBlocker::Ota);
  if (_rebootPending || _beforeRestartRunning) block(ArdPowerBlocker::Restart);
  if (_apActive) block(ArdPowerBlocker::AccessPoint);
#if ARDPORTAL_ENABLE_WIFI
  context.wifiActive = WiFi.getMode() != WIFI_OFF;
  context.wifiConnected = WiFi.status() == WL_CONNECTED;
  if (_attempting || _scanning || _scanRequested) block(ArdPowerBlocker::Network);
#endif
#if ARDPORTAL_ENABLE_WWW
  // Keep the radio awake between the browser's consecutive asset/API requests.
  if (_http || _httpConfigUpload || (_httpSince && uint32_t(millis() - _httpSince) < 1000)) block(ArdPowerBlocker::Http);
#endif
#if ARDPORTAL_SUPPORT_WEBSOCKET
  if (_eventWs.client) block(ArdPowerBlocker::WebSocket);
#if ARDPORTAL_ENABLE_MATTER_CONSOLE
  if (_matterWs.client) block(ArdPowerBlocker::WebSocket);
#endif
#if ARDPORTAL_ENABLE_CONSOLE && (ARDPORTAL_ENABLE_MQTT || ARDPORTAL_ENABLE_CONSOLE_MESSAGES)
  if (_consoleWs.client) block(ArdPowerBlocker::WebSocket);
#endif
#if ARDPORTAL_ENABLE_ZIGBEE
  if (_zigbeeWs.client) block(ArdPowerBlocker::WebSocket);
#endif
#endif
#if ARDPORTAL_ENABLE_MQTT
  if (mqttTrialActive() || _mqttClient.trialSavePending() || _mqttClient.sendBusy() || _mqttClient.receiveBusy() || _mqttClient.mqttTransport().available() || _mqttClient._mqttState == MqttState::Connecting) block(ArdPowerBlocker::Mqtt);
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  if (_config.host.length() && (_appControls._stateDirty || _appControls._mqttAckPending || _appControls._mqttAckInFlight)) block(ArdPowerBlocker::Mqtt);
#endif
#if ARDPORTAL_ENABLE_HA
  if (_config.host.length() && _homeAssistant._haDiscovery < _dynamic.count()) block(ArdPowerBlocker::Mqtt);
#endif
#endif
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  if (appConfigPageRegistrationBusy()) block(ArdPowerBlocker::Application);
#endif
#if ARDPORTAL_ENABLE_ZIGBEE
  context.zigbee = true;
  const bool zigbeeDeepSleep = _power.policy() != ArdPower::Policy::AlwaysOn && _power.sleepType() == ArdPower::SleepType::DeepSleep;
  const bool zigbeeRadioSleep = _power.policy() != ArdPower::Policy::AlwaysOn && (_power.sleepType() == ArdPower::SleepType::ConnectedSleep || _power.sleepType() == ArdPower::SleepType::LightSleep) && !otaActive();
  context.zigbeeSleeping = _zigbee.powerSaving(zigbeeRadioSleep || zigbeeDeepSleep, zigbeeDeepSleep);
  context.zigbeeDeepSleep = _zigbee.canDeepSleep();
  context.prepareDeepSleep = [](void* owner) {
    return static_cast<ArdPortal*>(owner)->_zigbee.prepareDeepSleep();
  };
  context.afterDeepSleepAttempt = [](void* owner) {
    static_cast<ArdPortal*>(owner)->_zigbee.cancelDeepSleep();
  };
#endif
#if ARDPORTAL_ENABLE_MATTER
#if !ARDPORTAL_MATTER_THREAD || (defined(CONFIG_ENABLE_ICD_SERVER) && CONFIG_ENABLE_ICD_SERVER)
  const bool matterSleep = _power.policy() != ArdPower::Policy::AlwaysOn && _power.sleepType() == ArdPower::SleepType::ConnectedSleep;
  context.managedRadioSleeping = matterSleep && _matter.connected();
#endif
  context.connectedSleepAdmission = [](void* owner, bool admitted) {
    auto* portal = static_cast<ArdPortal*>(owner);
    const bool released = portal->_matter.powerSaving(admitted, !admitted);
#if ARDPORTAL_ENABLE_CONSOLE_MESSAGES
    constexpr bool supported = !ARDPORTAL_MATTER_THREAD
#if defined(CONFIG_ENABLE_ICD_SERVER) && CONFIG_ENABLE_ICD_SERVER
                               || true
#endif
      ;
    String diagnostic = portal->_powerDiagnostics.update(portal->_power.policy(), portal->_power.sleepType(), portal->_power.blockers(), admitted, released, portal->_matter.connected(), supported);
    if (diagnostic.length()) portal->consoleLine(0, diagnostic);
#else
    (void)released;
#endif
  };
  // Never force light/deep sleep underneath CHIP. ConnectedSleep uses SDK/RTOS idle.
  if (!context.managedRadioSleeping) block(ArdPowerBlocker::Network);
#endif
  context.owner = this;
  context.beforeDeepSleep = [](void* owner) {
    static_cast<ArdPortal*>(owner)->closeMqtt();
  };
  _power.service(context);
}
