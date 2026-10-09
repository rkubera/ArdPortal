// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ArdPortalFeatures.h"
#if ARDPORTAL_ENABLE_MQTT
#include <Arduino.h>
#include "PortalTypes.h"
class ArdPortal;
#include <functional>
#include <memory>
#if ARDPORTAL_ENABLE_MQTT_TLS
#include <WiFiClientSecure.h>
#endif
#if defined(ESP32)
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif

#include "MqttTransport.h"
#include "ArdAllocation.h"

// Owns the MQTT protocol, transport, TLS session and connection trial.
class ArdMqtt {
public:
  using PortalConfig = ArdPortalConfig;
  using MqttState = ArdMqttState;
  using ChangeSource = ArdPortalChangeSource;
  using MessageCallback = std::function<void(const String&, const uint8_t*, size_t)>;
  static constexpr size_t PacketCapacity = 2048;
  /**
   * @brief Disallow copying or moving this resource-owning instance.
   * Input: const ArdMqtt&.
   * @return No result; this operation is deleted and cannot be called.
   */
  ArdMqtt(const ArdMqtt&) = delete;
  /**
   * @brief Replace this instance state with the supplied value.
   * Input: const ArdMqtt&.
   * @return No result; this operation is deleted and cannot be called.
   */
  ArdMqtt& operator=(const ArdMqtt&) = delete;
  /**
   * @brief Disallow copying or moving this resource-owning instance.
   * Input: ArdMqtt&&.
   * @return No result; this operation is deleted and cannot be called.
   */
  ArdMqtt(ArdMqtt&&) = delete;
  /**
   * @brief Replace this instance state with the supplied value.
   * Input: ArdMqtt&&.
   * @return No result; this operation is deleted and cannot be called.
   */
  ArdMqtt& operator=(ArdMqtt&&) = delete;
  /**
   * @brief Check whether the MQTT protocol session is connected.
   * @return True when the MQTT protocol session is connected.
   */
  bool connected() const { return _mqttState == MqttState::Connected; }
  /**
   * @brief Read the current connection or operation state.
   * @return The current MQTT connection state.
   */
  MqttState state() const { return _mqttState; }
  /**
   * @brief Install the callback for received MQTT application messages.
   * @param callback Completion or event handler supplied to this operation.
   * @return No value.
   */
  void onMessage(MessageCallback callback) { _message = std::move(callback); }
  /**
   * @brief Build an MQTT topic from its prefix, configured device name and suffix.
   * @param kind Topic prefix such as cmnd or stat.
   * @param command Command or state suffix appended to the topic.
   * @return The assembled MQTT topic.
   */
  String mqttTopic(const char* kind, const char* command) const;
  /**
   * @brief Validate an MQTT topic or subscription filter, including wildcard placement.
   * @param topic MQTT topic to publish, subscribe or match.
   * @param subscription Whether wildcard subscription-filter rules are allowed.
   * @return True for a supported topic/filter; false for invalid characters, length or wildcard use.
   */
  bool validMqttTopic(const String& topic, bool subscription = false) const;
  /**
   * @brief Queue an MQTT message for cooperative transmission.
   * @param topic MQTT topic to publish, subscribe or match.
   * @param payload Message bytes or text to send or decode.
   * @param retain Whether the broker should retain this MQTT message.
   * @return True if queued; false if disconnected, the topic is invalid or the packet cannot fit.
   */
  bool publish(const char* topic, const char* payload, bool retain = false);
  /**
   * @brief Queue an MQTT subscription request.
   * @param topic MQTT topic to publish, subscribe or match.
   * @return True if queued; false if disconnected or the topic/filter or packet is invalid.
   */
  bool subscribe(const char* topic);
private:
  friend class ArdPortal;
  friend class ArdHomeAssistant;
  friend class ArdAppControls;
  friend class ArdOta;
  /**
   * @brief Initialize this instance and its owned state.
   * @param portal Owning portal instance.
   * @return No value.
   */
  explicit ArdMqtt(ArdPortal& portal) : _portal(portal) {}
  ArdPortal& _portal;
  /**
   * @brief Check whether a connection trial or its pending save is active.
   * @return True while the corresponding trial state is pending; false otherwise.
   */
  bool trialActive() const { return _mqttTrial || _mqttTrialStart; }
  /**
   * @brief Check whether a connection trial or its pending save is active.
   * @return True while the corresponding trial state is pending; false otherwise.
   */
  bool trialSavePending() const { return _mqttSaveWaiting; }
#if ARDPORTAL_ENABLE_MQTT_TLS
  /**
   * @brief Check whether the selected MQTT transport uses TLS.
   * @return True if the active MQTT transport uses TLS.
   */
  bool tlsActive() const { return bool(_tls); }
#else
  /**
   * @brief Check whether the selected MQTT transport uses TLS.
   * @return True if the active MQTT transport uses TLS.
   */
  bool tlsActive() const { return false; }
#endif
  /**
   * @brief Return a busy response when the current operation prevents accepting a request.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool sendBusy() const { return _txSize || _subscriptionPending; }
  /**
   * @brief Read the revision counter for completed connection trials.
   * @return Revision counter incremented on a completed trial.
   */
  uint32_t trialRevision() const { return _mqttRevision; }
  /**
   * @brief Read the latest connection trial result code.
   * @return The latest trial result code.
   */
  uint8_t trialResult() const { return _mqttResult; }
  /**
   * @brief Expose the latest connection trial result text.
   * @return Reference to the latest trial result message.
   */
  const String& trialMessage() const { return _mqttResultMessage; }
  /**
   * @brief Notify MQTT of a change in station connectivity.
   * @param now Current time used to evaluate deadlines.
   * @return No value.
   */
  void wifiAvailable(uint32_t now);
  /**
   * @brief Notify the MQTT component that persisted configuration is available.
   * @return No value.
   */
  void configurationLoaded();
  /**
   * @brief Update device naming and MQTT/Home Assistant identity settings.
   * @param name Fallback name or generated symbol name.
   * @return No value.
   */
  void configureIdentity(const String& name) { _clientId = name; }
  /**
   * @brief Prepare a temporary connection using proposed settings.
   * @param candidate Proposed portal settings to test.
   * @return No value.
   */
  void prepareTrial(PortalConfig&& candidate);
  /**
   * @brief Remember the current settings as the rollback baseline for connection trials.
   * @return Reference to the requested stored value or component.
   */
  const PortalConfig& saveBaseline() const;
  /**
   * @brief Finish committing settings from a successful connection trial.
   * @param saved Result of the persistent save operation.
   * @param error Output error text; populated when the operation fails.
   * @param stillConnected Whether the existing connection can be retained during the trial.
   * @return No value.
   */
  void completeTrialSave(bool saved, const String& error, bool stillConnected);

