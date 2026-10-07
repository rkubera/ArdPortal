// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "AppConfigFields.h"
#include "ArdPortalFeatures.h"
#include "PortalTypes.h"
#include "ArdJSON.h"
class ArdPortal;

// Local dynamic forms and callbacks are independent of HA discovery.
class ArdAppControls {
private:
  friend class ArdPortal;
  friend class ArdMqtt;
  friend class ArdHomeAssistant;
  explicit ArdAppControls(ArdPortal& portal) : _portal(portal) {}
  ArdPortal& _portal;
  using ChangeSource = ArdPortalChangeSource;
  ArdJSON::JSONVar _appState = ArdJSON::JSONVar::object();
  uint8_t _appControlStage=0;
  const ArdJSON::JSONVar& stateValues() const { return _appState; }
  uint8_t controlStage() const { return _appControlStage; }
  void clearControlStage() { _appControlStage=0; }
#if ARDPORTAL_ENABLE_DEPENDENCIES
  bool dependencyMatches(const ArdJSON::JSONVar& rule) const;
  bool dependencyValueMatches(const ArdJSON::JSONVar& rule) const;
  bool dependencyFieldVisible(size_t index) const;
#endif
  bool appFieldVisible(const ArdJSON::JSONVar& field) const;
  void applyAppConfig(ArdJSON::JSONVar app);
  bool appControl(const ArdJSON::JSONVar& field, size_t control, const ArdJSON::JSONVar& value, ChangeSource source);
  bool applyAppState(const String& key, const ArdJSON::JSONVar& value, ChangeSource source, bool publishMqtt = true);
  bool setAppConfigStateValue(const char* key, const ArdJSON::JSONVar& value, bool publishMqtt);
  bool emitAppConfigEvent(const char* key, const ArdJSON::JSONVar& value);
  void markDirty(const String& key);
#if ARDPORTAL_ENABLE_MQTT
  ArdAppConfigFieldMask _stateDirty=0, _mqttAckPending=0, _mqttAckInFlight=0;
  uint32_t _stateSince=0;
  uint8_t _subscriptions=0;
  bool _yieldPending=false;
  ArdJSON::JSONVar _mqttAppQueue = ArdJSON::JSONVar::object();
#if ARDPORTAL_CONTROL_SUPPORT_EMISSIONS
  struct AppEmission { String topic, payload; };
  AppEmission _appEmissions[8];
  uint8_t _emissionHead=0, _emissionCount=0;
#endif
  ArdAppConfigFieldMask stateMask() const;
  bool queueAppEmission(const String& topic, const String& payload);
  void resetMqtt();
  bool canServiceMqtt() const;
  bool acceptMqttState(const String& key, ArdJSON::JSONVar value);
  void prepareMqtt(uint32_t now);
  bool publishCommands();
  bool publishState();
  void serviceMqttValues(uint32_t now);
  void dynamicMqtt(const String& topic, const uint8_t* data, size_t size, bool retained);
  void acknowledgeSave(bool saved);
  void disconnected();
  void yieldAfterPublish();
#endif
};
