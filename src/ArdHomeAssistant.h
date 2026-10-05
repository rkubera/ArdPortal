// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
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
  String discoveryConfig(const ArdJSON::JSONVar& field) const;
private:
  friend class ArdPortal;
  friend class ArdMqtt;
  explicit ArdHomeAssistant(ArdPortal& portal) : _portal(portal) {}
  ArdPortal& _portal;
  uint32_t _haSince=0;
#if ARDPORTAL_ENABLE_DEPENDENCIES
  uint32_t _dependencyRevision=0;
#endif
  uint8_t _haDiscovery=0;
  bool _haOnline=false;
#if ARDPORTAL_ENABLE_DEPENDENCIES
  uint64_t _dependencyMask=0, _dependencyDirty=0, _dependencyKnown=0, _dependencyVisible=0, _dependencyDiscovery=0;
#endif
  void resetDiscovery();
#if ARDPORTAL_ENABLE_DEPENDENCIES
  void serviceDependencies();
#endif
  void serviceDiscovery(uint32_t now);
  bool handleStatus(const String& topic, const uint8_t* data, size_t size);
};
#endif
