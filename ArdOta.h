// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ArdPortalFeatures.h"
#if ARDPORTAL_ENABLE_OTA
#include <Arduino.h>
#include "PortalTypes.h"
class ArdPortal;
// Owns firmware validation and bounded upload writes. HTTP owns its connection.
class ArdOta {
public:
  ArdOta(const ArdOta&) = delete;
  ArdOta& operator=(const ArdOta&) = delete;
  ArdOta(ArdOta&&) = delete;
  ArdOta& operator=(ArdOta&&) = delete;
  bool active() const { return _otaActive; }
  size_t received() const { return _otaReceived; }
  size_t expected() const { return _otaExpected; }
private:
  friend class ArdPortal;
  friend class ArdMqtt;
  friend class ArdHomeAssistant;
  explicit ArdOta(ArdPortal& portal) : _portal(portal) {}
  ArdPortal& _portal;
  bool _otaActive = false;
  size_t _otaExpected = 0, _otaReceived = 0;
  void abortUpgrade();
  bool writeUpgrade(uint8_t* data, size_t length);
  void startUpload(uint32_t bodySize, int split);
  void receiveUpload();
};

#endif
