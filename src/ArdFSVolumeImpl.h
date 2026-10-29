#pragma once
// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#if defined(ESP8266)
#include <flash_hal.h>
#endif
#include <algorithm>
#include "ArdCRC.h"
// V4 frames continuation payloads with a type word; roots publish their type
// before payload. Catalog discovery can skip data without parsing user bytes.
namespace ArdFSDetail {
constexpr uint32_t BankMagic = 0x31445241, RecordMagic = 0x31524341, Commit = 0x51ac0ffe, DataMagic = 0x34445441;
constexpr uint32_t Sector = 4096, Header = 32;
inline bool erased(const uint8_t* bytes, size_t n) {
  while (n--)
    if (*bytes++ != 255) return false;
  return true;
}
// Fields are explicitly little endian, independent of compiler struct packing.
/**
 * @brief Encode a 32-bit word into little-endian journal bytes.
 * @param p Destination for four bytes.
 * @param n Word to encode.
 * @return No value.
 */
inline uint16_t get16(const uint8_t* p) {
  return uint16_t(p[0]) | uint16_t(p[1]) << 8;
}
inline void put16(uint8_t* p, uint16_t n) {
  p[0] = uint8_t(n);
  p[1] = uint8_t(n >> 8);
}
inline void put(uint8_t* p, uint32_t n) {
  for (int i = 0; i < 4; ++i) p[i] = uint8_t(n >> (8 * i));
}
/**
 * @brief Decode a little-endian 32-bit journal word.
 * @param p Pointer to four source bytes.
 * @return Decoded word.
 */
inline uint32_t get(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
// Header words are naturally aligned; keep generic get/put for arbitrary bytes.
inline uint32_t getWord(const uint8_t* p) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  uint32_t n;
  memcpy(&n, __builtin_assume_aligned(p, 4), 4);
  return n;
#else
  return get(p);
#endif
}
inline void putWord(uint8_t* p, uint32_t n) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  memcpy(__builtin_assume_aligned(p, 4), &n, 4);
#else
  put(p, n);
#endif
}

}
inline void ArdFSVolume::Index::clear() {
  while (head) {
    Entry* next = head->next;
    ArdFSVolume::destroyEntry(head);
    head = next;
  }
  count = 0;
}
inline ArdFSVolume::Entry* ArdFSVolume::Index::find(const char* path) const {
  for (Entry* e = head; e; e = e->next)
    if (!strcmp(e->path, path)) return e;
  return nullptr;
}
inline ArdFSVolume::Entry* ArdFSVolume::Index::at(size_t index) const {
  Entry* e = head;
  while (e && index--) e = e->next;
  return e;
}
inline ArdFSDetail::Entry* ArdFSDetail::Entry::create(ArdFSVolume* volume, size_t nameLength, size_t slots) {
  size_t nameBytes = (nameLength + 2) & ~size_t(1);
  void* memory = ArdAllocation::allocate(sizeof(Entry) + nameBytes + slots * sizeof(uint16_t));
  if (!memory) return nullptr;
  auto* entry = ::new (memory) Entry;
  entry->volume = volume;
  entry->path = static_cast<char*>(memory) + sizeof(Entry);
  entry->pathLength = nameLength;
  entry->path[nameLength] = 0;
  entry->sectors = slots ? reinterpret_cast<uint16_t*>(entry->path + nameBytes) : nullptr;
  return entry;
}
inline bool ArdFSVolume::Index::insert(Entry* entry) {
  Entry* old = find(entry->path);
  if (old) erase(entry->path);
  else if (count == 32) {
    ArdFSVolume::destroyEntry(entry);
    return false;
  }
  entry->next = head;
  head = entry;
  ++count;
  return true;
}
inline void ArdFSVolume::Index::erase(const char* path) {
  Entry** at = &head;
  while (*at && strcmp((*at)->path, path)) at = &(*at)->next;
  if (*at) {
    Entry* old = *at;
    *at = old->next;
    --count;
    ArdFSVolume::destroyEntry(old);
  }
}
inline void ArdFSVolume::Index::merge(Index& other) {
  while (other.head) {
    Entry* entry = other.head;
    other.head = entry->next;
    insert(entry);
  }
  other.count = 0;
}
/**
 * @brief Update the journal CRC-32 checksum over a byte range.
 * @param bytes Byte buffer to read or write.
 * @param length Number of bytes or elements to process.
 * @param c Current character or running CRC state, according to its type.
 * @return Updated CRC-32 accumulator; callers apply the final complement when committing a record.
 */
inline uint32_t ArdFSVolume::crc(const void* bytes, size_t length, uint32_t c) {
  auto p = static_cast<const uint8_t*>(bytes);
  for (size_t i = 0; i < length; ++i) {
    c = ArdCRC::updateByte(c, p[i]);
  }
  return c;
}
// Shared layout and atomic publication for record and bank headers.
inline uint32_t ArdFSVolume::makeHeader(uint8_t* bytes, uint32_t magic, uint32_t a, uint32_t b, uint32_t c) {
  memset(bytes, 255, ArdFSDetail::Header);
  ArdFSDetail::putWord(bytes, magic);
  ArdFSDetail::putWord(bytes + 4, a);
  ArdFSDetail::putWord(bytes + 8, b);
  ArdFSDetail::putWord(bytes + 12, c);
  return crc(bytes, 16);
}
inline int ArdFSVolume::commitStep(uint32_t address, uint8_t* scratch, uint8_t& stage) {
  uint8_t phase = stage - 5;
  if (phase > 1) return 1;
  ArdFSDetail::putWord(scratch, ArdFSDetail::Commit);
  int status = programStep(address + 24, scratch, 4, phase);
  stage = phase + 5;
  if (status < 0) _error = status;
  return status < 0 ? -1 : 0;
}
/**
 * @brief Locate and validate the flash region used by the two-bank journal.
 * @return True on success; false if validation, resource allocation or the operation fails.
 */
inline bool ArdFSVolume::configure() {
  if (_configured) return true;
#if defined(ESP8266)
  _address = FS_PHYS_ADDR;
  uint32_t size = FS_PHYS_SIZE;
  if (ESP.getFlashChipSize() > ESP.getFlashChipRealSize() || _address < ESP.getSketchSize() || _address > ESP.getFlashChipRealSize() || size > ESP.getFlashChipRealSize() - _address) return false;
#elif defined(ESP32)
  _partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "ardfs");
  if (!_partition) _partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "littlefs");
  if (!_partition) _partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "spiffs");
  if (!_partition || _partition->encrypted) {
    return false;
  }
  uint32_t size = _partition->size;
