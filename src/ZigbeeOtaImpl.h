// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "LanguageLiterals.h"
#include <esp_ota_ops.h>
/** @brief Receive a standard Zigbee OTA image without restarting inside the SDK task. @param message Native OTA callback. @return Failure on identity, size, flash or image-validation error. */
esp_err_t ArdZigbee::otaAction(const esp_zb_zcl_ota_upgrade_value_message_t* message) {
  if (!message) return ESP_ERR_INVALID_ARG;
  auto fail = [&]() {
    if (_otaHandle) {
      esp_ota_abort(_otaHandle);
      _otaHandle = 0;
    }
    _otaVerified = false;
    _otaActive = false;
    traffic(ARD_LANG_NATIVE_ZIGBEE_OTA_FAILED_CURRENT_FIRMWARE_RETAINED);
    return ESP_FAIL;
  };
  auto verify = [&]() {
    if (_otaVerified) return true;
    if (!_otaHandle) return false;
    esp_err_t result = esp_ota_end(_otaHandle);
    _otaHandle = 0;
    _otaVerified = result == ESP_OK;
    return _otaVerified;
  };
  if (message->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) return fail();
  switch (message->upgrade_status) {
    case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_START:
      if (_otaActive || !_otaAllowed.exchange(false)) return ESP_FAIL;
      _otaPartition = esp_ota_get_next_update_partition(nullptr);
      if (!_otaPartition) return fail();
      _otaActive = true;
      _otaLastActivity = millis();
      _otaImage.reset();
      _otaHandle = 0;
      _otaVerified = false;
      if (esp_ota_begin(_otaPartition, OTA_WITH_SEQUENTIAL_WRITES, &_otaHandle) != ESP_OK) return fail();
      traffic(ARD_LANG_NATIVE_ZIGBEE_OTA_DOWNLOAD_STARTED);
      return ESP_OK;
    case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_RECEIVE:
      if (!_otaActive || !_otaHandle || !_otaImage.feed(message->payload, message->payload_size, _otaPartition->size, [&](const uint8_t* data, size_t bytes) {
            return esp_ota_write(_otaHandle, data, bytes) == ESP_OK;
          })) return fail();
      _otaLastActivity = millis();
      return ESP_OK;
    case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_CHECK: return _otaActive && _otaImage.complete() && verify() ? ESP_OK : fail();
    case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_APPLY: return _otaActive ? ESP_OK : ESP_FAIL;
    case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_FINISH:
      {
        if (!_otaActive || !_otaImage.complete() || !verify()) return fail();
        if (esp_ota_set_boot_partition(_otaPartition) != ESP_OK) return fail();
        _otaFinished = true;
        _otaActive = false;
        traffic(ARD_LANG_NATIVE_ZIGBEE_OTA_VERIFIED_PORTAL_RESTART_PENDING);
        return ESP_OK;
      }
    case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_ABORT:
    case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_ERROR: return fail();
    default: return ESP_OK;
  }
}
/** @brief Ask the coordinator OTA server for a matching newer image. @return True when a request was submitted. */
bool ArdZigbee::queryOta() {
  if (!_otaEndpoint || otaActive() || _state != State::Connected || !ArdHeap::permits(1024, 512) || !esp_zb_lock_acquire(0)) return false;
  esp_err_t result = esp_zb_ota_upgrade_client_query_interval_set(_otaEndpoint, 10);
  if (result == ESP_OK) result = esp_zb_ota_upgrade_client_query_image_req(0, 1);
  esp_zb_lock_release();
  if (result == ESP_OK) traffic(ARD_LANG_NATIVE_TX_ZIGBEE_OTA_IMAGE_QUERY_TO_COORDINATOR);
  return result == ESP_OK;
}
