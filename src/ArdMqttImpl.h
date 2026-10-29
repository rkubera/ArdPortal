#include "LanguageLiterals.h"
#include "DeviceName.h"
// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#include "ArdPortalFeatures.h"
#if ARDPORTAL_ENABLE_MQTT
#include "ArdPortalMqtt.h"
#include "ArdPortalDeclarations.h"
#include "Language.h"
#include "NetworkIo.h"
#include <new>
using ArdNetworkIo::writeChunk;
using ArdNetworkIo::stopClient;
#if !ARDPORTAL_USE_PUBSUBCLIENT
inline WiFiClient& ArdPortalMqttIO::transport() { return owner->mqttTransport(); }
inline size_t ArdPortalMqttIO::write(const uint8_t* data, size_t length) { return owner->writeMqtt(data, length); }
// The owning portal closes its transport immediately after loop() fails.
inline void ArdPortalMqttIO::stop() {}

inline void ArdPortalMqttIO::notify(char* topic, uint8_t* payload, unsigned int length) {
  owner->receiveMessage(topic, payload, length);
}
#endif


/**
 * @brief Build an MQTT topic from its prefix, configured device name and suffix.
 * @param kind Topic prefix such as cmnd or stat.
 * @param command Command or state suffix appended to the topic.
 * @return The assembled MQTT topic.
 */
String ArdMqtt::mqttTopic(const char* kind, const char* command) const {
  if (!kind || !command) return "";
  String topic = String(kind) + "/" + ArdDeviceName::mqtt(_portal._config.deviceName) + "/" + command;
  return validMqttTopic(topic, true) ? topic : String();
}

/**
 * @brief Validate an MQTT topic or subscription filter, including wildcard placement.
 * @param topic MQTT topic to publish, subscribe or match.
 * @param subscription Whether wildcard subscription-filter rules are allowed.
 * @return True for a supported topic/filter; false for invalid characters, length or wildcard use.
 */
bool ArdMqtt::validMqttTopic(const String& topic, bool subscription) const {
  int first = topic.indexOf('/'), second = topic.indexOf('/', first + 1);
  if (first < 0 || second < 0 || topic.indexOf('/', second + 1) >= 0) return false;
  // Validate the slices in place: no prefix/command/device substrings and no
  // normalized copy of the configured name are needed.
  if (strncmp_P(topic.c_str(), PSTR("cmnd/"), 5) && strncmp_P(topic.c_str(), PSTR("get/"), 4) && strncmp_P(topic.c_str(), PSTR("stat/"), 5)) return false;
  const String& name = _portal._config.deviceName;
  if (size_t(second - first - 1) != name.length() || size_t(second + 1) == topic.length()) return false;
  for (size_t i = 0; i < name.length(); ++i)
    if (topic[first + 1 + i] != (name[i] == ' ' ? '-' : name[i])) return false;
  const size_t command = second + 1;
  if (subscription && command + 1 == topic.length() && (topic[command] == '+' || topic[command] == '#')) return true;
  for (size_t i = command; i < topic.length(); ++i)
    if (uint8_t(topic[i]) < 32 || topic[i] == '#' || topic[i] == '+') return false;
  return true;
}

/**
 * @brief Queue an MQTT message for cooperative transmission.
 * @param topic MQTT topic to publish, subscribe or match.
 * @param payload Message bytes or text to send or decode.
 * @param retain Whether the broker should retain this MQTT message.
 * @return True if queued; false if disconnected, the topic is invalid or the packet cannot fit.
 */
bool ArdMqtt::publish(const char* topic, const char* payload, bool retain) {
  return topic && validMqttTopic(topic) && publishRaw(topic, payload, retain);
}

/**
 * @brief Queue a QoS 0 MQTT PUBLISH packet on the active transport.
 * @param topic MQTT topic to publish, subscribe or match.
 * @param payload Message bytes or text to send or decode.
 * @param retain Whether the broker should retain this MQTT message.
 * @return True if the packet is accepted for transmission; false if disconnected or it exceeds packet capacity.
 */
