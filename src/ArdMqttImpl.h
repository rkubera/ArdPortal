// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#include "ArdPortalFeatures.h"
#if ARDPORTAL_ENABLE_MQTT
#include "ArdMqtt.h"
#include "ArdPortalDeclarations.h"
#include "Language.h"
#include "NetworkIo.h"
#include <new>
using ArdNetworkIo::writeChunk;
using ArdNetworkIo::stopClient;
namespace { void appendText(String& packet, const String& text) { packet += char(text.length() >> 8); packet += char(text.length() & 255); packet += text; } }

bool ArdMqtt::queuePacket(uint8_t type, const String& body) {
  if (_txSize || body.length() > PacketCapacity - 5) return false;
  size_t length = body.length(), index = 0; _tx[index++] = type;
  do { uint8_t byte = length % 128; length /= 128; if (length) byte |= 128; _tx[index++] = byte; } while (length);
  memcpy(_tx + index, body.c_str(), body.length()); _txSize = index + body.length(); _txOffset = 0; _txSince = millis(); return true;
}

String ArdMqtt::mqttTopic(const char* kind, const char* command) const {
  if (!kind || !command) return "";
  String topic = String(kind) + "/" + _portal._config.deviceName + "/" + command;
  return validMqttTopic(topic, true) ? topic : String();
}

bool ArdMqtt::validMqttTopic(const String& topic, bool subscription) const {
  int first = topic.indexOf('/'), second = topic.indexOf('/', first + 1);
  if (first < 0 || second < 0 || topic.indexOf('/', second + 1) >= 0) return false;
  String kind = topic.substring(0, first), command = topic.substring(second + 1);
  if ((kind != "cmnd" && kind != "get" && kind != "stat") || topic.substring(first + 1, second) != _portal._config.deviceName || !command.length()) return false;
  if (subscription && (command == "+" || command == "#")) return true;
  for (size_t i = 0; i < command.length(); ++i) if (uint8_t(command[i]) < 32 || command[i] == '#' || command[i] == '+') return false;
  return true;
}

bool ArdMqtt::publish(const char* topic, const char* payload, bool retain) {
  return topic && validMqttTopic(topic) && publishRaw(topic, payload, retain);
}

bool ArdMqtt::publishRaw(const char* topic, const char* payload, bool retain) {
  if (!connected() || !topic || !*topic || !payload || strchr(topic, '#') || strchr(topic, '+') ||
      strlen(topic) + strlen(payload) + 2 > PacketCapacity - 5) return false;
  String body; appendText(body, topic); body += payload;
  bool queued = queuePacket(retain ? 0x31 : 0x30, body);
  if (queued) _portal.logMqtt(ArdUILanguage::text(ArdUILanguage::Key::s_146), topic, reinterpret_cast<const uint8_t*>(payload), strlen(payload));
  return queued;
}

bool ArdMqtt::subscribe(const char* topic) {
  if (!topic || !validMqttTopic(topic,true)) return false;
  return subscribeRaw(topic);
}

bool ArdMqtt::subscribeRaw(const char* topic) {
  if (!connected() || !topic || _subscriptionPending || strlen(topic) + 5 > PacketCapacity - 5) return false;
  uint16_t id = ++_packetId; if (!id) id = ++_packetId;
  String body; body += char(id >> 8); body += char(id & 255); appendText(body, topic); body += char(0);
  if (!queuePacket(0x82, body)) return false;
  _subscriptionId = id; _subscriptionPending = true; _subscriptionSince = millis(); return true;
}