#endif
  _bankSize = (size / (2 * ArdFSDetail::Sector)) * ArdFSDetail::Sector;
  _configured = _bankSize >= 8 * ArdFSDetail::Sector && _bankSize / ArdFSDetail::Sector <= UINT16_MAX
#if defined(ESP8266)
                && _address % ArdFSDetail::Sector == 0
#endif
    ;
  return _configured;
}
inline bool ArdFSVolume::readPhysical(uint32_t address, void* bytes, size_t n) {
  uint32_t limit = _bankSize * 2;
  if (address > limit || n > limit - address) return false;
#if defined(ESP8266)
  alignas(4) uint32_t words[64];
  uint32_t absolute = _address + address, skip = absolute & 3U;
  if (n > sizeof(words) - skip) return false;
  size_t rounded = (skip + n + 3) & ~size_t(3);
  if (!ESP.flashRead(absolute - skip, words, rounded)) return false;
  memcpy(bytes, reinterpret_cast<uint8_t*>(words) + skip, n);
#else
  if (n > 256 || esp_partition_read(_partition, address, bytes, n) != ESP_OK) return false;
#endif
  yield();
  return true;
}
/**
 * @brief Write a bounded flash range in small transfers, yielding between chunks.
 * @param address Byte offset within the configured flash region.
 * @param bytes Byte buffer to read or write.
 * @param n Number of bytes or elements to process.
 * @return True if all requested bytes were written; false for invalid bounds or a flash error.
 */
inline bool ArdFSVolume::writePhysical(uint32_t address, const void* bytes, size_t n) {
  uint32_t limit = _bankSize * 2;
  if (n > 256 || address > limit || n > limit - address) return false;
#if defined(ESP8266)
  bool ok = ESP.flashWrite(_address + address, static_cast<const uint8_t*>(bytes), n);
#else
  bool ok = esp_partition_write(_partition, address, bytes, n) == ESP_OK;
#endif
  yield();
  return ok;
}
/**
 * @brief Erase one physical flash sector and yield.
 * @param address Byte offset within the configured flash region.
 * @return True if the sector was erased; false for invalid alignment, bounds or a flash error.
 */