bool ArdMqtt::publishRaw(const char* topic, const char* payload, bool retain) {
#if ARDPORTAL_USE_PUBSUBCLIENT
  if (!topic || !*topic || !payload || strchr(topic, '#') || strchr(topic, '+') || strlen(topic) + strlen(payload) + 2 > PacketCapacity - 5) return false;
#endif
  bool queued = connected() && _protocol.publish(topic, payload, retain);
#if ARDPORTAL_ENABLE_CONSOLE || ARDPORTAL_MQTT_DEBUG_SERIAL || ARDPORTAL_ENABLE_LOG_CALLBACK
  if (queued) _portal.logMqtt(ArdUILanguage::text(ArdUILanguage::Key::s_146), topic, reinterpret_cast<const uint8_t*>(payload), strlen(payload));
#endif
  return queued;
}

/**
 * @brief Queue an MQTT subscription request.
 * @param topic MQTT topic to publish, subscribe or match.
 * @return True if queued; false if disconnected or the topic/filter or packet is invalid.
 */
bool ArdMqtt::subscribe(const char* topic) {
  if (!topic || !validMqttTopic(topic, true)) return false;
  return subscribeRaw(topic);
}

/**
 * @brief Queue an MQTT SUBSCRIBE packet on the active transport.
 * @param topic MQTT topic to publish, subscribe or match.
 * @return True if accepted for transmission; false if disconnected or the packet cannot be constructed.
 */
bool ArdMqtt::subscribeRaw(const char* topic) {
  return connected() && topic && _protocol.subscribe(topic);
}

/**
 * @brief Close the MQTT transport and clear its protocol state.
 * @param reason Restart or failure reason.
 * @return No value.
 */
void ArdMqtt::closeMqtt(const char* reason) {
#if ARDPORTAL_ENABLE_CONSOLE || ARDPORTAL_MQTT_DEBUG_SERIAL || ARDPORTAL_ENABLE_LOG_CALLBACK
  if (reason || _mqttState == MqttState::Connecting || _mqttState == MqttState::Connected) {
    String message = ArdUILanguage::text(ArdUILanguage::Key::s_147);
    _portal.consoleLine(true, message);
#if ARDPORTAL_ENABLE_CONSOLE_MESSAGES || ARDPORTAL_MQTT_DEBUG_SERIAL
    if (reason) {
      const uint32_t now = millis();
      message += String(F(" [")) + reason + ARD_LANG_NATIVE_STATE + (_mqttState == MqttState::Connected ? "connected" : _mqttState == MqttState::Connecting ? "connecting"
                                                                                                                                                            : "retry");
      message += ARD_LANG_NATIVE_SESSIONMS + String(uint32_t(now - _mqttSince)) + ARD_LANG_NATIVE_TXIDLEMS + String(uint32_t(now - _lastTx));
      message += ARD_LANG_NATIVE_MAXLOOPGAPMS + String(_maxServiceGapMs) + ARD_LANG_NATIVE_HEAP + String(ArdHeap::sample().free8) + "]";
    }
    _portal.logMessage(message);
#if ARDPORTAL_MQTT_DEBUG_SERIAL
    ardMqttSerialDiagnostic(message);
#endif
#else
    (void)reason;
#endif
  }
#else
  (void)reason;
#endif
  if (_mqttTrial) _mqttTrialFailed = true;
#if ARDPORTAL_USE_PUBSUBCLIENT
#if ARDPORTAL_ENABLE_MQTT_TLS
  if (!_tls) stopClient(_mqtt);
#else
  stopClient(_mqtt);
#endif
#endif
  _protocol.disconnect();
#if ARDPORTAL_USE_PUBSUBCLIENT
  _protocol.setClient(_mqtt);
#endif
#if ARDPORTAL_ENABLE_MQTT_TLS
  if (_tls) {
    _tls->stop();
    _tls.reset();
  }
#if defined(ESP8266)
  _trustAnchors.reset();
#endif
#endif
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  _portal._appControls.disconnected();
#endif
  stopClient(_mqtt);
  _mqttSince = millis();
  _mqttState = !_portal._config.host.length() ? MqttState::Disabled : (_portal.wifiConnected() ? MqttState::WaitingRetry : MqttState::WaitingForWifi);
}

