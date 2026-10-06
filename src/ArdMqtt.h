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

// Owns the MQTT protocol, transport, TLS session and connection trial.
class ArdMqtt {
public:
  using PortalConfig = ArdPortalConfig;
  using MqttState = ArdMqttState;
  using ChangeSource = ArdPortalChangeSource;
  using MessageCallback = std::function<void(const String&, const uint8_t*, size_t)>;
  static constexpr size_t PacketCapacity = 2048;
  ArdMqtt(const ArdMqtt&) = delete;
  ArdMqtt& operator=(const ArdMqtt&) = delete;
  ArdMqtt(ArdMqtt&&) = delete;
  ArdMqtt& operator=(ArdMqtt&&) = delete;
  bool connected() const { return _mqttState == MqttState::Connected; }
  MqttState state() const { return _mqttState; }
  void onMessage(MessageCallback callback) { _message = std::move(callback); }
  String mqttTopic(const char* kind, const char* command) const;
  bool validMqttTopic(const String& topic, bool subscription = false) const;
  bool publish(const char* topic, const char* payload, bool retain = false);
  bool subscribe(const char* topic);
private:
  friend class ArdPortal;
  friend class ArdHomeAssistant;
  friend class ArdAppControls;
  friend class ArdOta;
  explicit ArdMqtt(ArdPortal& portal) : _portal(portal) {}
  ArdPortal& _portal;
  bool trialActive() const { return _mqttTrial || _mqttTrialStart; }
  bool trialSavePending() const { return _mqttSaveWaiting; }
#if ARDPORTAL_ENABLE_MQTT_TLS
  bool tlsActive() const { return bool(_tls); }
#else
  bool tlsActive() const { return false; }
#endif
  bool sendBusy() const { return _txSize || _subscriptionPending; }
  uint32_t trialRevision() const { return _mqttRevision; }
  uint8_t trialResult() const { return _mqttResult; }
  const String& trialMessage() const { return _mqttResultMessage; }
  void wifiAvailable(uint32_t now);
  void configurationLoaded();
  void configureIdentity(const String& name) { _clientId = name; }
  void prepareTrial(PortalConfig&& candidate);
  const PortalConfig& saveBaseline() const;
  void completeTrialSave(bool saved, const String& error, bool stillConnected);

  ArdMqttTransport _mqtt;
#if ARDPORTAL_ENABLE_MQTT_TLS
#if defined(ESP8266)
  std::unique_ptr<BearSSL::X509List> _trustAnchors;
#endif
  std::unique_ptr<WiFiClientSecure> _tls;
  WiFiClient& mqttTransport() { return _tls ? static_cast<WiFiClient&>(*_tls) : _mqtt; }
#else
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
  void serviceMqtt(uint32_t now);
  void closeMqtt(const char* reason=nullptr);
  bool publishRaw(const char* topic, const char* payload, bool retain = false);
  bool subscribeRaw(const char* topic);
  bool queuePacket(uint8_t type, const String& body);
  bool processPacket(uint8_t type, const uint8_t* body, size_t length);
  bool connectMqttTransport();
  size_t writeMqtt(const uint8_t* data, size_t length);
  void startTrial();
  void serviceTrial();
  void finishMqttTrial(bool saved, const String& error);
};

#endif
