#pragma once
#include "ESP8266WiFi.h"
#include <time.h>
#define BR_TLS12 0x0303
namespace BearSSL {
struct X509List {
  bool valid;
  explicit X509List(const char* pem) : valid(strstr(pem, "CERTIFICATE") != nullptr) {}
  int getCount() const { return valid ? 1 : 0; }
};
}
struct WiFiClientSecure : WiFiClient {
  bool trusted = false;
  static int tlsAttempts, tlsWrites;
  static String lastHost;
  static bool tlsConnectResult;
  void setTrustAnchors(const BearSSL::X509List* ca) { trusted = ca && ca->getCount(); }
  void setX509Time(time_t) {}
  void setSSLVersion(int, int) {}
  bool connect(const char* host, uint16_t port) {
    ++tlsAttempts; lastHost = host;
    if (!trusted || !tlsConnectResult) return false;
    return WiFiClient::connect(IPAddress{}, port);
  }
  size_t write(const uint8_t* data, size_t size) { ++tlsWrites; return WiFiClient::write(data, size); }
  void stop() { abort(); }
};