/**
 * @brief Decode and handle the completed MQTT protocol packet.
 * @param type JSON, control or protocol type being examined.
 * @param body HTTP or MQTT message body.
 * @param length Number of bytes or elements to process.
 * @return True on success; false if validation, resource allocation or the operation fails.
 */
void ArdMqtt::receiveMessage(char* name, uint8_t* payload, unsigned int length) {
  connectionEstablished();
  String topic(name);
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  const bool retained = _protocol.retained();
#endif
#if ARDPORTAL_ENABLE_HA
  if (_portal._homeAssistant.handleStatus(topic, payload, length)) return;
#endif
  if (!validMqttTopic(topic)) {
#if ARDPORTAL_ENABLE_CONSOLE_MESSAGES
    _portal.log(ArdUILanguage::text(ArdUILanguage::Key::s_149));
#endif
    return;
  }
  _portal.logMqtt("RX", topic, payload, length);
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  _portal._appControls.dynamicMqtt(topic, payload, length, retained);
#endif
  if (_message) _message(topic, payload, length);
}

void ArdMqtt::connectionEstablished() {
  if (_mqttState != MqttState::Connecting || !_protocol.connected()) return;
#if ARDPORTAL_ENABLE_CONSOLE_MESSAGES
  _portal.log(ArdUILanguage::text(ArdUILanguage::Key::s_148));
#endif
#if ARDPORTAL_ENABLE_CONSOLE_MESSAGES || ARDPORTAL_MQTT_DEBUG_SERIAL
  String message = String(F(ARD_LANG_NATIVE_MQTT_CONNECTED_CLIENTID)) + _clientId + ARD_LANG_NATIVE_CONNECTMS + String(uint32_t(millis() - _mqttSince));
  _portal.logMessage(message);
#if ARDPORTAL_MQTT_DEBUG_SERIAL
  ardMqttSerialDiagnostic(message);
#endif
#endif
  _mqttState = MqttState::Connected;
  _lastTx = _lastServiceMs = millis();
  _maxServiceGapMs = 0;
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  _portal._appControls.resetMqtt();
#endif
#if ARDPORTAL_ENABLE_HA
  _portal._homeAssistant.resetDiscovery();
#endif
}

/**
 * @brief Advance MQTT connection, protocol and reconnect work.
 * @param now Current time used to evaluate deadlines.
 * @return No value.
 */
