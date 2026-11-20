// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>

struct ArdPortalConfig {
  String ssid, password, host, user, mqttPassword, deviceName, deviceDescription;
  String deviceManufacturer = "DYI";
  String apName = "ESP-Setup", apPassword = "1234567890";
  uint8_t apBand = 24;  // Preferred AP band: 24 = 2.4 GHz, 5 = 5 GHz.
  uint16_t port = 1883;
  bool mqttTls = false;
  String caCert;  // PEM root CA (or CA bundle), required when TLS is enabled.
#if ARDPORTAL_ENABLE_TIMEZONE_RULES
  // Two recurring local-time rules. Week 0 means the last occurrence.
  uint8_t timezoneRule1Week = 0, timezoneRule2Week = 0;
  uint8_t timezoneRule1DayOfWeek = 1, timezoneRule2DayOfWeek = 1;  // Sunday = 1.
  uint8_t timezoneRule1Month = 3, timezoneRule2Month = 10;
  uint8_t timezoneRule1Hour = 2, timezoneRule2Hour = 3;
  int8_t timezoneRule1Offset = 2, timezoneRule2Offset = 1;  // Hours east of UTC.
#endif
};

enum class ArdMqttState { Disabled,
                          WaitingForWifi,
                          WaitingRetry,
                          Connecting,
                          Connected,
                          WaitingForTime };
enum class ArdPortalChangeSource { Application,
                                   Portal,
                                   FactoryReset,
                                   Mqtt,
                                   Zigbee,
                                   Matter };

enum class ArdPortalRestartReason { Portal,
                                    FirmwareUpdate,
                                    FactoryReset,
                                    StorageFormat,
                                    ConfigurationRestore };
