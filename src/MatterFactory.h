// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <initializer_list>
#include <nvs.h>
#include <nvs_flash.h>

namespace ArdMatterFactory {
/** @brief Check mandatory factory records without copying secrets or erasing NVS.
 * Cryptographic validation remains with the SDK/controller. Missing data stops
 * endpoint creation; it must never enable the example credentials provider. */
inline bool ready() {
  if (nvs_flash_init_partition("fctry") != ESP_OK) return false;
  nvs_handle_t handle;
  if (nvs_open_from_partition("fctry", "chip-factory", NVS_READONLY, &handle) != ESP_OK) return false;
  bool valid = true;
  for (const char* key : { "dac-cert", "dac-key", "dac-pub-key", "pai-cert", "cert-dclrn" }) {
    size_t length = 0;
    if (nvs_get_blob(handle, key, nullptr, &length) != ESP_OK || !length) valid = false;
  }
  for (const char* key : { "salt", "verifier", "vendor-name", "product-name" }) {
    size_t length = 0;
    if (nvs_get_str(handle, key, nullptr, &length) != ESP_OK || length <= 1) valid = false;
  }
  for (const char* key : { "discriminator", "iteration-count", "vendor-id", "product-id", "hardware-ver" }) {
    uint32_t value = 0;
    if (nvs_get_u32(handle, key, &value) != ESP_OK) valid = false;
  }
  nvs_close(handle);
  return valid;
}
}