void ArdMqtt::serviceMqtt(uint32_t now) {
  if (!_autoReconnect) {
    if (connected()) closeMqtt(ARD_LANG_NATIVE_DISABLED_BY_API);
    _mqttState = MqttState::Disabled;
    return;
  }
  if (_mqttState == MqttState::Connected) {
    const uint32_t gap = uint32_t(now - _lastServiceMs);
    if (gap > _maxServiceGapMs) _maxServiceGapMs = gap;
  }
  _lastServiceMs = now;
  if (_portal._httpConfigUpload) return;
  if (!_portal._config.host.length()) {
    _mqttState = MqttState::Disabled;
    return;
  }
  if (!_portal.wifiConnected()) {
    _mqttState = MqttState::WaitingForWifi;
    return;
  }
#if ARDPORTAL_ENABLE_MQTT_TLS
  if (_portal._config.mqttTls && !_portal.tlsClockReady()) {
    if (_tls) closeMqtt();
    _mqttState = MqttState::WaitingForTime;
    return;
  }
#else
  // A stored TLS configuration must never silently use plaintext.
  if (_portal._config.mqttTls) {
    _mqttState = MqttState::Disabled;
    return;
  }
#endif
  if (_mqttState == MqttState::WaitingForTime) {
    _mqttSince = now - _portal._options.retryMs;
    _mqttState = MqttState::WaitingRetry;
  }
  if (_mqttState == MqttState::WaitingForWifi) _mqttState = MqttState::WaitingRetry;
  if (_mqttState == MqttState::WaitingRetry && uint32_t(now - _mqttSince) >= _portal._options.retryMs) {
#if ARDPORTAL_ENABLE_CONSOLE_MESSAGES
    _portal.log(ArdUILanguage::text(ArdUILanguage::Key::s_145));
#endif
#if ARDPORTAL_ENABLE_CONSOLE_MESSAGES || ARDPORTAL_MQTT_DEBUG_SERIAL
    String message = String(F(ARD_LANG_NATIVE_MQTT_CONNECTING)) + _portal._config.host + ":" + String(_portal._config.port) + ARD_LANG_NATIVE_CLIENTID + _clientId + ARD_LANG_NATIVE_TLS + String(_portal._config.mqttTls ? "yes" : "no");
    _portal.logMessage(message);
#if ARDPORTAL_MQTT_DEBUG_SERIAL
    ardMqttSerialDiagnostic(message);
#endif
#endif
    if (!connectMqttTransport()) {
#if ARDPORTAL_ENABLE_CONSOLE_MESSAGES
      _portal.log(ArdUILanguage::text(ArdUILanguage::Key::s_151));
#endif
      closeMqtt(ARD_LANG_NATIVE_TRANSPORT_CONNECT_FAILED);
      return;
    }
    _mqttState = MqttState::Connecting;
    _mqttSince = _lastTx = millis();
#if ARDPORTAL_USE_PUBSUBCLIENT
    _protocol.setClient(mqttTransport());
    _protocol.setCallback([this](char* topic, uint8_t* payload, unsigned int length) {
      receiveMessage(topic, payload, length);
    });
#endif
    _protocol.setKeepAlive(_portal._options.keepAliveSeconds);
#if ARDPORTAL_USE_PUBSUBCLIENT
    _protocol.setSocketTimeout((_portal._options.mqttTimeoutMs + 999) / 1000);
#else
    _protocol.setTimeoutMs(_portal._options.mqttTimeoutMs);
#endif
    if (!_protocol.setBufferSize(PacketCapacity)) { closeMqtt(); return; }
#if ARDPORTAL_USE_PUBSUBCLIENT
    _protocol.setServer(_portal._config.host.c_str(), _portal._config.port);
#endif
    const char* willTopic = nullptr;
    String availability;
#if ARDPORTAL_ENABLE_HA
    if (_portal._dynamic.count()) { availability = mqttTopic("stat", "availability"); willTopic = availability.c_str(); }
#endif
    if (!_protocol.connect(_clientId.c_str(), _portal._config.user.length() ? _portal._config.user.c_str() : nullptr,
                           _portal._config.mqttPassword.length() ? _portal._config.mqttPassword.c_str() : nullptr,
                           willTopic, 0, true, willTopic ? "offline" : nullptr)) {
      closeMqtt(); return;
    }
  }
  if (_mqttState != MqttState::Connecting && _mqttState != MqttState::Connected) return;
  if (!_protocol.loop()) { closeMqtt(ARD_LANG_NATIVE_TRANSPORT_CLOSED); return; }
  connectionEstablished();
}

/**
 * @brief Open the configured plain or TLS MQTT transport.
 * @return True on success; false if validation, resource allocation or the operation fails.
 */
bool ArdMqtt::connectMqttTransport() {
#if ARDPORTAL_ENABLE_MQTT_TLS
  if (_portal._config.mqttTls) {
    _tls.reset(ArdAllocation::create<WiFiClientSecure>());
    if (!_tls) return false;
#if defined(ESP32)
    _tls->setCACert(_portal._config.caCert.c_str());
    _tls->setHandshakeTimeout(_portal._options.tlsHandshakeTimeoutSeconds);
    // Keep the hostname: the TLS layer uses it for SNI and certificate verification.
    if (!_tls->connect(_portal._config.host.c_str(), _portal._config.port, _portal._options.tcpTimeoutMs)) return false;
#else
    _trustAnchors.reset(ArdAllocation::create<BearSSL::X509List>(_portal._config.caCert.c_str()));
    if (!_trustAnchors || !_trustAnchors->getCount()) return false;
    _tls->setTrustAnchors(_trustAnchors.get());
    _tls->setX509Time(time(nullptr));
    // The public ESP8266 wrapper does not forward Stream::setTimeout to BearSSL.
    // Its core controls the synchronous handshake and I/O timeouts.
    _tls->setSSLVersion(BR_TLS12, BR_TLS12);
    if (!_tls->connect(_portal._config.host.c_str(), _portal._config.port)) return false;
#endif
    _tls->setNoDelay(true);
    return true;
  }
#else
  if (_portal._config.mqttTls) return false;
#endif
  IPAddress ip;
  if (!ip.fromString(_portal._config.host) && !WiFi.hostByName(_portal._config.host.c_str(), ip)) return false;
#if defined(ESP32)
  if (!_mqtt.connect(ip, _portal._config.port, _portal._options.tcpTimeoutMs)) return false;
#else
  _mqtt.setTimeout(_portal._options.tcpTimeoutMs);
  if (!_mqtt.connect(ip, _portal._config.port)) return false;
#endif
  _mqtt.protectConnection();
  _mqtt.setTimeout(100);
  _mqtt.setNoDelay(true);
  return true;
}