  ArdMqttTransport _mqtt;
#if ARDPORTAL_ENABLE_MQTT_TLS
#if defined(ESP8266)
  ArdAllocation::Pointer<BearSSL::X509List> _trustAnchors;
#endif
  ArdAllocation::Pointer<WiFiClientSecure> _tls;
  /**
   * @brief Expose the selected plain or TLS MQTT client.
   * @return Reference to the requested stored value or component.
   */
  WiFiClient& mqttTransport() { return _tls ? static_cast<WiFiClient&>(*_tls) : _mqtt; }
#else
  /**
   * @brief Expose the selected plain or TLS MQTT client.
   * @return Reference to the requested stored value or component.
   */
  WiFiClient& mqttTransport() { return _mqtt; }
#endif
  String _clientId;
  MqttState _mqttState = MqttState::Disabled;
  MessageCallback _message;
  PortalConfig _mqttPrevious;
  bool _mqttTrial = false, _mqttTrialStart = false, _mqttTrialFailed = false, _mqttSaveWaiting = false;
  uint32_t _mqttRevision = 0, _mqttTrialSince = 0;
  uint8_t _mqttResult = 0;
  String _mqttResultMessage;
  uint32_t _mqttSince = 0, _lastTx = 0, _pingSince = 0, _txSince = 0;
  uint32_t _lastServiceMs=0,_maxServiceGapMs=0;
  bool _pingPending = false;
  uint8_t _rx[PacketCapacity], _tx[PacketCapacity];
  size_t _rxSize = 0, _txSize = 0, _txOffset = 0;
  uint16_t _packetId = 0, _subscriptionId = 0;
  bool _subscriptionPending = false;
  uint32_t _subscriptionSince = 0, _packetSince = 0;
  /**
   * @brief Advance MQTT connection, protocol and reconnect work.
   * @param now Current time used to evaluate deadlines.
   * @return No value.
   */
  void serviceMqtt(uint32_t now);
  /**
   * @brief Close the MQTT transport and clear its protocol state.
   * @param reason Restart or failure reason.
   * @return No value.
   */
  void closeMqtt(const char* reason=nullptr);
  /**
   * @brief Queue a QoS 0 MQTT PUBLISH packet on the active transport.
   * @param topic MQTT topic to publish, subscribe or match.
   * @param payload Message bytes or text to send or decode.
   * @param retain Whether the broker should retain this MQTT message.
   * @return True if the packet is accepted for transmission; false if disconnected or it exceeds packet capacity.
   */
  bool publishRaw(const char* topic, const char* payload, bool retain = false);
  /**
   * @brief Queue an MQTT SUBSCRIBE packet on the active transport.
   * @param topic MQTT topic to publish, subscribe or match.
   * @return True if accepted for transmission; false if disconnected or the packet cannot be constructed.
   */
  bool subscribeRaw(const char* topic);
  /**
   * @brief Queue an encoded MQTT packet for cooperative transmission.
   * @param type JSON, control or protocol type being examined.
   * @param body HTTP or MQTT message body.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool queuePacket(uint8_t type, const String& body);
  /**
   * @brief Decode and handle the completed MQTT protocol packet.
   * @param type JSON, control or protocol type being examined.
   * @param body HTTP or MQTT message body.
   * @param length Number of bytes or elements to process.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool processPacket(uint8_t type, const uint8_t* body, size_t length);
  /**
   * @brief Open the configured plain or TLS MQTT transport.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool connectMqttTransport();
  /**
   * @brief Transmit a bounded portion of queued MQTT bytes.
   * @param data Data buffer or value used by the operation.
   * @param length Number of bytes or elements to process.
   * @return Transmit a bounded portion of queued MQTT bytes.
   */
  size_t writeMqtt(const uint8_t* data, size_t length);
  /**
   * @brief Start a connection trial without committing the proposed settings.
   * @return No value.
   */
  void startTrial();
  /**
   * @brief Advance a temporary connection attempt and handle its timeout or result.
   * @return No value.
   */
  void serviceTrial();
  /**
   * @brief Record the MQTT trial result and arrange its configuration save.
   * @param saved Result of the persistent save operation.
   * @param error Output error text; populated when the operation fails.
   * @return No value.
   */
  void finishMqttTrial(bool saved, const String& error);
};

#endif
