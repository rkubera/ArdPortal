// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#if defined(ESP8266) && __has_include(<include/ClientContext.h>)
#include <lwip/tcp.h>
#include <include/ClientContext.h>
#define ARDPORTAL_TCP_PRIORITY_AVAILABLE 1
#else
#define ARDPORTAL_TCP_PRIORITY_AVAILABLE 0
#endif
// Keep the broker connection above transient HTTP/WebSocket connections when
// ESP8266 lwIP must reclaim a TCP PCB. No additional connection buffer is used.
class ArdMqttTransport : public WiFiClient {
public:
  void protectConnection() {
#if ARDPORTAL_TCP_PRIORITY_AVAILABLE
    if(_client&&_client->getPCB())tcp_setprio(_client->getPCB(),TCP_PRIO_MAX);
#endif
  }
};
#undef ARDPORTAL_TCP_PRIORITY_AVAILABLE