/**
 * @brief Transmit a bounded portion of queued MQTT bytes.
 * @param data Data buffer or value used by the operation.
 * @param length Number of bytes or elements to process.
 * @return Transmit a bounded portion of queued MQTT bytes.
 */


/**
 * @brief Record the MQTT trial result and arrange its configuration save.
 * @param saved Result of the persistent save operation.
 * @param error Output error text; populated when the operation fails.
 * @return No value.
 */
void ArdMqtt::finishMqttTrial(bool saved, const String& error) {
  _mqttTrial = false;
  _mqttSaveWaiting = false;
  _mqttResult = saved ? 1 : 2;
  _mqttResultMessage = saved ? ArdUILanguage::text(ArdUILanguage::Key::s_171) : error;
  if (!saved) {
    closeMqtt();
    _portal._config = std::move(_mqttPrevious);
    _mqttSince = millis() - _portal._options.retryMs;
  }
  _mqttPrevious = PortalConfig();
  _portal.log(_mqttResultMessage);
}

/**
 * @brief Start a connection trial without committing the proposed settings.
 * @return No value.
 */
void ArdMqtt::startTrial() {
  if (_mqttTrialStart && !_portal._http) {
    closeMqtt();
    _mqttPrevious = std::move(_portal._config);
    _portal._config = std::move(_portal._pending);
    _mqttTrialStart = false;
    _mqttTrial = true;
    _mqttTrialFailed = false;
    _mqttTrialSince = millis();
    _mqttSince = millis() - _portal._options.retryMs;
    _mqttState = MqttState::WaitingRetry;
  }
}

/**
 * @brief Advance a temporary connection attempt and handle its timeout or result.
 * @return No value.
 */
void ArdMqtt::serviceTrial() {
  if (_mqttTrial && !_mqttSaveWaiting && (connected() || _mqttTrialFailed || uint32_t(millis() - _mqttTrialSince) >= _portal._options.mqttTimeoutMs)) {
    if (connected()) {
      _mqttSaveWaiting = true;
      if (!_portal.scheduleConfig(_portal._config, _portal._appConfig, ChangeSource::Portal, true)) {
        _mqttSaveWaiting = false;
        finishMqttTrial(false, ArdUILanguage::text(ArdUILanguage::Key::s_107));
      }
    } else finishMqttTrial(false, ArdUILanguage::text(ArdUILanguage::Key::s_108));
  }
}

/**
 * @brief Notify MQTT of a change in station connectivity.
 * @param now Current time used to evaluate deadlines.
 * @return No value.
 */
void ArdMqtt::wifiAvailable(uint32_t now) {
  _mqttSince = now - _portal._options.retryMs;
}
/**
 * @brief Notify the MQTT component that persisted configuration is available.
 * @return No value.
 */
void ArdMqtt::configurationLoaded() {
  _mqttState = _portal._config.host.length() ? MqttState::WaitingForWifi : MqttState::Disabled;
}
/**
 * @brief Prepare a temporary connection using proposed settings.
 * @param candidate Proposed portal settings to test.
 * @return No value.
 */
void ArdMqtt::prepareTrial(PortalConfig&& candidate) {
  _autoReconnect = true;
  _portal._pending = std::move(candidate);
  _mqttTrialStart = true;
  ++_mqttRevision;
  _mqttResult = 0;
  _mqttResultMessage = "";
}
/**
 * @brief Remember the current settings as the rollback baseline for connection trials.
 * @return Reference to the requested stored value or component.
 */
