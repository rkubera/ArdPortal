#pragma once
#include "Arduino.h"
#ifndef FS_PHYS_SIZE
#define FS_PHYS_SIZE 65536U
#endif
constexpr int ESP_OK = 0, ESP_FAIL = -1;
constexpr int ESP_PARTITION_TYPE_DATA = 1, ESP_PARTITION_SUBTYPE_ANY = 0, ESP_PARTITION_SUBTYPE_DATA_SPIFFS = 0x82;
struct esp_partition_t {
  uint32_t size;
  bool encrypted;
};
inline const esp_partition_t* esp_partition_find_first(int, int, const char*) {
  static const esp_partition_t partition = { FS_PHYS_SIZE, false };
  return &partition;
}
inline int esp_partition_read(const esp_partition_t* p, size_t at, void* bytes, size_t n) {
  if (at > p->size || n > p->size - at || ardfsFlashFailure()) return ESP_FAIL;
  if (n && EspStub::badSector(ESP.badReadSectorsForTest, 0x100000U + at, n)) return ESP_FAIL;
  memcpy(bytes, ardfsTestFlash.data() + at, n);
  return ESP_OK;
}
inline int esp_partition_write(const esp_partition_t* p, size_t at, const void* bytes, size_t n) {
  if (at > p->size || n > p->size - at) return ESP_FAIL;
  return ESP.flashWrite(0x100000U + at, static_cast<const uint8_t*>(bytes), n) ? ESP_OK : ESP_FAIL;
}
inline int esp_partition_erase_range(const esp_partition_t* p, size_t at, size_t n) {
  if (at % 4096 || n != 4096 || at > p->size || n > p->size - at) return ESP_FAIL;
  return ESP.flashEraseSector((0x100000U + at) / 4096) ? ESP_OK : ESP_FAIL;
}