inline bool ArdFSVolume::erasePhysical(uint32_t address) {
  uint32_t limit = _bankSize * 2;
  if (address % ArdFSDetail::Sector || address > limit || ArdFSDetail::Sector > limit - address) return false;
#if defined(ESP8266)
  bool ok = ESP.flashEraseSector((_address + address) / ArdFSDetail::Sector);
#else
  bool ok = esp_partition_erase_range(_partition, address, ArdFSDetail::Sector) == ESP_OK;
#endif
  // Count attempted transaction erases conservatively, including retries and
  // metadata. Formatting is excluded; the next catalog checkpoints the count.
  if (_transaction) {
    auto& wear = _maps[address >= _bankSize].erases;
    if (wear != UINT32_MAX) ++wear;
  }
  yield();
  return ok;
}
// Each bank owns its map. Logical addresses remain stable when an unused tail
// sector is retired; existing committed records are never shifted in place.
inline void ArdFSVolume::clearMaps() {
  memset(_maps, 0, sizeof(_maps));
  _maps[0].slot = _maps[1].slot = UINT32_MAX;
  for (auto& map : _maps) {
    map.meta[0] = 0;
    map.meta[1] = 1;
  }
  _maps[0].nextSector = _maps[1].nextSector = 2;
}
__attribute__((noinline)) inline uint32_t ArdFSVolume::bankLimit(const BankMap& map) const {
  unsigned reserved = (map.meta[0] != UINT16_MAX) + (map.meta[1] != UINT16_MAX);
  return _bankSize - (reserved - 1U + map.count) * ArdFSDetail::Sector;
}
inline int ArdFSVolume::decodeMap(uint8_t* h, BankMap& map, uint32_t bank, uint32_t slot) {
  if (ArdFSDetail::getWord(h) != ArdFSDetail::BankMagic || ArdFSDetail::getWord(h + 24) != ArdFSDetail::Commit) return -1;
  if (ArdFSDetail::getWord(h + 4) != 4 || (slot & 1) || h[32] > 32 || h[33] || h[34] > 32 || ArdFSDetail::getWord(h + 164) != 0x31524c57U) return -1;
  uint32_t checksum = ArdFSDetail::getWord(h + 16);
  ArdFSDetail::putWord(h + 16, UINT32_MAX);
  ArdFSDetail::putWord(h + 24, UINT32_MAX);
  uint32_t actual = ~crc(h, 256);
  ArdFSDetail::putWord(h + 16, checksum);
  ArdFSDetail::putWord(h + 24, ArdFSDetail::Commit);
  if (checksum != actual || ArdFSDetail::getWord(h + 12) != _bankSize || ArdFSDetail::getWord(h + 180) != bank + slot * 128U) return -1;
  map.count = h[32];
  for (unsigned i = 0; i < h[34]; ++i)
    if (ArdFSDetail::get16(h + 100 + i * 2) >= _bankSize / ArdFSDetail::Sector) return -1;
  for (unsigned i = 0; i < map.count; ++i) {
    map.bad[i] = ArdFSDetail::get16(h + 36 + i * 2);
    if (map.bad[i] >= _bankSize / ArdFSDetail::Sector || (i && map.bad[i] <= map.bad[i - 1])) return -1;
  }
  map.revision = ArdFSDetail::getWord(h + 20);
  map.nextSector = ArdFSDetail::get16(h + 168);
  map.erases = ArdFSDetail::getWord(h + 172);
  if (map.nextSector >= _bankSize / ArdFSDetail::Sector) return -1;
  for (unsigned i = 0; i < 2; ++i) {
    map.meta[i] = ArdFSDetail::get16(h + 176 + i * 2);
    if (map.meta[i] != UINT16_MAX && (map.meta[i] >= _bankSize / 4096 || isBad(map, map.meta[i]))) return -1;
  }
  if (map.count + (map.meta[0] != UINT16_MAX) + (map.meta[1] != UINT16_MAX) > _bankSize / 4096) return -1;
  if (map.meta[0] == map.meta[1] || (map.meta[0] != slot / 32 && map.meta[1] != slot / 32)) return -1;
  map.rotate = false;
  map.valid = true;
  map.slot = slot;
  map.generation = ArdFSDetail::getWord(h + 8);
  return 1;
}
inline void ArdFSVolume::encodeMap(uint8_t* h, uint32_t slot) const {
  const size_t bytes = 256;
  memset(h, 255, bytes);
  ArdFSDetail::putWord(h, ArdFSDetail::BankMagic);
  ArdFSDetail::putWord(h + 4, 4);
  ArdFSDetail::putWord(h + 8, _workMap->generation);
  ArdFSDetail::putWord(h + 12, _bankSize);
  ArdFSDetail::putWord(h + 20, _workMap->revision);
  h[32] = _workMap->count;
  h[33] = 0;
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  memcpy(h + 36, _workMap->bad, _workMap->count * sizeof(_workMap->bad[0]));
#else
  for (unsigned i = 0; i < _workMap->count; ++i) {
    h[36 + i * 2] = uint8_t(_workMap->bad[i]);
    h[37 + i * 2] = uint8_t(_workMap->bad[i] >> 8);
  }
#endif
  h[34] = _catalogCount;
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  memcpy(h + 100, _catalog, _catalogCount * sizeof(uint16_t));
#else
  for (unsigned i = 0; i < _catalogCount; ++i) ArdFSDetail::put16(h + 100 + i * 2, _catalog[i]);
#endif
  ArdFSDetail::putWord(h + 164, 0x31524c57U);
  ArdFSDetail::put16(h + 168, _workMap->nextSector);
  ArdFSDetail::putWord(h + 172, _workMap->erases);
  for (unsigned i = 0; i < 2; ++i) ArdFSDetail::put16(h + 176 + i * 2, _workMap->meta[i]);
  ArdFSDetail::putWord(h + 180, _workBank + slot * 128U);
  ArdFSDetail::putWord(h + 16, ~crc(h, bytes));
}
inline void ArdFSVolume::cancelMap(ArdFSFile::MapCommit& h) {
  if (!h.rotating) return;
  uint16_t id = h.slot / 32;
  if (!isBad(*_workMap, id)) _uses[_workBank != 0].get()[id] = 0;
  _workMap->meta[h.target] = h.oldSector;
  if (h.oldSector != UINT16_MAX && !isBad(*_workMap, h.oldSector)) _uses[_workBank != 0].get()[h.oldSector] = 255;
  h.rotating = false;
}
inline void ArdFSVolume::completeMap(ArdFSFile::MapCommit& h) {
  if (h.rotating && h.oldSector != UINT16_MAX && !isBad(*_workMap, h.oldSector))
    _uses[_workBank != 0].get()[h.oldSector] = 0;
  h.rotating = h.uncertain = false;
  _workMap->slot = h.slot;
  _workMap->rotate = false;
  _workMap->valid = true;
  if (_transaction && !_transaction->published) publishFragments(*_transaction);
}
inline bool ArdFSVolume::nextMapSector(ArdFSFile::MapCommit& h) {
  uint16_t id = h.slot / 32;
  if (!retireSector(id)) return false;
  if (h.rotating) {
    _workMap->meta[h.target] = h.oldSector;
    if (h.oldSector != UINT16_MAX && !isBad(*_workMap, h.oldSector)) _uses[_workBank != 0].get()[h.oldSector] = 255;
  } else _workMap->meta[h.target] = UINT16_MAX;
  h.rotating = false;
  h.left = _bankSize / ArdFSDetail::Sector;
  h.stage = 9;
  return true;
}
inline int ArdFSVolume::publishMapStep(ArdFSFile::MapCommit& h, uint8_t* buffer) {
  if (!h.stage) {
    h.target = _workMap->slot == UINT32_MAX ? 0 : (_workMap->meta[0] == _workMap->slot / 32 ? 1 : 0);
    unsigned within = _workMap->slot == UINT32_MAX ? 0 : _workMap->slot % 32 + (h.mirror ? 0 : 2);
    h.left = _bankSize / ArdFSDetail::Sector;
    if (within >= 32 || _workMap->meta[h.target] == UINT16_MAX) h.stage = 9;
    else {
      h.slot = _workMap->meta[h.target] * 32U + within;
      h.stage = _workMap->slot == UINT32_MAX ? 1 : 2;
    }
    return 0;
  }
  uint32_t address = _workBank + h.slot * 128U;
  bool ok = true;
  switch (h.stage) {
    case 9:
      {
        uint16_t id;
        int status = allocateSector(h.left, id);
        if (status < 0) _workMap->rotate = true;
        if (status != 1) return status;
        h.oldSector = _workMap->meta[h.target];
        if (h.oldSector != UINT16_MAX) _uses[_workBank != 0].get()[h.oldSector] = 1;
        _workMap->meta[h.target] = id;
        _uses[_workBank != 0].get()[id] = 255;
        h.slot = id * 32U;
        h.rotating = true;
        h.stage = 1;
        return 0;
      }
    case 1:
    case 8:
      {
        if (h.stage == 1) h.at = UINT16_MAX;
        int status = eraseStep(address, h.at);
        ok = status >= 0;
        h.stage = status == 1 ? 2 : 8;
        break;
      }
    case 2:
      ok = readPhysical(address, buffer, 256);
      if (!ok) break;
      if (!ArdFSDetail::erased(buffer, 256)) {
        if (h.slot % 32 == 30) h.stage = 9;
        else h.slot += 2;
        return 0;
      }
      // Reserve a sequence per attempt, including mirrors and retries.
      ++_workMap->revision;
      encodeMap(buffer, h.slot);
      h.stage = 10;
      return 0;
    case 10:
    case 11:
      {
        int status = programStep(address, buffer, 256, h.stage);
        ok = status >= 0;
        if (status == 1) h.stage = 5;
        break;
      }
    case 5:
    case 6:
      {
        uint8_t previous = h.stage;
        ok = commitStep(address, buffer, h.stage) >= 0;
        if (!ok && previous == 5) {
          h.stage = 6;
          return 0;
        }
        if (!ok && _error == -5) h.uncertain = true;
        break;
      }
    default:
      completeMap(h);
      if (!h.mirror) {
        h.mirror = true;
        h.stage = 0;
        return 0;
      }
      return 1;
  }
  if (!ok) return nextMapSector(h) ? 0 : -1;
  return 0;
}