const ArdMqtt::PortalConfig& ArdMqtt::saveBaseline() const {
  return _mqttSaveWaiting ? _mqttPrevious : _portal._config;
}
/**
 * @brief Finish committing settings from a successful connection trial.
 * @param saved Result of the persistent save operation.
 * @param error Output error text; populated when the operation fails.
 * @param stillConnected Whether the existing connection can be retained during the trial.
 * @return No value.
 */
void ArdMqtt::completeTrialSave(bool saved, const String& error, bool stillConnected) {
  finishMqttTrial(saved, error.length() ? error : ArdUILanguage::text(ArdUILanguage::Key::s_107));
  if (saved && !stillConnected) _mqttResultMessage = ArdUILanguage::text(ArdUILanguage::Key::s_147);
}


/** @brief Resume saved MQTT connection. @return True when retry is enabled. */
bool ArdMqtt::connect() {
  if (!_portal._configurationReady || _portal.portalAndAppConfigBusy() || _portal.otaActive() || _portal._rebootPending || connectionTestBusy() || !_portal._config.host.length()) return false;
  _autoReconnect = true;
  if (!connected()) {
    _mqttState = _portal.wifiConnected() ? MqttState::WaitingRetry : MqttState::WaitingForWifi;
    _mqttSince = millis() - _portal._options.retryMs;
  }
  return true;
}
/** @brief Stop MQTT and future retries. @return True when accepted. */
bool ArdMqtt::disconnect() {
  if (connectionTestBusy() || _portal.otaActive() || _portal._rebootPending) return false;
  _autoReconnect = false;
  closeMqtt(ARD_LANG_NATIVE_DISCONNECTED_BY_API);
  _mqttState = MqttState::Disabled;
  return true;
}
/** @brief Queue an unsubscribe packet with acknowledgement tracking. @param topic Filter. @return True when queued. */
bool ArdMqtt::unsubscribe(const char* topic) {
  return connected() && topic && validMqttTopic(topic, true) && _protocol.unsubscribe(topic);
}
/** @brief Start the WWW-compatible connection trial. @param config Candidate settings. @return True when staged. */
bool ArdMqtt::testConnection(const PortalConfig& config) {
  if (!_portal._configurationReady || _portal.portalAndAppConfigBusy() || _portal.otaActive() || _portal._rebootPending || connectionTestBusy() || !_portal.wifiConnected() || !config.host.length() || !ArdPortal::validPortalConfig(config)) return false;
#if !ARDPORTAL_ENABLE_MQTT_TLS
  if (config.mqttTls) return false;
#endif
  if (config.ssid != _portal._config.ssid || config.password != _portal._config.password) return false;
  PortalConfig candidate = config;
  _autoReconnect = true;
  prepareTrial(std::move(candidate));
  return true;
}
/** @brief Deliver shared state changes and completed trial notifications. @return No value. */
void ArdMqtt::serviceApiCallbacks() {
  if (_apiState != _mqttState) {
    _apiState = _mqttState;
    auto callback = _stateCallback;
    if (callback) callback(_mqttState);
  }
  if (_mqttResult && !connectionTestBusy() && _apiTrialRevision != _mqttRevision) {
    _apiTrialRevision = _mqttRevision;
    auto callback = _trialCallback;
    if (callback) callback(_mqttResult == 1, _mqttResultMessage);
  }
}
#if !ARDPORTAL_USE_PUBSUBCLIENT
size_t ArdMqtt::writeMqtt(const uint8_t* data, size_t length) {
#if ARDPORTAL_ENABLE_MQTT_TLS
  if (!_tls) return writeChunk(_mqtt, data, length);
    // Never send on the raw TLS socket: all MQTT bytes must pass through encryption.
#if defined(ESP8266)
  int room = _tls->availableForWrite();
  if (room <= 0) return 0;
  if (length > size_t(room)) length = room;
#endif
  return _tls->write(data, length);
#else
  return writeChunk(_mqtt, data, length);
#endif
}
#endif
#endif
