// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ArdJSON.h"

namespace ArdMqttFieldTopics {
using V = ArdJSON::JSONVar;
// HA uses different names for the primary state topic of these components.
inline const char* discoveryStateKey(const V& field) {
  String type = field["type"].asString();
  return (type == "tag" || type == "device_trigger") ? "topic" : type == "lawn_mower" ? "activity_state_topic"
                                                                                      : "state_topic";
}
inline String stateTopic(const V& field, const String& fallback) {
  const V& mqtt = field["mqtt"];
  // An explicit MQTT setting takes precedence, including an empty/default value.
  String topic = mqtt.hasOwnProperty("state_topic") ? mqtt["state_topic"].asString() : field["ha"][discoveryStateKey(field)].asString();
  return !topic.length() || topic == "$state" ? fallback : topic;
}
}
