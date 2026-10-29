#include "ArdMQTT.h"
#include <cassert>
#include <deque>
#include <vector>
uint32_t fakeMillis = 1;
struct Socket : Client {
  bool online = true;
  size_t limit = 256;
  std::deque<uint8_t> rx;
  std::vector<uint8_t> tx;
  uint8_t connected() override {
    return online;
  }
  int available() override {
    return rx.size();
  }
  int read() override {
    if (rx.empty()) return -1;
    int b = rx.front();
    rx.pop_front();
    return b;
  }
  size_t write(const uint8_t* p, size_t n) override {
    n = std::min(n, limit);
    tx.insert(tx.end(), p, p + n);
    return n;
  }
  void stop() override {
    online = false;
  }
  void inject(std::initializer_list<uint8_t> data) {
    rx.insert(rx.end(), data);
  }
};
void open(ArdMQTT& mqtt, Socket& socket) {
  assert(mqtt.connect("standalone"));
  assert(!mqtt.connected() && mqtt.pending());
  assert(mqtt.loop() && socket.tx[0] == 0x10);
  socket.inject({ 0x20, 2, 0, 0 });
  assert(mqtt.loop() && mqtt.connected());
  socket.tx.clear();
}
int main() {
  Socket socket;
  ArdMQTT mqtt(socket);
  mqtt.setSocketTimeout(1);
  open(mqtt, socket);
  assert(mqtt.subscribe("arbitrary/device/#") && mqtt.busy());
  assert(!mqtt.subscribe("other"));
  assert(mqtt.loop());
  auto id1 = socket.tx[2], id2 = socket.tx[3];
  socket.tx.clear();
  socket.inject({ 0x90, 3, id1, id2, 0 });
  assert(mqtt.loop() && !mqtt.busy());
  assert(mqtt.unsubscribe("arbitrary/device/#"));
  assert(mqtt.loop() && socket.tx[0] == 0xa2);
  id1 = socket.tx[2];
  id2 = socket.tx[3];
  socket.inject({ 0xb0, 2, id1, id2 });
  assert(mqtt.loop() && !mqtt.busy());
  unsigned messages = 0;
  mqtt.setCallback([&](char* topic, uint8_t* data, unsigned len) {
    assert(String(topic) == "x");
    assert(len == 3 && data[0] == 'a' && data[1] == 0 && data[2] == 'b');
    assert(mqtt.retained());
    ++messages;
  });
  socket.inject({ 0x31, 6, 0 });
  assert(mqtt.loop() && mqtt.receiveBusy() && !messages);
  socket.inject({ 1, 'x', 'a', 0, 'b' });
  assert(mqtt.loop() && messages == 1 && !mqtt.receiveBusy());
  socket.tx.clear();
  socket.limit = 1;
  assert(mqtt.publish("x", "abc"));
  assert(!mqtt.publish("x", "busy"));
  while (mqtt.busy()) assert(mqtt.loop());
  assert(socket.tx.size() == 8 && socket.tx[0] == 0x30);
  mqtt.setKeepAlive(1);
  fakeMillis = 502;
  socket.limit = 256;
  assert(mqtt.loop() && mqtt.receiveBusy());
  assert(mqtt.loop());
  socket.inject({ 0xd0, 0 });
  assert(mqtt.loop() && !mqtt.receiveBusy());
  socket.inject({ 0x30, 0xff, 0xff, 0xff, 0xff });
  assert(!mqtt.loop() && !mqtt.connected() && !socket.online);
  Socket timeout;
  ArdMQTT waiting(timeout);
  waiting.setSocketTimeout(1);
  assert(waiting.connect("timeout"));
  fakeMillis += 1001;
  assert(!waiting.loop() && !timeout.online);
  Socket malformed;
  ArdMQTT bad(malformed);
  open(bad, malformed);
  malformed.inject({ 0x32, 3, 0, 1, 'x' });
  assert(!bad.loop());
  Socket refuse;
  ArdMQTT denied(refuse);
  assert(denied.connect("denied"));
  refuse.inject({ 0x20, 2, 0, 5 });
  assert(!denied.loop());
}