void ArdMqtt::closeMqtt() {
  if (_mqttState == MqttState::Connecting || _mqttState == MqttState::Connected) _portal.log(ArdUILanguage::text(ArdUILanguage::Key::s_147));
  if (_mqttTrial) _mqttTrialFailed = true;
#if ARDPORTAL_ENABLE_MQTT_TLS
  if (_tls) { _tls->stop(); _tls.reset(); }
#if defined(ESP8266)
  _trustAnchors.reset();
#endif
#endif
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  _portal._appControls.disconnected();
#endif
  stopClient(_mqtt); _txSize = _txOffset = _rxSize = 0; _pingPending = false; _subscriptionPending = false;
  _mqttSince = millis();
  _mqttState = !_portal._config.host.length() ? MqttState::Disabled : (_portal.wifiConnected() ? MqttState::WaitingRetry : MqttState::WaitingForWifi);
}

bool ArdMqtt::processPacket(uint8_t type, const uint8_t* body, size_t length) {
  if (_mqttState == MqttState::Connecting) {
    if (type != 0x20 || length != 2 || body[0] != 0 || body[1] != 0) return false;
    _portal.log(ArdUILanguage::text(ArdUILanguage::Key::s_148));
    _mqttState = MqttState::Connected; _lastTx = millis(); 
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
_portal._appControls.resetMqtt();
#endif
#if ARDPORTAL_ENABLE_HA
    _portal._homeAssistant.resetDiscovery();
#endif
    return true;
  }
  if (type == 0xd0) {
    if (length || !_pingPending) return false;
    _pingPending = false; return true;
  }
  if (type == 0x90) {
    if (length != 3 || !_subscriptionPending || uint16_t(body[0] << 8 | body[1]) != _subscriptionId || body[2] != 0) return false;
    _subscriptionPending = false; return true;
  }
  if ((type >> 4) == 3) {
    // This small client requests QoS 0 only. Reject unsupported/malformed packets.
    if ((type & 6) || length < 2) return false;
    size_t topicSize = (body[0] << 8) | body[1];
    if (!topicSize || topicSize + 2 > length) return false;
    String topic; for (size_t i = 0; i < topicSize; ++i) { if (!body[2+i]) return false; topic += char(body[2+i]); }
#if ARDPORTAL_ENABLE_HA
    if (_portal._homeAssistant.handleStatus(topic, body + 2 + topicSize, length - 2 - topicSize)) return true;
#endif
    if (!validMqttTopic(topic)) { _portal.log(ArdUILanguage::text(ArdUILanguage::Key::s_149)); return true; }
    _portal.logMqtt("RX", topic, body + 2 + topicSize, length - 2 - topicSize);
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
    _portal._appControls.dynamicMqtt(topic,body+2+topicSize,length-2-topicSize,type&1);
#endif
    if (_message) _message(topic, body + 2 + topicSize, length - 2 - topicSize);
    return true;
  }
  return false;
}

