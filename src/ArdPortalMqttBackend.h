// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ArdPortalFeatures.h"
#if ARDPORTAL_ENABLE_MQTT
#if ARDPORTAL_USE_PUBSUBCLIENT
#include "ArdPubSubClient.h"
using ArdPortalMqttBackend = ArdPubSubClient;
#else
#include "ArdMQTT.h"
#include <WiFiClient.h>
class ArdMqtt;
struct ArdPortalMqttIO {
  ArdMqtt* owner;
  bool ready() const {
    return true;
  }
  WiFiClient& transport();
  size_t write(const uint8_t* data, size_t length);
  void stop();
  void notify(char* topic, uint8_t* payload, unsigned int length);
};
using ArdPortalMqttBackend = ArdMQTTClient<ArdPortalMqttIO>;
#endif
#endif