// Synchronous compatibility wrappers drain the same bounded state machines.
inline bool ArdFSVolume::begin() {
  int status;
  do { status = beginStep(); } while (status == 0);
  return status == 1;
}
inline int ArdFSVolume::beginStep() {
  if (_mountStage == 255) return -1;
  int status = mountStep();
  if (status < 0) _mountStage = 255;
  return status;
}
inline int ArdFSVolume::mountStep() {
  if (_mounted) return 1;
  alignas(4) uint8_t bytes[256];
  if (_mountStage >= 9) return mountFragments(bytes);
  switch (_mountStage) {
    case 0:
      if (!configure()) {
        _error = -22;
        return -1;
      }
      clearMaps();
      _uses[0].reset();
      _uses[1].reset();
      _headerScan = 0;
      _scanAt = 0;
      _checkDamage = false;
      _headerObserved = false;
      _entries.clear();
      _scanEntry.reset();
      _mountStage = 1;
      return 0;
    case 1:
    case 2:
      {
        const uint32_t pages = _bankSize / 256;
        if (_headerScan == pages * 2) {
          _scanAt = 0;
          _mountStage = 3;
          return 0;
        }
        uint32_t page = _headerScan++, bank = page / pages, slot = (page % pages) * 2;
        uint16_t sector = slot / 32;
        if (!readPhysical(page * 256, bytes, 256)) {
          if (!_uses[bank]) {
            _uses[bank].reset(static_cast<uint8_t*>(ArdAllocation::allocate(_bankSize / 4096)));
            if (!_uses[bank]) {
              _error = -12;
              return -1;
            }
            memset(_uses[bank].get(), 0, _bankSize / 4096);
          }
          _uses[bank].get()[sector] = 1;
          return 0;
        }
        if (!ArdFSDetail::erased(bytes, 256)) _headerObserved = true;
        uint32_t magic = ArdFSDetail::getWord(bytes);
        // Only a catalog/erased first word can introduce further catalog slots.
        // An unrecognized type may be a damaged data tag: never scan its payload.
        bool skip = !(slot % 32) && magic != ArdFSDetail::BankMagic && magic != UINT32_MAX;
        if (skip) {
          _headerScan += 15;
          if (magic == ArdFSDetail::RecordMagic || magic == ArdFSDetail::DataMagic) return 0;
        }
        bool committed = ArdFSDetail::getWord(bytes + 24) == ArdFSDetail::Commit && (ArdFSDetail::getWord(bytes) == ArdFSDetail::BankMagic || ArdFSDetail::getWord(bytes + 4) == 4);
        if (committed) {
          BankMap decoded;
          int valid = decodeMap(bytes, decoded, bank * _bankSize, slot);
          if (_mountStage == 1) {
            auto& old = _maps[bank];
            if (valid > 0 && (!old.valid || int32_t(decoded.generation - old.generation) > 0 || (decoded.generation == old.generation && int32_t(decoded.revision - old.revision) > 0))) memcpy(&old, &decoded, sizeof(old));
            else if (valid < 0) _checkDamage = true;
          } else if (valid < 0 && !isBad(_maps[bank], sector)) {
            unsigned first = _maps[1].valid && (!_maps[0].valid || int32_t(_maps[1].generation - _maps[0].generation) > 0);
            uint32_t g = ArdFSDetail::getWord(bytes + 8), r = ArdFSDetail::getWord(bytes + 20);
            if (!_maps[first].valid || int32_t(g - _maps[first].generation) > 0 || (g == _maps[first].generation && int32_t(r - _maps[first].revision) > 0)) {
              _error = -5;
              return -1;
            }
          }
        }
        return 0;
      }
    case 3:
      {
        uint32_t sectors = _bankSize / 4096;
        unsigned budget = 256;
        while (_scanAt < sectors * 2 && budget--) {
          unsigned bank = _scanAt / sectors;
          uint16_t id = _scanAt++ % sectors;
          if (_uses[bank] && _uses[bank].get()[id] && !isBad(_maps[bank], id)) {
            _error = -5;
            return -1;
          }
        }
        if (_scanAt < sectors * 2) return 0;
        if (_checkDamage) {
          _checkDamage = false;
          _headerScan = 0;
          _mountStage = 2;
          return 0;
        }
        unsigned first = _maps[1].valid && (!_maps[0].valid || int32_t(_maps[1].generation - _maps[0].generation) > 0);
        if (!_maps[first].valid) {
          // All untyped pages were scanned; a typed sector itself is nonblank.
          _error = _headerObserved ? -5 : -84;
          return -1;
        }
        _uses[0].reset();
        _uses[1].reset();
        _active = first * _bankSize;
        _generation = _maps[first].generation;
        _mountStage = 9;
        return 0;
      }
    default:
      _scanEntry.reset();
      _mounted = true;
      _error = 0;
      return 1;
  }
}
inline bool ArdFSVolume::format() {
  int status;
  do { status = formatStep(); } while (status == 0);
  return status == 1;
}
inline bool ArdFSFile::Writer::allocateBuffer() {
  ownedBuffer.reset(new (std::nothrow) SectorBuffer);
  sectorBuffer = ownedBuffer.get();
  return sectorBuffer != nullptr;
}
inline int ArdFSVolume::formatStep() {
  switch (_formatStage) {
    case 0:
      if (!configure()) {
        _error = -22;
        return -1;
      }
      end();
      _formatBuffer.reset(new (std::nothrow) ArdFSFile::MapBuffer);
      if (!_formatBuffer) {
        _error = -12;
        return -1;
      }
      _formatMap = {};
      clearMaps();
      for (auto& map : _maps) {
        map.meta[0] = map.meta[1] = UINT16_MAX;
        map.nextSector = 0;
      }
      _catalogCount = 0;
      _mounted = false;
      _mountStage = 0;
      _formatAt = 0;
      _formatStage = 1;
      return 0;
    case 1:
      if (_formatAt == _bankSize * 2) {
        _formatStage = 2;
        return 0;
      }
      _formatMap.at = UINT16_MAX;
      _formatStage = 4;
      return 0;
    case 4:
      {
        int status = eraseStep(_formatAt, _formatMap.at);
        if (!status) return 0;
        if (status < 0) {
          _workBank = _formatAt >= _bankSize ? _bankSize : 0;
          _workMap = &_maps[_workBank != 0];
          if (!retireSector((_formatAt - _workBank) / 4096)) {
            _formatBuffer.reset();
            _formatStage = 0;
            _error = -5;
            return -1;
          }
        }
        _formatAt += ArdFSDetail::Sector;
        _formatStage = 1;
        return 0;
      }
    case 2:
      _workBank = _maps[0].count + 3U > _bankSize / 4096 ? _bankSize : 0;
      _workMap = &_maps[_workBank != 0];
      _workMap->generation = 1;
      if (_workMap->count + 3U > _bankSize / 4096 || !initializeUses(_workBank)) {
        _formatBuffer.reset();
        _formatStage = 0;
        _error = -5;
        return -1;
      }
      _formatStage = 3;
      return 0;
    default:
      {
        int status = publishMapStep(_formatMap, _formatBuffer->data);
        if (status < 0) {
          _formatBuffer.reset();
          _formatStage = 0;
          _error = -5;
          return -1;
        }
        if (!status) return 0;
        _formatBuffer.reset();
        _formatStage = 0;
        _error = 0;
        return 1;
      }
  }
}
/**
 * @brief Check whether the mounted journal contains the requested path.
 * @param path Journal file path.
 * @return True if the path exists; false if missing or not mounted.
 */
