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
  /**
   * @brief Initialize this instance and its owned state.
   * @param portal Owning portal instance.
   * @return No value.
   */
  explicit ArdAppControls(ArdPortal& portal) : _portal(portal) {}
  ArdPortal& _portal;
  using ChangeSource = ArdPortalChangeSource;
  ArdJSON::JSONVar _appState = ArdJSON::JSONVar::object();
  uint8_t _appControlStage=0;
  /**
   * @brief Expose the current runtime application values.
   * @return Reference to the requested stored value or component.
   */
  const ArdJSON::JSONVar& stateValues() const { return _appState; }
  /**
   * @brief Resolve the action stage for an application control.
   * @return The action stage for an application control.
   */
  uint8_t controlStage() const { return _appControlStage; }
  /**
   * @brief Discard the pending application action stage.
   * @return No value.
   */
  void clearControlStage() { _appControlStage=0; }
#if ARDPORTAL_ENABLE_DEPENDENCIES
  /**
   * @brief Evaluate a visibility dependency against the current application values.
   * @param rule Dependency rule to evaluate.
   * @return True when the referenced field is visible and its value matches the rule.
   */
  bool dependencyMatches(const ArdJSON::JSONVar& rule) const;
  /**
   * @brief Compare the referenced application value or property with the dependency expectation.
   * @param rule Dependency rule to evaluate.
   * @return True when the selected value matches the rule; false otherwise.
   */
  bool dependencyValueMatches(const ArdJSON::JSONVar& rule) const;
  /**
   * @brief Evaluate field visibility through the indexed dependency chain.
   * @param index Zero-based element or field index.
   * @return True when the complete dependency chain permits visibility; false otherwise.
   */
  bool dependencyFieldVisible(size_t index) const;
#endif
  /**
   * @brief Evaluate the field visibility condition against current application values.
   * @param field Application field definition or identifier.
   * @return True when the field is visible; false when its dependency condition is not satisfied.
   */
  bool appFieldVisible(const ArdJSON::JSONVar& field) const;
  /**
   * @brief Validate and apply an incoming application configuration change.
   * @param app Application configuration object.
   * @return No value.
   */
  void applyAppConfig(ArdJSON::JSONVar app);
  void applyAppPatch(ArdJSON::JSONVar patch);
  /**
   * @brief Validate and dispatch an application control action to the registered command handler.
   * @param field Application field definition or identifier.
   * @param control Control index within the field, if applicable.
   * @param value Input value, or output destination when passed by mutable reference.
   * @param source Input source or origin of a configuration change, as indicated by its type.
   * @return True if the action was accepted; false for an invalid, hidden or unsupported action.
   */
  bool appControl(const ArdJSON::JSONVar& field, size_t control, const ArdJSON::JSONVar& value, ChangeSource source);
  /**
   * @brief Validate and apply an incoming runtime application state change.
   * @param key Configuration key or JSON object member name.
   * @param value Input value, or output destination when passed by mutable reference.
   * @param source Input source or origin of a configuration change, as indicated by its type.
   * @param publishMqtt Whether to queue MQTT state publication for this update.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool applyAppState(const String& key, const ArdJSON::JSONVar& value, ChangeSource source, bool publishMqtt = true);
  /**
   * @brief Update a runtime application value and optionally queue its MQTT state.
   * @param key Configuration key or JSON object member name.
   * @param value Input value, or output destination when passed by mutable reference.
   * @param publishMqtt Whether to queue MQTT state publication for this update.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool setAppConfigStateValue(const char* key, const ArdJSON::JSONVar& value, bool publishMqtt);
  /**
   * @brief Emit a transient application event through the field MQTT topic.
   * @param key Configuration key or JSON object member name.
   * @param value Input value, or output destination when passed by mutable reference.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool emitAppConfigEvent(const char* key, const ArdJSON::JSONVar& value);
  /**
   * @brief Mark affected entities or fields for incremental publication.
   * @param key Configuration key or JSON object member name.
   * @return No value.
   */
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
  /**
   * @brief Expose the field mask used for pending state publication.
   * @return The field mask used for pending state publication.
   */
  ArdAppConfigFieldMask stateMask() const;
  /**
   * @brief Queue a transient application MQTT message in the bounded emission buffer.
   * @param topic MQTT topic to publish, subscribe or match.
   * @param payload Message bytes or text to send or decode.
   * @return True if queued; false when MQTT is unavailable or the bounded queue cannot accept it.
   */
  bool queueAppEmission(const String& topic, const String& payload);
  /**
   * @brief Reset subscription and publication cursors for the next MQTT connection.
   * @return No value.
   */
  void resetMqtt();
  /**
   * @brief Check whether MQTT is connected and portal operations permit application publication.
   * @return True if application MQTT work may proceed; false otherwise.
   */
  bool canServiceMqtt() const;
  /**
   * @brief Apply an incoming MQTT state value to its application field.
   * @param key Configuration key or JSON object member name.
   * @param value Input value, or output destination when passed by mutable reference.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool acceptMqttState(const String& key, ArdJSON::JSONVar value);
  /**
   * @brief Apply the MQTT endpoint and identity settings before connection.
   * @param now Current time used to evaluate deadlines.
   * @return No value.
   */
  void prepareMqtt(uint32_t now);
  /**
   * @brief Publish application command-topic subscriptions incrementally.
   * @return True when the subscription pass advances or is complete; false when it cannot proceed.
   */
  bool publishCommands();
  /**
   * @brief Publish the next pending application state within the cooperative publication budget.
   * @return True if publication work was performed or advanced; false if it could not proceed.
   */
  bool publishState();
  /**
   * @brief Publish pending application state within the cooperative work budget.
   * @param now Current time used to evaluate deadlines.
   * @return No value.
   */
  void serviceMqttValues(uint32_t now);
  /**
   * @brief Route an incoming MQTT application command.
   * @param topic MQTT topic to publish, subscribe or match.
   * @param data Data buffer or value used by the operation.
   * @param size Number of bytes or elements.
   * @param retained Whether the MQTT publication is retained.
   * @return No value.
   */
  void dynamicMqtt(const String& topic, const uint8_t* data, size_t size, bool retained);
  /**
   * @brief Publish the application state after a successful accepted update.
   * @param saved Result of the persistent save operation.
   * @return No value.
   */
  void acknowledgeSave(bool saved);
  /**
   * @brief Record a closed MQTT transport and arrange reconnection.
   * @return No value.
   */
  void disconnected();
  /**
   * @brief Yield after publishing an MQTT discovery or state message.
   * @return No value.
   */
  void yieldAfterPublish();
#endif
};
