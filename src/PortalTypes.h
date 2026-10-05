// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>

struct ArdPortalConfig {
    String ssid, password, host, user, mqttPassword, deviceName, deviceDescription;
    String deviceManufacturer = "DYI";
    String apName = "ESP-Setup", apPassword = "1234567890";
    uint16_t port = 1883;
    bool mqttTls = false;
    String caCert; // PEM root CA (or CA bundle), required when TLS is enabled.
  };

enum class ArdMqttState { Disabled, WaitingForWifi, WaitingRetry, Connecting, Connected, WaitingForTime };
enum class ArdPortalChangeSource { Application, Portal, FactoryReset, Mqtt };