inline bool ArdFSVolume::exists(const char* path) {
  bool found = _mounted && _entries.find(path);
  _error = found ? 0 : -2;
  return found;
}
inline int ArdFSVolume::prepareStep(ArdFSFile::Writer& h) {
  switch (h.prepareStage) {
    case 0: return prepareFragment(h);
    case 1:
      if (h.workAt == 2) {
        h.workAt = 0;
        h.prepareStage = 13;
        h.allocationLeft = _bankSize / 4096;
        return 0;
      }
      {
        uint16_t id = _workMap->meta[h.workAt];
        if (id == UINT16_MAX || isBad(*_workMap, id)) {
          _workMap->meta[h.workAt++] = UINT16_MAX;
          return 0;
        }
        if (h.reserveStage) {
          if (checkPhysical(h.bank + id * 4096U + (h.reserveStage - 1) * 256U, nullptr, 256) < 0) return -1;
          if (++h.reserveStage <= 16) return 0;
          h.reserveStage = 0;
        } else {
          if (!erasePhysical(h.bank + id * 4096U)) {
            if (!retireSector(id)) return -1;
          } else if (h.reusingBank) {
            h.reserveStage = 1;
            return 0;
          }
        }
        if (!isBad(*_workMap, id)) _uses[h.bank != 0].get()[id] = 0;
        _workMap->meta[h.workAt++] = UINT16_MAX;
        return 0;
      }
    case 13:
      {
        uint16_t id;
        int status = allocateSector(h.allocationLeft, id);
        if (status != 1) return status;
        _uses[h.bank != 0].get()[id] = 255;
        _workMap->meta[h.workAt++] = id;
        if (h.workAt == 2) {
          h.workAt = 0;
          h.prepareStage = 2;
        }
        return 0;
      }
    case 2:
      {
        // At most 32 paths; metadata work is independent of file sizes.
        while (h.copyIndex < _entries.count && !h.copyPath(_entries.at(h.copyIndex)->path)) ++h.copyIndex;
        if (h.copyIndex == _entries.count) {
          h.prepareStage = 8;
          return 0;
        }
        const Entry& e = *_entries.at(h.copyIndex);
        h.copy.reset(ArdFSFile::Writer::create(h.copyPath(e.path)));
        if (!h.copy) return -1;
        auto& copy = *h.copy;
        copy.volume = this;
        copy.sectorBuffer = h.sectorBuffer;
        copy.bank = h.bank;
        copy.capacity = e.length;
        copy.expected = e.length;
        copy.prepareStage = 8;
        h.prepareStage = 3;
        return 0;
      }
    case 3:
      if (!sourceChecksum(*_entries.at(h.copyIndex), h.bytes, h.copy->verified)) return -1;
      h.prepareStage = 4;
      return 0;
    case 4:
      {
        int status = prepareFragment(*h.copy);
        if (status < 0) return -1;
        if (status == 1) h.prepareStage = 5;
        return 0;
      }
    case 5:
      {
        int status = copyPayload(*h.copy, *_entries.at(h.copyIndex));
        if (status == 1) h.prepareStage = 7;
        return status < 0 ? -1 : 0;
      }
    case 7:
      {
        int status = finishFragment(*h.copy);
        if (status < 0) return -1;
        if (status == 1) {
          h.copy.reset();
          ++h.copyIndex;
          h.prepareStage = 2;
        }
        return 0;
      }
    case 8:
    case 9:
    case 11:
    case 12: return prepareFragment(h);
    default: return 1;
  }
}

