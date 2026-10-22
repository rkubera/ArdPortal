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
  if (_http || _httpConfigUpload) block(ArdPowerBlocker::Http);
#endif
#if ARDPORTAL_SUPPORT_WEBSOCKET
  if (_eventWs.client) block(ArdPowerBlocker::WebSocket);
#if ARDPORTAL_ENABLE_CONSOLE
  if (_consoleWs.client) block(ArdPowerBlocker::WebSocket);
#endif
#if ARDPORTAL_ENABLE_ZIGBEE
  if (_zigbeeWs.client) block(ArdPowerBlocker::WebSocket);
#endif
#endif
#if ARDPORTAL_ENABLE_MQTT
  if (mqttTrialActive() || _mqttClient.trialSavePending() || _mqttClient.sendBusy() || _mqttClient._rxSize || _mqttClient.mqttTransport().available() || _mqttClient._pingPending || _mqttClient._mqttState == MqttState::Connecting) block(ArdPowerBlocker::Mqtt);
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
  context.owner = this;
  context.beforeDeepSleep = [](void* owner) {
    static_cast<ArdPortal*>(owner)->closeMqtt();
  };
  _power.service(context);
}
