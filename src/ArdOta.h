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
  /**
   * @brief Disallow copying or moving this resource-owning instance.
   * Input: const ArdOta&.
   * @return No result; this operation is deleted and cannot be called.
   */
  ArdOta(const ArdOta&) = delete;
  /**
   * @brief Replace this instance state with the supplied value.
   * Input: const ArdOta&.
   * @return No result; this operation is deleted and cannot be called.
   */
  ArdOta& operator=(const ArdOta&) = delete;
  /**
   * @brief Disallow copying or moving this resource-owning instance.
   * Input: ArdOta&&.
   * @return No result; this operation is deleted and cannot be called.
   */
  ArdOta(ArdOta&&) = delete;
  /**
   * @brief Replace this instance state with the supplied value.
   * Input: ArdOta&&.
   * @return No result; this operation is deleted and cannot be called.
   */
  ArdOta& operator=(ArdOta&&) = delete;
  /**
   * @brief Check whether the component currently has an active operation.
   * @return True if the component currently has an active operation; false otherwise.
   */
  bool active() const { return _otaActive; }
  /**
   * @brief Read the received firmware byte count.
   * @return Number of firmware bytes received so far.
   */
  size_t received() const { return _otaReceived; }
  /**
   * @brief Read the expected firmware upload byte count.
   * @return Number of firmware bytes expected for the current upload.
   */
  size_t expected() const { return _otaExpected; }
private:
  friend class ArdPortal;
  friend class ArdMqtt;
  friend class ArdHomeAssistant;
  /**
   * @brief Initialize this instance and its owned state.
   * @param portal Owning portal instance.
   * @return No value.
   */
  explicit ArdOta(ArdPortal& portal) : _portal(portal) {}
  ArdPortal& _portal;
  bool _otaActive = false;
  size_t _otaExpected = 0, _otaReceived = 0;
#if defined(ESP8266)
  uint8_t _gzipPrefix[4] = {};
  uint8_t _gzipPrefixSize = 0;
#endif
  /**
   * @brief Abort the OTA update and release its transport state.
   * @return No value.
   */
  void abortUpgrade();
  /**
   * @brief Write firmware bytes to the OTA updater and check the result.
   * @param data Data buffer or value used by the operation.
   * @param length Number of bytes or elements to process.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool writeUpgrade(uint8_t* data, size_t length);
  /**
   * @brief Prepare the OTA updater for an incoming firmware upload.
   * @param bodySize Expected upload body length in bytes.
   * @param split Header/body boundary in the received HTTP request.
   * @return No value.
   */
  void startUpload(uint32_t bodySize, int split);
  /**
   * @brief Feed the next HTTP upload bytes into the OTA updater.
   * @return No value.
   */
  void receiveUpload();
};

#endif