inline void ArdFSVolume::bufferInput(ArdFSFile::Writer& h, const uint8_t* bytes, size_t n) {
  if (h.bytes != bytes) memcpy(h.bytes, bytes, n);
  h.pendingAt = 0;
  h.pendingSize = n;
  h.checksum = crc(bytes, n, h.checksum);
  h.length += n;
}
// Keep only the current 256-byte page. Verified prefixes remain in flash.
// Relocation copies that prefix through a second bounded scratch page.
inline void ArdFSVolume::startSectorBuffer(ArdFSFile::Writer& h) {
  h.sectorStage = 2;
  h.sectorAddress = h.entry->address & ~4095U;
  h.sectorUsed = h.entry->dataOffset;
  h.sectorAt = h.sectorUsed & ~255U;
  memset(h.sectorBuffer->data, 0xff, 256);
  if (!h.sectorAt) ArdFSDetail::putWord(h.sectorBuffer->data, ArdFSDetail::RecordMagic);
}
// One bounded read-back implementation for erased pages and programmed data.
__attribute__((noinline)) inline int ArdFSVolume::checkPhysical(uint32_t address, const void* expected, size_t n) {
  alignas(4) uint8_t bytes[256];
  if (!readPhysical(address, bytes, n)) return -5;
  return (expected ? !memcmp(bytes, expected, n) : ArdFSDetail::erased(bytes, n)) ? 1 : -84;
}
// These bounded primitives serve file pages, catalog pages and formatting.
inline int ArdFSVolume::eraseStep(uint32_t address, uint16_t& at) {
  if (at == UINT16_MAX) {
    if (!erasePhysical(address)) return -1;
    at = 0;
    return 0;
  }
  int status = checkPhysical(address + at, nullptr, 256);
  if (status < 0) return status;
  at += 256;
  return at == ArdFSDetail::Sector ? 1 : 0;
}
inline int ArdFSVolume::programStep(uint32_t address, const void* bytes, size_t n, uint8_t& stage) {
  bool verify = stage & 1;
  int status = verify ? checkPhysical(address, bytes, n) : (writePhysical(address, bytes, n) ? 1 : -5);
  if (status < 0) return status;
  ++stage;
  return verify ? 1 : 0;
}
inline int ArdFSVolume::retrySector(ArdFSFile::Writer& h) {
  bool replay = h.sectorStage & 16;
  uint8_t phase = h.sectorStage & 15;
  bool erase = phase == 2 || phase == 8;
  if (++h.sectorRetries < 2) {
    h.sectorStage = (replay ? 16 : 0) | (erase ? 2 : (phase == 10 || phase == 11 ? 10 : 0));
    return 0;
  }
  if (!retireFragment(h)) return -1;
  if (!replay) {
    h.replaySource = h.sectorAddress;
    h.replayLimit = h.publishingRoot ? h.sectorUsed : h.sectorAt;
  }
  h.sectorRetries = 0;
  h.sectorStage = 3;
  return 0;
}
inline int ArdFSVolume::drainSectorStep(ArdFSFile::Writer& h) {
  auto* scratch = h.sectorBuffer->replay;
  bool replay = h.sectorStage & 16;
  uint8_t phase = h.sectorStage & 15;
  if (phase == 3) {
    uint16_t sector;
    int status = allocateFragment(h, sector);
    if (status != 1) return status;
    h.entry->sectors[h.node] = sector;
    h.sectorAddress = h.bank + sector * ArdFSDetail::Sector;
    if (!h.node) {
      h.entry->address = h.sectorAddress + h.entry->dataOffset;
    }
    if (h.publishingRoot) buildFragmentHeader(h);
    h.sectorStage = 18;
    return 0;
  }
  if (phase == 2 || phase == 8) {
    if (phase == 2) h.replayAt = UINT16_MAX;
    int status = eraseStep(h.sectorAddress, h.replayAt);
    if (status < 0) return retrySector(h);
    if (status == 1) {
      h.replayAt = !h.publishingRoot && !h.node ? h.entry->dataOffset & ~255U : 0;
      h.sectorStage = !h.publishingRoot ? (replay ? 26 : 10) : (replay ? 21 : 0);
      h.sectorRetries = 0;
    } else h.sectorStage = (replay ? 16 : 0) | 8;
    return 0;
  }
  if (phase == 10 || phase == 11) {
    ArdFSDetail::putWord(scratch, h.node ? ArdFSDetail::DataMagic : ArdFSDetail::RecordMagic);
    int status = programStep(h.sectorAddress, scratch, 4, h.sectorStage);
    if (status < 0) return retrySector(h);
    if (status == 1) h.sectorStage = replay ? 21 : 0;
    return 0;
  }
  if (phase == 5) {
    if (h.replayAt >= h.replayLimit) {
      h.sectorRetries = 0;
      if (h.publishingRoot) {
        h.sectorDone = true;
        return 1;
      }
      h.sectorStage = 0;
      return 0;
    }
    size_t n = std::min<size_t>(256, h.replayLimit - h.replayAt);
    memset(scratch, 255, 256);
    if (h.replayAt == h.sectorAt) memcpy(scratch, h.sectorBuffer->data, n);
    else if (!readPhysical(h.replaySource + h.replayAt, scratch, n)) {
      _error = -5;
      return -1;
    }
    if (h.publishingRoot) overlayFragmentHeader(h, scratch, h.replayAt, n);
    h.sectorStage = 16;
    return 0;
  }
  if (phase == 9) {
    if (!readPhysical(h.sectorAddress + h.sectorAt, h.sectorBuffer->data, 256)) return -1;
    overlayFragmentHeader(h, h.sectorBuffer->data, h.sectorAt, 256);
    h.sectorStage = 0;
    return 0;
  }
  // Normal pages and prefix replay use the same bounded program/verify path.
  uint16_t at = replay ? h.replayAt : h.sectorAt;
  size_t remaining = replay ? h.replayLimit - at : h.sectorUsed - at;
  size_t n = !replay && h.publishingRoot ? 256 : (std::min<size_t>(256, remaining) + 3) & ~size_t(3);
  const uint8_t* bytes = replay ? scratch : h.sectorBuffer->data;
  int status = programStep(h.sectorAddress + at, bytes, n, h.sectorStage);
  if (status < 0) return retrySector(h);
  if (!status) return 0;
  if (replay) {
    h.replayAt += n;
    h.sectorStage = 21;
    return 0;
  }
  h.sectorAt += n;
  h.sectorRetries = 0;
  if (h.publishingRoot) {
    if (h.sectorAt >= ArdFSDetail::Header + h.pathLength + h.entry->count * 2U) {
      h.sectorDone = true;
      return 1;
    }
    h.sectorStage = 9;
    return 0;
  }
  memset(h.sectorBuffer->data, 255, 256);
  h.sectorStage = 0;
  if (h.sectorFinal) {
    h.sectorDone = true;
    return 1;
  }
  if (h.sectorAt == ArdFSDetail::Sector) {
    ++h.node;
    h.sectorUsed = 4;
    h.sectorAt = 0;
    ArdFSDetail::putWord(h.sectorBuffer->data, ArdFSDetail::DataMagic);
    h.sectorStage = 2;
  }
  return 1;
}
inline int ArdFSVolume::writeBufferedStep(ArdFSFile::Writer& h, bool final) {
  if (h.sectorDone) return 1;
  if (h.node == h.entry->count) {
    if (h.pendingAt == h.pendingSize) {
      if (final) h.sectorDone = true;
      return 1;
    }
    if (h.node >= h.extent / ArdFSDetail::Sector) return -1;
    uint16_t sector;
    int status = allocateFragment(h, sector);
    if (status != 1) return status;
    h.entry->sectors[h.entry->count++] = sector;
    h.sectorAddress = h.bank + sector * ArdFSDetail::Sector;
    return 0;
  }
  if (h.sectorStage || h.sectorUsed - h.sectorAt == 256 || h.sectorFinal) return drainSectorStep(h) < 0 ? -1 : 0;
  if (h.pendingAt < h.pendingSize) {
    size_t n = std::min<size_t>(h.pendingSize - h.pendingAt, 256 - (h.sectorUsed - h.sectorAt));
    memcpy(h.sectorBuffer->data + (h.sectorUsed - h.sectorAt), h.bytes + h.pendingAt, n);
    h.pendingAt += n;
    h.sectorUsed += n;
    return 0;
  }
  h.pendingAt = h.pendingSize = 0;
  if (final) {
    // An empty record still needs its root sector erased before publication.
    if (h.sectorUsed == h.sectorAt) {
      h.sectorDone = true;
      return 1;
    }
    h.sectorFinal = true;
    return 0;
  }
  return 1;
}