void ArdMqtt::serviceMqtt(uint32_t now) {
  if (_portal._httpConfigUpload) return;
  if (!_portal._config.host.length()) { _mqttState = MqttState::Disabled; return; }
  if (!_portal.wifiConnected()) { _mqttState = MqttState::WaitingForWifi; return; }
#if ARDPORTAL_ENABLE_MQTT_TLS
  if (_portal._config.mqttTls && !_portal.tlsClockReady()) {
    if (_tls) closeMqtt();
    _mqttState = MqttState::WaitingForTime; return;
  }
#else
  // A stored TLS configuration must never silently use plaintext.
  if (_portal._config.mqttTls) { _mqttState=MqttState::Disabled;return; }
#endif
  if (_mqttState == MqttState::WaitingForTime) { _mqttSince = now - _portal._options.retryMs; _mqttState = MqttState::WaitingRetry; }
  if (_mqttState == MqttState::WaitingForWifi) _mqttState = MqttState::WaitingRetry;
  if (_mqttState == MqttState::WaitingRetry && uint32_t(now - _mqttSince) >= _portal._options.retryMs) {
    _portal.log(ArdUILanguage::text(ArdUILanguage::Key::s_145));
    if (!connectMqttTransport()) { _portal.log(ArdUILanguage::text(ArdUILanguage::Key::s_151)); closeMqtt(); return; }
    _mqttState = MqttState::Connecting; _mqttSince = _lastTx = millis();
    String body; appendText(body, "MQTT"); body += char(4);
    uint8_t flags = 2;
#if ARDPORTAL_ENABLE_HA
    if(_portal._dynamic.count()) flags=0x26;
#endif
    if (_portal._config.user.length()) flags |= 0x80; if (_portal._config.mqttPassword.length()) flags |= 0x40;
    body += char(flags); body += char(_portal._options.keepAliveSeconds >> 8); body += char(_portal._options.keepAliveSeconds & 255);
    appendText(body, _clientId);
#if ARDPORTAL_ENABLE_HA
    if (flags & 4) { appendText(body,mqttTopic("stat","availability")); appendText(body,"offline"); }
#endif
    if (flags & 0x80) appendText(body, _portal._config.user);
    if (flags & 0x40) appendText(body, _portal._config.mqttPassword);
    if (!queuePacket(0x10, body)) { closeMqtt(); return; }
  }
  if (_mqttState != MqttState::Connecting && _mqttState != MqttState::Connected) return;
  if (!mqttTransport().connected() && !mqttTransport().available()) { closeMqtt(); return; }
  now = millis();
  if ((_mqttState == MqttState::Connecting && uint32_t(now - _mqttSince) >= _portal._options.mqttTimeoutMs) ||
      (_pingPending && uint32_t(now - _pingSince) >= _portal._options.mqttTimeoutMs) ||
      (_subscriptionPending && uint32_t(now - _subscriptionSince) >= _portal._options.mqttTimeoutMs) ||
      (_rxSize && uint32_t(now - _packetSince) >= _portal._options.mqttTimeoutMs)) { closeMqtt(); return; }
  if (_txSize) {
    size_t length = _txSize - _txOffset; if (length > 256) length = 256;
    size_t written = writeMqtt(_tx + _txOffset, length); _txOffset += written;
    if (_txOffset == _txSize) { _txSize = _txOffset = 0; _lastTx = millis(); }
    else if (uint32_t(now - _txSince) >= _portal._options.mqttTimeoutMs) { closeMqtt(); return; }
  }
  // Consume at most 256 bytes per loop. Remaining Length can span multiple loops.
  for (size_t budget = 0; budget < 256 && mqttTransport().available(); ++budget) {
    if (_rxSize >= PacketCapacity) { closeMqtt(); return; }
    int c = mqttTransport().read(); if (c < 0) break;
    if (!_rxSize) _packetSince = millis();
    _rx[_rxSize++] = c;
    if (_rxSize < 2) continue;
    size_t remaining = 0, multiplier = 1, header = 1; bool complete = false;
    for (size_t i = 1; i < _rxSize && i <= 4; ++i) {
      remaining += (_rx[i] & 127) * multiplier; multiplier *= 128; header = i + 1;
      if (!(_rx[i] & 128)) { complete = true; break; }
    }
    if (!complete) { if (_rxSize >= 5) { closeMqtt(); return; } continue; }
    if (remaining > PacketCapacity - header) { closeMqtt(); return; }
    if (_rxSize == header + remaining) {
      bool ok = processPacket(_rx[0], _rx + header, remaining); _rxSize = 0;
      if (!ok) { closeMqtt(); return; }
    }
  }
  if (connected() && !_pingPending && !_txSize && uint32_t(millis() - _lastTx) >= uint32_t(_portal._options.keepAliveSeconds) * 500) {
    if (queuePacket(0xc0, "")) { _pingPending = true; _pingSince = millis(); }
  }
}

