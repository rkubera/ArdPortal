#pragma once
#include "Arduino.h"
#define WL_CONNECTED 3
#define WIFI_STA 1
#define WIFI_AP_STA 3
#define WIFI_SCAN_RUNNING -1
struct IPAddress {
  String text = "192.168.4.1";
  bool fromString(const String& s) { text = s; return s.indexOf('.') >= 0; }
  String toString() const { return text; }
};
struct Endpoint { std::deque<uint8_t> input; std::vector<uint8_t> output; bool connected = true; size_t writeLimit = 4096; };
class WiFiClient {
public:
  std::shared_ptr<Endpoint> endpoint;
  static bool connectResult;
  bool connect(IPAddress, uint16_t) { if (connectResult) endpoint = std::make_shared<Endpoint>(); return connectResult; }
  operator bool() const { return endpoint && endpoint->connected; }
  bool connected() const { return bool(*this); }
  int available() { return endpoint ? endpoint->input.size() : 0; }
  int read() { if (!available()) return -1; int c = endpoint->input.front(); endpoint->input.pop_front(); return c; }
  int availableForWrite() { return connected() ? endpoint->writeLimit : 0; }
  size_t write(const uint8_t* data, size_t size) { if (!connected()) return 0; size = std::min(size, endpoint->writeLimit); endpoint->output.insert(endpoint->output.end(), data, data + size); return size; }
  bool flush(unsigned) { return true; }
  void stop(unsigned) { abort(); }
  void abort() { if(endpoint) endpoint->connected = false; }
  void setTimeout(uint32_t) {}
  void setNoDelay(bool) {}
};
struct WiFiServer {
  static std::deque<WiFiClient> incoming;
  explicit WiFiServer(int) {}
  void begin() {}
  WiFiClient accept() { if(incoming.empty()) return {}; auto client = incoming.front(); incoming.pop_front(); return client; }
};
struct WiFiStub {
  int state = 0, scans = -2, attempts = 0, apStarts = 0, disconnects = 0;
  unsigned stations = 0;
  unsigned softAPgetStationNum() const { return stations; }
  bool softAPdisconnect(bool) { return true; }
  String apName, apPassword, hostName;
  void hostname(const char* name) { hostName = name; }
  int status() const { return state; }
  void persistent(bool) {}
  void setAutoReconnect(bool) {}
  void mode(int) {}
  void begin(const char*, const char*) { ++attempts; }
  void disconnect(bool = false, bool = false) { ++disconnects; state = 0; }
  bool softAP(const char* name, const char* password) { ++apStarts; apName = name; apPassword = password; return true; }
  IPAddress localIP() const { return {}; }
  IPAddress softAPIP() const { return {}; }
  String macAddress() const { return "AA:BB:CC:DD:EE:FF"; }
  int scanNetworks(bool) { scans = WIFI_SCAN_RUNNING; return scans; }
  int scanComplete() const { return scans; }
  void scanDelete() { scans = -2; }
  String SSID(int) const { return "test\"network"; }
  int encryptionType(int i) const { return i ? 8 : 7; }
  int RSSI(int) const { return -45; }
  bool hostByName(const char*, IPAddress&) { return true; }
};
extern WiFiStub WiFi;

constexpr int ENC_TYPE_NONE = 7;