inline ArdFSFile ArdFSVolume::open(const char* path, const char* mode, size_t length, bool deferred) {
  ArdFSFile f;
  if (!_mounted || !path || !*path || strlen(path) > 72 || !mode || (*mode != 'r' && *mode != 'w') || (*mode == 'w' && _writer)) {
    _error = -22;
    return f;
  }
  if (*mode == 'r') {
    const Entry* e = _entries.find(path);
    if (!e) {
      _error = -2;
      return f;
    }
    unsigned bank = e->address >= _bankSize;
    if (_readers[bank] == UINT16_MAX) {
      _error = -24;
      return f;
    }
    auto* uses = _uses[bank].get();
    for (unsigned i = 0; i < e->count; ++i)
      if (!uses || uses[e->sectors[i]] >= 254) {
        _error = -24;
        return f;
      }
    void* memory = ArdAllocation::allocate(sizeof(ArdFSFile::Handle) + e->count * sizeof(uint16_t));
    if (!memory) {
      _error = -12;
      return f;
    }
    auto* h = ::new (memory) ArdFSFile::Handle;
    h->volume = this;
    h->address = e->address;
    h->length = e->length;
    h->count = e->count;
    h->epoch = _epoch;
    if (e->count) memcpy(h->sectors(), e->sectors, e->count * sizeof(uint16_t));
    for (unsigned i = 0; i < e->count; ++i) ++uses[e->sectors[i]];
    ++_readers[bank];
    f._handle.reset(h);
  } else {
    if (!_entries.find(path) && _entries.count >= 32) {
      _error = -28;
      return f;
    }
    uint32_t maximum = fragmentCapacity(strlen(path));
    if (length != SIZE_MAX && length > maximum) {
      _error = -28;
      return f;
    }
    f._handle.reset(ArdFSFile::Writer::create(path));
    if (!f._handle) {
      _error = -12;
      return f;
    }
    auto& h = static_cast<ArdFSFile::Writer&>(*f._handle);
    h.volume = this;
    h.epoch = _epoch;
    h.expected = length;
    h.capacity = length == SIZE_MAX ? maximum : length;
    if (length == SIZE_MAX) {
      uint32_t available = dataSectors(_maps[_active != 0]);
      if (!available) available = dataSectors(_maps[_active == 0]);
      for (const Entry* e = _entries.head; e; e = e->next)
        if (strcmp(e->path, path)) {
          uint32_t n = fragmentCount(e->pathLength, e->length);
          if (n > available) {
            f._handle.reset();
            _error = -28;
            return f;
          }
          available -= n;
        }
      uint32_t slots = std::min<uint32_t>(available, (ArdFSDetail::Sector - ArdFSDetail::Header - h.pathLength - 2) / 2);
      if (!slots) {
        f._handle.reset();
        _error = -28;
        return f;
      }
      h.capacity = slots * (ArdFSDetail::Sector - 4) + 4 - ArdFSDetail::Header - h.pathLength - slots * 2;
    }
    if (!h.allocateBuffer()) {
      f._handle.reset();
      _error = -12;
      return f;
    }
    _writer = true;
    if (!deferred) {
      int status;
      do { status = f.prepareStep(); } while (!status);
      if (status < 0) {
        f.close();
        return f;
      }
    }
  }
  _error = 0;
  return f;
}

inline void ArdFSFile::HandleDeleter::operator()(Handle* handle) const noexcept {
  if (!handle) return;
  if (handle->writing) delete static_cast<Writer*>(handle);
  else {
    auto& v = *handle->volume;
    if (handle->epoch == v._epoch) {
      unsigned bank = handle->address >= v._bankSize;
      auto* uses = v._uses[bank].get();
      for (unsigned i = 0; i < handle->count; ++i)
        if (uses && uses[handle->sectors()[i]] && uses[handle->sectors()[i]] != 255) --uses[handle->sectors()[i]];
      if (v._readers[bank]) --v._readers[bank];
    }
    delete handle;
  }
}
inline ArdFSFile::operator bool() const {
  return _handle && _handle->volume && _handle->epoch == _handle->volume->_epoch;
}
inline size_t ArdFSFile::size() const {
  return *this ? _handle->length : 0;
}
inline int ArdFSFile::read(uint8_t* p, size_t n) {
  if (!*this || _handle->writing || !_handle->volume->_mounted) return -1;
  auto& h = *_handle;
  n = std::min<size_t>(n, h.length - h.offset);
  if (!h.volume->readFragments(h.address, h.sectors(), h.count, h.offset, p, n)) return -1;
  h.offset += n;
  return n;
}
inline size_t ArdFSFile::queueWrite(const uint8_t* p, size_t n) {
  if (!*this || !_handle->writing || !n || n > 256) return 0;
  auto& h = static_cast<Writer&>(*_handle);
  if (h.pendingSize || h.mutationSource || h.flushStage || h.failed || h.committed || h.prepareStage != 10 || h.finishStage || n > h.capacity - h.length) return 0;
  ArdFSVolume::bufferInput(h, p, n);
  return n;
}
inline size_t ArdFSFile::write(const uint8_t* p, size_t n) {
  if (!*this || !_handle->writing) return 0;
  auto& h = static_cast<Writer&>(*_handle);
  if (h.mutationSource || h.flushStage || h.failed || h.committed || h.prepareStage != 10 || h.finishStage != 0 || n > h.capacity - h.length) {
    h.failed = true;
    return 0;
  }
  size_t accepted = 0;
  while (accepted < n) {
    int status;
    do { status = writeStep(); } while (status == 0);
    if (status < 0) return 0;
    size_t count = std::min<size_t>(sizeof(h.bytes), n - accepted);
    ArdFSVolume::bufferInput(h, p + accepted, count);
    accepted += count;
  }
  return n;
}
inline int ArdFSFile::writeStep() {
  if (!*this) return -1;
  if (!_handle->writing) return 1;
  auto& h = static_cast<Writer&>(*_handle);
  if (h.failed) return -1;
  if (h.mutationSource || h.committed || !h.entry || h.flushStage) return 1;
  if (h.prepareStage != 10) return -1;
  h.volume->_error = 0;
  int status = h.volume->writeBufferedStep(h);
  if (status < 0) {
    h.failed = true;
    if (!h.volume->_error) h.volume->_error = -5;
  }
  return status;
}

