#pragma once
#include "ESP8266WiFi.h"
enum class DNSReplyCode { NoError };
struct DNSServer {
  bool active = false;
  unsigned requests = 0;
  void setErrorReplyCode(DNSReplyCode) {}
  bool start(uint16_t port, const char* wildcard, IPAddress) { active = port == 53 && String(wildcard) == "*"; return active; }
  void stop() { active = false; }
  void processNextRequest() { if (active) ++requests; }
};