bool ArdMqtt::connectMqttTransport() {
#if ARDPORTAL_ENABLE_MQTT_TLS
  if (_portal._config.mqttTls) {
    _tls.reset(new (std::nothrow) WiFiClientSecure());
    if (!_tls) return false;
#if defined(ESP32)
    _tls->setCACert(_portal._config.caCert.c_str());
    _tls->setHandshakeTimeout(_portal._options.tlsHandshakeTimeoutSeconds);
    // Keep the hostname: the TLS layer uses it for SNI and certificate verification.
    if (!_tls->connect(_portal._config.host.c_str(), _portal._config.port, _portal._options.tcpTimeoutMs)) return false;
#else
    _trustAnchors.reset(new (std::nothrow) BearSSL::X509List(_portal._config.caCert.c_str()));
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
  if(_portal._config.mqttTls)return false;
#endif
  IPAddress ip;
  if (!ip.fromString(_portal._config.host) && !WiFi.hostByName(_portal._config.host.c_str(), ip)) return false;
#if defined(ESP32)
  if (!_mqtt.connect(ip, _portal._config.port, _portal._options.tcpTimeoutMs)) return false;
#else
  _mqtt.setTimeout(_portal._options.tcpTimeoutMs);
  if (!_mqtt.connect(ip, _portal._config.port)) return false;
#endif
  _mqtt.setTimeout(100); _mqtt.setNoDelay(true); return true;
}

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
  return writeChunk(_mqtt,data,length);
#endif
}

void ArdMqtt::finishMqttTrial(bool saved, const String& error) {
  _mqttTrial = false; _mqttSaveWaiting = false;
  _mqttResult = saved ? 1 : 2;
  _mqttResultMessage = saved ? ArdUILanguage::text(ArdUILanguage::Key::s_171) : error;
  if (!saved) { closeMqtt(); _portal._config = std::move(_mqttPrevious); _mqttSince = millis() - _portal._options.retryMs; }
  _mqttPrevious = Config(); _portal.log(_mqttResultMessage);
}

void ArdMqtt::startTrial() {
  if (_mqttTrialStart && !_portal._http) {
    closeMqtt(); _mqttPrevious = std::move(_portal._config); _portal._config = std::move(_portal._pending);
    _mqttTrialStart = false; _mqttTrial = true; _mqttTrialFailed = false;
    _mqttTrialSince = millis(); _mqttSince = millis() - _portal._options.retryMs;
    _mqttState = MqttState::WaitingRetry;
  }
}

void ArdMqtt::serviceTrial() {
  if (_mqttTrial && !_mqttSaveWaiting && (connected() || _mqttTrialFailed || uint32_t(millis() - _mqttTrialSince) >= _portal._options.mqttTimeoutMs)) {
    if (connected()) {
      _mqttSaveWaiting = true;
      if (!_portal.scheduleConfig(_portal._config, _portal._appConfig, ChangeSource::Portal, true)) {
        _mqttSaveWaiting = false; finishMqttTrial(false, ArdUILanguage::text(ArdUILanguage::Key::s_107));
      }
    } else finishMqttTrial(false, ArdUILanguage::text(ArdUILanguage::Key::s_108));
  }
}

void ArdMqtt::wifiAvailable(uint32_t now) { _mqttSince = now - _portal._options.retryMs; }
void ArdMqtt::configurationLoaded() {
  _mqttState = _portal._config.host.length() ? MqttState::WaitingForWifi : MqttState::Disabled;
}
void ArdMqtt::prepareTrial(Config&& candidate) {
  _portal._pending = std::move(candidate);
  _mqttTrialStart = true; ++_mqttRevision; _mqttResult = 0; _mqttResultMessage = "";
}
const ArdMqtt::Config& ArdMqtt::saveBaseline() const {
  return _mqttSaveWaiting ? _mqttPrevious : _portal._config;
}
void ArdMqtt::completeTrialSave(bool saved, const String& error, bool stillConnected) {
  finishMqttTrial(saved, error.length() ? error : ArdUILanguage::text(ArdUILanguage::Key::s_107));
  if (saved && !stillConnected) _mqttResultMessage = ArdUILanguage::text(ArdUILanguage::Key::s_147);
}

#endif
