// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <PubSubClient.h>

// Observe only the fixed header while forwarding reads. PubSubClient's callback
// omits RETAIN; the portal must still reject retained commands. No packet buffer.
class ArdPubSubTransport : public Client {
public:
  Client* client = nullptr;
  bool retained = false, pingPending = false;
  bool receiveBusy() const { return _remaining || _multiplier || pingPending; }
  void reset() { _remaining = _multiplier = 0; retained = pingPending = false; }
  int connect(IPAddress ip, uint16_t port) override { return client->connect(ip, port); }
  int connect(const char* host, uint16_t port) override { return client->connect(host, port); }
  size_t write(uint8_t byte) override { return client->write(byte); }
  size_t write(const uint8_t* data, size_t size) override { if (size && data[0] == 0xc0) pingPending = true; return client->write(data, size); }
  int available() override { return client->available(); }
  int peek() override { return client->peek(); }
  void flush() override { client->flush(); }
  void stop() override { if (client) client->stop(); reset(); }
  uint8_t connected() override { return client && client->connected(); }
  operator bool() override { return connected(); }
  int read() override {
    int byte = client->read();
    if (byte < 0) return byte;
    if (!_multiplier && !_remaining) { retained = byte & 1; if (byte == 0xd0) pingPending = false; _multiplier = 1; }
    else if (_multiplier) {
      _remaining += (byte & 127) * _multiplier;
      _multiplier = byte & 128 ? _multiplier * 128 : 0;
    } else --_remaining;
    return byte;
  }
  int read(uint8_t* data, size_t size) override {
    size_t count = 0;
    while (count < size) { int byte = read(); if (byte < 0) break; data[count++] = byte; }
    return count;
  }
private:
  uint32_t _remaining = 0, _multiplier = 0;
};

// Compile-time adapter: owns only PubSubClient and its header observer.
class ArdPubSubClient : public PubSubClient {
public:
  void setClient(Client& client) {
    _transport.client = &client;
    _transport.reset();
    PubSubClient::setClient(_transport);
  }
  bool receiveBusy() const { return _transport.receiveBusy(); }
  bool retained() const { return _transport.retained; }
private:
  ArdPubSubTransport _transport;
};
