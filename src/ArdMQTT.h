// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>
#include <Client.h>
#include <functional>
#include <utility>

// Default transport binding for standalone use. Portal builds replace this
// binding at compile time: no type-erased callback or transport dispatch layer.
struct ArdMQTTIO {
  using Callback = std::function<void(char*, uint8_t*, unsigned int)>;
  Client* _client = nullptr;
  Callback _callback;
  ArdMQTTIO() = default;
  explicit ArdMQTTIO(Client& client) : _client(&client) {}
  Client& transport() { return *_client; }
  bool ready() const { return _client; }
  void notify(char* topic, uint8_t* payload, unsigned int length) {
    if (_callback) _callback(topic, payload, length);
  }
  size_t write(const uint8_t* data, size_t length) { return _client->write(data, length); }
  void stop() { if (_client) _client->stop(); }
};

// Standalone, cooperative MQTT 3.1.1 client; transport is supplied by the caller.
template<class IO = ArdMQTTIO>
class ArdMQTTClient : private IO {
public:
  static constexpr size_t PacketCapacity = 2048;
  ArdMQTTClient() = default;
  explicit ArdMQTTClient(Client& client) : IO(client) {}
  explicit ArdMQTTClient(IO binding) : IO(std::move(binding)) {}
  ArdMQTTClient(const ArdMQTTClient&) = delete;
  ArdMQTTClient& operator=(const ArdMQTTClient&) = delete;
  void setClient(Client& client) { this->_client = &client; }
  void setCallback(ArdMQTTIO::Callback callback) { this->_callback = std::move(callback); }
  void setKeepAlive(uint16_t seconds) { _keepAlive = seconds; }
  void setSocketTimeout(uint16_t seconds) { _timeout = uint32_t(seconds) * 1000; }
  void setTimeoutMs(uint32_t milliseconds) { _timeout = milliseconds; }
  bool setBufferSize(uint16_t size) { return size == PacketCapacity; }
  bool connected() const { return _connected; }
  bool pending() const { return _connecting; }
  bool busy() const { return _txSize || _subscriptionPending; }
  bool receiveBusy() const { return _rxSize || _pingPending; }
  bool retained() const { return _retained; }
  void disconnect() {
    _connected = _connecting = _pingPending = _subscriptionPending = false;
    _tx = String(); _txSize = _txOffset = _rxSize = 0;
    IO::stop();
  }
  bool connect(const char* id, const char* user = nullptr, const char* password = nullptr,
               const char* willTopic = nullptr, uint8_t willQos = 0,
               bool willRetain = false, const char* will = nullptr);
  bool publish(const char* topic, const char* payload, bool retain = false);
  bool subscribe(const char* topic);
  bool unsubscribe(const char* topic);
  bool loop();
private:
  bool _connected = false, _connecting = false, _retained = false;
  bool _pingPending = false, _subscriptionPending = false, _unsubscribePending = false;
  uint16_t _keepAlive = 60, _packetId = 0, _subscriptionId = 0;
  uint32_t _timeout = 10000, _since = 0, _lastTx = 0, _pingSince = 0;
  uint32_t _txSince = 0, _subscriptionSince = 0, _packetSince = 0;
  uint8_t _rx[PacketCapacity];
  String _tx;
  size_t _rxSize = 0, _txSize = 0, _txOffset = 0;
  static void appendText(String& packet, const char* text) {
    const size_t length = strlen(text);
    packet += char(length >> 8);
    packet += char(length);
    packet.concat(text, length);
  }
  bool queuePacket(uint8_t type, const String& body);
  bool subscription(const char* topic, bool remove);
  bool processPacket(uint8_t type, uint8_t* body, size_t length);
};

template<class IO>
bool ArdMQTTClient<IO>::queuePacket(uint8_t type, const String& body) {
  if (_txSize || body.length() > PacketCapacity - 5) return false;
  char header[5];
  size_t length = body.length(), index = 0;
  header[index++] = char(type);
  do {
    uint8_t byte = length % 128;
    length /= 128;
    if (length) byte |= 128;
    header[index++] = char(byte);
  } while (length);
  String packet;
  if (!packet.reserve(index + body.length()) || !packet.concat(header, index) || !packet.concat(body.c_str(), body.length())) return false;
  _tx = std::move(packet);
  _txSize = index + body.length();
  _txOffset = 0;
  _txSince = millis();
  return true;
}

