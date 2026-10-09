// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "AppConfigFields.h"
#include "ArdPortalFeatures.h"
#if ARDPORTAL_ENABLE_HA
#include <Arduino.h>
#include "ArdJSON.h"
#include "DependencyConditions.h"
class ArdPortal;

// Home Assistant discovery, availability and broker birth-message handling.
class ArdHomeAssistant {
public:
  /**
   * @brief Disallow copying or moving this resource-owning instance.
   * Input: const ArdHomeAssistant&.
   * @return No result; this operation is deleted and cannot be called.
   */
  ArdHomeAssistant(const ArdHomeAssistant&) = delete;
  /**
   * @brief Replace this instance state with the supplied value.
   * Input: const ArdHomeAssistant&.
   * @return No result; this operation is deleted and cannot be called.
   */
  ArdHomeAssistant& operator=(const ArdHomeAssistant&) = delete;
  /**
   * @brief Build the Home Assistant MQTT discovery document for an entity.
   * @param field Application field definition or identifier.
   * @param error Output error text; populated when the operation fails.
   * @return The resulting text; an empty value indicates no available text or failure where applicable.
   */
  String discoveryConfig(const ArdJSON::JSONVar& field,String* error=nullptr) const;
private:
  friend class ArdPortal;
  friend class ArdMqtt;
  friend class ArdAppControls;
  /**
   * @brief Initialize this instance and its owned state.
   * @param portal Owning portal instance.
   * @return No value.
   */
  explicit ArdHomeAssistant(ArdPortal& portal) : _portal(portal) {}
  ArdPortal& _portal;
  uint32_t _haSince=0,_discoveryRetryAt=0;
  bool _discoveryRetryPending=false;
#if ARDPORTAL_ENABLE_DEPENDENCIES
  uint32_t _dependencyRevision=0;
#endif
  uint16_t _haDiscovery=0;
  bool _haOnline=false;
  uint8_t _serviceStage=0;
#if ARDPORTAL_ENABLE_DEPENDENCIES
  ArdAppConfigFieldMask _dependencyMask=0, _dependencyDirty=0, _dependencyKnown=0, _dependencyVisible=0, _dependencyDiscovery=0;
#endif
  /**
   * @brief Restart the Home Assistant discovery publication cursor.
   * @return No value.
   */
  void resetDiscovery();
  /**
   * @brief Check whether the specified page or field has been registered.
   * @param first Whether to initialize the incremental publication pass.
   * @return No value.
   */
  void registered(size_t first);
  /**
   * @brief Check whether the specified page or field has been registered.
   * @param first Whether to initialize the incremental publication pass.
   * @param states Mask of application states to publish.
   * @param dependencies Mask of dependency fields requiring work.
   * @return No value.
   */
  void registered(size_t first,const ArdAppConfigFieldMask& states,const ArdAppConfigFieldMask& dependencies);
  /**
   * @brief Perform one pending Home Assistant publication or dependency work unit.
   * @param now Current time used to evaluate deadlines.
   * @return True if a work unit was performed; false if there is no eligible work.
   */
  bool serviceOne(uint32_t now);
#if ARDPORTAL_ENABLE_DEPENDENCIES
  /**
   * @brief Invalidate only direct and transitive dependents of a changed controller.
   * @param key Changed application field ID.
   * @return No value; unrelated telemetry does not disturb traversal.
   */
  void dependencyChanged(const String& key);
  uint32_t _dependencyRetryAt=0;
  bool _dependencyRetryPending=false;
  using DependencyFrame = ArdDependencies::Frame;
  ArdDependencies::Stack _walk;
  size_t _walkDepth=0,_walkIndex=0;
  ArdAppConfigFieldMask _walkActive=0,_walkComplete=0,_walkVisible=0;
  /**
   * @brief Advance dependency-driven visibility and state changes.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool serviceDependencies();
  /**
   * @brief Finish the current dependency visibility transition.
   * @param visible Whether the field or page should be visible.
   * @return No value.
   */
  void completeDependency(bool visible);
#endif
  /**
   * @brief Publish the next pending Home Assistant discovery entry within the work budget.
   * @param now Current time used to evaluate deadlines.
   * @return No value.
   */
  void serviceDiscovery(uint32_t now);
  /**
   * @brief Handle the Home Assistant birth/status topic and schedule discovery when HA comes online.
   * @param topic MQTT topic to publish, subscribe or match.
   * @param data Data buffer or value used by the operation.
   * @param size Number of bytes or elements.
   * @return True if the message belongs to the Home Assistant status topic; false otherwise.
   */
  bool handleStatus(const String& topic, const uint8_t* data, size_t size);
};
#endif