inline int ArdFSFile::prepareStep() {
  if (!*this) return -1;
  if (!_handle->writing) return 1;
  auto& h = static_cast<Writer&>(*_handle);
  if (h.failed) return -1;
  h.volume->_error = 0;
  int status = h.volume->prepareStep(h);
  if (status < 0) {
    h.failed = true;
    if (!h.volume->_error) h.volume->_error = -5;
  }
  return status;
}
inline int ArdFSFile::flushStep() {
  if (!*this) return -1;
  if (!_handle->writing) return 1;
  auto& h = static_cast<Writer&>(*_handle);
  if (h.committed) return 1;
  if (h.failed || h.prepareStage != 10 || (h.expected != SIZE_MAX && h.length != h.expected)) return -1;
  auto& v = *h.volume;
  v._error = 0;
  if (!h.flushStage) h.flushStage = 1;
  int status = 1;
  switch (h.flushStage) {
    case 1:
      if (h.entry) status = v.writeBufferedStep(h, true);
      break;
    case 2:
      if (h.entry) status = v.finishFragment(h);
      break;
    case 3:
      v.buildCatalog(h);
      break;
    case 4:
      status = v.publishMapStep(h.map, h.sectorBuffer->data);
      break;
    default:
      h.committed = true;
      h.sectorBuffer = nullptr;
      h.ownedBuffer.reset();
      return 1;
  }
  if (status < 0) {
    h.failed = true;
    if (!v._error) v._error = -5;
    return -1;
  }
  if (status == 1) ++h.flushStage;
  return 0;
}
inline bool ArdFSFile::flush() {
  int status;
  do { status = flushStep(); } while (status == 0);
  return status == 1;
}
inline bool ArdFSFile::close() {
  bool ok = true;
  if (*this && _handle->writing) {
    auto& h = static_cast<Writer&>(*_handle);
    auto& v = *h.volume;
    // A verified record becomes visible at the first catalog commit. Closing in
    // the narrow commit/read-back window must not recycle its sectors.
    if (h.flushStage == 4 && (h.map.stage == 6 || h.map.stage == 7)) {
      alignas(4) uint8_t commit[4];
      bool read = h.map.stage == 7 || v.readPhysical(h.bank + h.map.slot * 128U + 24, commit, 4);
      if (read && (h.map.stage == 7 || ArdFSDetail::getWord(commit) == ArdFSDetail::Commit)) v.completeMap(h.map);
      else if (!read) h.map.uncertain = true;
    }
    if (h.map.uncertain) {
      v.end();
      v._error = -5;
      ok = false;
    } else v.cancelMap(h.map);
    v._staged.clear();
    v._transaction = nullptr;
    v._writer = false;
  }
  _handle.reset();
  return ok;
}

inline size_t ArdFSFile::position() const {
  return *this ? (_handle->writing ? _handle->length : _handle->offset) : 0;
}
inline bool ArdFSFile::seek(size_t position) {
  if (!*this || _handle->writing || position > _handle->length) return false;
  _handle->offset = position;
  return true;
}
inline size_t ArdFSVolume::usedBytes() const {
  size_t used = 0;
  if (_mounted) {
    const auto* refs = _uses[_active != 0].get();
    for (uint32_t i = 0; i < _bankSize / ArdFSDetail::Sector; ++i)
      if (refs[i] && refs[i] != 255) used += ArdFSDetail::Sector;
    return used;
  }
  return used;
}
inline bool ArdFSVolume::fileInfo(size_t index, String& path, size_t& size) const {
  const Entry* e = _mounted ? _entries.at(index) : nullptr;
  if (!e) return false;
  path = e->path;
  if (path != e->path) return false;
  size = e->length;
  return true;
}
inline ArdFSFile ArdFSVolume::mutate(const char* source, const char* destination) {
  ArdFSFile f;
  if (!_mounted || _writer || !source || !*source || strlen(source) > 72 || (destination && (!*destination || strlen(destination) > 72))) {
    _error = -22;
    return f;
  }
  if (!_entries.find(source)) {
    _error = -2;
    return f;
  }
  if (destination && _entries.find(destination)) {
    _error = -17;
    return f;
  }
  f._handle.reset(ArdFSFile::Writer::create(destination ? destination : ""));
  if (!f._handle) {
    _error = -12;
    return f;
  }
  auto& h = static_cast<ArdFSFile::Writer&>(*f._handle);
  h.mutationSource = _entries.find(source)->path;
  if (!h.allocateBuffer()) {
    f._handle.reset();
    _error = -12;
    return f;
  }
  h.volume = this;
  h.epoch = _epoch;
  h.expected = 0;
  _writer = true;
  _error = 0;
  return f;
}

inline ArdFSFile::Writer* ArdFSFile::Writer::create(const char* path) {
  size_t length = strlen(path);
  void* memory = ArdAllocation::allocate(sizeof(Writer) + length + 1);
  if (!memory) return nullptr;
  auto* writer = ::new (memory) Writer;
  char* name = static_cast<char*>(memory) + sizeof(Writer);
  memcpy(name, path, length + 1);
  writer->path = name;
  writer->pathLength = length;
  return writer;
}

inline const char* ArdFSFile::Writer::copyPath(const char* source) const {
  if (mutationSource) {
    if (source != mutationSource) return source;
    return pathLength ? path : nullptr;
  }
  return !strcmp(source, path) ? nullptr : source;
}

#include "ArdFSFragmentsImpl.h"