template<class IO>
inline __attribute__((always_inline)) bool ArdMQTTClient<IO>::connect(const char* id, const char* user, const char* password,
                             const char* willTopic, uint8_t willQos, bool willRetain, const char* will) {
  if (!IO::ready() || !IO::transport().connected() || !id || _connected || _connecting || willQos) return false;
  String body;
  appendText(body, "MQTT"); body += char(4);
  uint8_t flags = 2;
  if (willTopic && will) flags |= 4 | (willRetain ? 32 : 0);
  if (user && *user) flags |= 128;
  if (password && *password) flags |= 64;
  body += char(flags); body += char(_keepAlive >> 8); body += char(_keepAlive);
  appendText(body, id);
  if (flags & 4) { appendText(body, willTopic); appendText(body, will); }
  if (flags & 128) appendText(body, user);
  if (flags & 64) appendText(body, password);
  if (!queuePacket(0x10, body)) return false;
  _connecting = true; _since = _lastTx = millis();
  return true;
}
template<class IO>
inline __attribute__((always_inline)) bool ArdMQTTClient<IO>::publish(const char* topic, const char* payload, bool retain) {
  if (!connected() || !topic || !*topic || !payload || strchr(topic, '#') || strchr(topic, '+') || strlen(topic) + strlen(payload) + 2 > PacketCapacity - 5) return false;
  String body; appendText(body, topic); body += payload;
  return queuePacket(retain ? 0x31 : 0x30, body);
}
template<class IO>
inline __attribute__((always_inline)) bool ArdMQTTClient<IO>::subscription(const char* topic, bool remove) {
  if (!connected() || !topic || !*topic || _subscriptionPending || strlen(topic) + 5 > PacketCapacity - 5) return false;
  uint16_t id = ++_packetId; if (!id) id = ++_packetId;
  String body; body += char(id >> 8); body += char(id); appendText(body, topic);
  if (!remove) body += char(0);
  if (!queuePacket(remove ? 0xa2 : 0x82, body)) return false;
  _subscriptionId = id; _subscriptionPending = true; _unsubscribePending = remove;
  _subscriptionSince = millis(); return true;
}
template<class IO>
inline __attribute__((always_inline)) bool ArdMQTTClient<IO>::subscribe(const char* topic) { return subscription(topic, false); }
template<class IO>
bool ArdMQTTClient<IO>::unsubscribe(const char* topic) { return subscription(topic, true); }
template<class IO>
bool ArdMQTTClient<IO>::processPacket(uint8_t type, uint8_t* body, size_t length) {
  if (_connecting) {
    if (type != 0x20 || length != 2 || body[0] || body[1]) return false;
    _connecting = false; _connected = true; return true;
  }
  if (type == 0xd0) {
    if (length || !_pingPending) return false;
    _pingPending = false; return true;
  }
  if (type == 0x90 || type == 0xb0) {
    bool remove = type == 0xb0;
    if (length != (remove ? 2u : 3u) || !_subscriptionPending || remove != _unsubscribePending ||
        uint16_t(body[0] << 8 | body[1]) != _subscriptionId || (!remove && body[2])) return false;
    _subscriptionPending = false; _unsubscribePending = false; return true;
  }
  if ((type >> 4) != 3 || (type & 6) || length < 2) return false;
  size_t topicSize = (body[0] << 8) | body[1];
  if (!topicSize || topicSize + 2 > length) return false;
  for (size_t i = 0; i < topicSize; ++i) if (!body[2+i]) return false;
  // Reuse the two-byte topic length for a terminated callback topic.
  memmove(body + 1, body + 2, topicSize);
  body[topicSize + 1] = 0;
  _retained = type & 1;
  IO::notify(reinterpret_cast<char*>(body + 1), body + 2 + topicSize, length - 2 - topicSize);
  return true;
}
template<class IO>
inline __attribute__((always_inline)) bool ArdMQTTClient<IO>::loop() {
  if (!IO::ready() || (!_connected && !_connecting)) return false;
  auto fail = [this]() { disconnect(); return false; };
  if (!IO::transport().connected() && !IO::transport().available()) return fail();
  if (_txSize) {
    size_t length = _txSize - _txOffset;
    if (length > 256) length = 256;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(_tx.c_str()) + _txOffset;
    _txOffset += IO::write(data, length);
    if (_txOffset == _txSize) { _tx = String(); _txSize = _txOffset = 0; _lastTx = millis(); }
    else if (uint32_t(millis() - _txSince) >= _timeout) return fail();
  }
  for (size_t budget = 0; budget < 256 && IO::transport().available(); ++budget) {
    if (_rxSize >= PacketCapacity) return fail();
    int c = IO::transport().read(); if (c < 0) break;
    if (!_rxSize) _packetSince = millis();
    _rx[_rxSize++] = c; if (_rxSize < 2) continue;
    size_t remaining = 0, multiplier = 1, header = 1; bool complete = false;
    for (size_t i = 1; i < _rxSize && i <= 4; ++i) {
      remaining += (_rx[i] & 127) * multiplier; multiplier *= 128; header = i + 1;
      if (!(_rx[i] & 128)) { complete = true; break; }
    }
    if (!complete) { if (_rxSize >= 5) return fail(); continue; }
    if (remaining > PacketCapacity - header) return fail();
    if (_rxSize == header + remaining) {
      bool ok = processPacket(_rx[0], _rx + header, remaining); _rxSize = 0;
      if (!ok) return fail();
    }
  }
  const uint32_t now = millis();
  if ((_connecting && uint32_t(now - _since) >= _timeout) ||
      (_pingPending && uint32_t(now - _pingSince) >= _timeout) ||
      (_subscriptionPending && uint32_t(now - _subscriptionSince) >= _timeout) ||
      (_rxSize && uint32_t(now - _packetSince) >= _timeout)) return fail();
  if (connected() && !_pingPending && !_txSize && _keepAlive && uint32_t(now - _lastTx) >= uint32_t(_keepAlive) * 500) {
    if (queuePacket(0xc0, "")) { _pingPending = true; _pingSince = now; }
  }
  return true;
}

using ArdMQTT = ArdMQTTClient<>;
