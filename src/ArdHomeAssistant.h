// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "AppConfigFields.h"
#include "ArdPortalFeatures.h"
#if ARDPORTAL_ENABLE_HA
#include <Arduino.h>
#include "ArdJSON.h"
class ArdPortal;

// Home Assistant discovery, availability and broker birth-message handling.
class ArdHomeAssistant {
public:
  ArdHomeAssistant(const ArdHomeAssistant&) = delete;
  ArdHomeAssistant& operator=(const ArdHomeAssistant&) = delete;
  String discoveryConfig(const ArdJSON::JSONVar& field,String* error=nullptr) const;
private:
  friend class ArdPortal;
  friend class ArdMqtt;
  explicit ArdHomeAssistant(ArdPortal& portal) : _portal(portal) {}
  ArdPortal& _portal;
  uint32_t _haSince=0;
#if ARDPORTAL_ENABLE_DEPENDENCIES
  uint32_t _dependencyRevision=0;
#endif
  uint16_t _haDiscovery=0;
  bool _haOnline=false;
  uint8_t _serviceStage=0;
#if ARDPORTAL_ENABLE_DEPENDENCIES
  ArdAppConfigFieldMask _dependencyMask=0, _dependencyDirty=0, _dependencyKnown=0, _dependencyVisible=0, _dependencyDiscovery=0;
#endif
  void resetDiscovery();
  void registered(size_t first);
  void registered(size_t first,const ArdAppConfigFieldMask& states,const ArdAppConfigFieldMask& dependencies);
  bool serviceOne(uint32_t now);
#if ARDPORTAL_ENABLE_DEPENDENCIES
  struct DependencyFrame {uint16_t index;uint8_t edge;};
  std::unique_ptr<DependencyFrame[]> _walk;
  size_t _walkDepth=0,_walkIndex=0;
  ArdAppConfigFieldMask _walkActive=0,_walkComplete=0;
  bool serviceDependencies();
  void completeDependency(bool visible);
#endif
  void serviceDiscovery(uint32_t now);
  bool handleStatus(const String& topic, const uint8_t* data, size_t size);
};
#endif
