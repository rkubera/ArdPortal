#pragma once
// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#if defined(ESP8266)
#include <flash_hal.h>
#endif
#include <algorithm>
#include "ArdCRC.h"
namespace ArdFSDetail {
constexpr uint32_t BankMagic = 0x31445241, RecordMagic = 0x31524341, Commit = 0x51ac0ffe;
constexpr uint32_t Sector = 4096, Header = 32;
/**
 * @brief Round a journal extent up to a flash sector boundary.
 * @param n Number of bytes or elements to process.
 * @return The smallest whole-sector extent containing the requested byte count.
 */
inline uint32_t aligned(uint32_t n) {
  return (n + Sector - 1) / Sector * Sector;
}
inline bool erased(const uint8_t* bytes, size_t n) {
  while (n--) if (*bytes++ != 255) return false;
  return true;
}
// Fields are explicitly little endian, independent of compiler struct packing.
/**
 * @brief Encode a 32-bit word into little-endian journal bytes.
 * @param p Destination for four bytes.
 * @param n Word to encode.
 * @return No value.
 */
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
}
inline void ArdFSVolume::Index::clear() {
  while (head) { Entry* next = head->next; delete head; head = next; }
  count = 0;
}
inline ArdFSVolume::Entry* ArdFSVolume::Index::find(const char* path) const {
  for (Entry* e = head; e; e = e->next) if (!strcmp(e->path, path)) return e;
  return nullptr;
}
inline ArdFSVolume::Entry* ArdFSVolume::Index::at(size_t index) const {
  Entry* e = head;
  while (e && index--) e = e->next;
  return e;
}
inline ArdFSVolume::Entry* ArdFSVolume::Entry::create(size_t nameLength) {
  void* memory = ArdAllocation::allocate(sizeof(Entry) + nameLength + 1);
  if (!memory) return nullptr;
  auto* entry = ::new (memory) Entry;
  // The name shares this allocation and remains stable until the entry is freed.
  entry->path = static_cast<char*>(memory) + sizeof(Entry);
  entry->pathLength = nameLength;
  entry->path[nameLength] = 0;
  return entry;
}
inline bool ArdFSVolume::Index::insert(Entry* entry) {
  Entry* old = find(entry->path);
  if (old) {
    old->address = entry->address; old->length = entry->length;
    delete entry;
  } else {
    if (count == 32) { delete entry; return false; }
    entry->next = head; head = entry; ++count;
  }
  return true;
}
inline bool ArdFSVolume::Index::set(const char* path, uint32_t address, uint32_t length) {
  Entry* entry = Entry::create(strlen(path));
  if (!entry) return false;
  memcpy(entry->path, path, entry->pathLength);
  entry->address = address; entry->length = length;
  return insert(entry);
}
inline void ArdFSVolume::Index::merge(Index& other) {
  while (other.head) {
    Entry* entry = other.head; other.head = entry->next;
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
    c ^= p[i];
    for (int j = 0; j < 8; ++j) c = (c >> 1) ^ (0xedb88320U & (0U - (c & 1)));
  }
  return c;
}
// Shared layout and atomic publication for record and bank headers.
inline uint32_t ArdFSVolume::makeHeader(uint8_t* bytes, uint32_t magic, uint32_t a, uint32_t b, uint32_t c) {
  memset(bytes, 255, ArdFSDetail::Header);
  ArdFSDetail::put(bytes, magic); ArdFSDetail::put(bytes + 4, a); ArdFSDetail::put(bytes + 8, b); ArdFSDetail::put(bytes + 12, c);
  return crc(bytes, 16);
}
inline int ArdFSVolume::publishStep(uint32_t address, const uint8_t* header, uint8_t* scratch, uint8_t& stage, size_t length) {
  if (address == UINT32_MAX) return -1;
  switch (stage) {
    case 3:
      if (!writePhysical(address, header, length)) return -1;
      break;
    case 4:
      if (!readPhysical(address, scratch, length) || memcmp(header, scratch, length)) return -1;
      break;
    case 5:
      ArdFSDetail::put(scratch, ArdFSDetail::Commit);
      if (!writePhysical(address + 24, scratch, 4)) return -1;
      break;
    case 6:
      if (!readPhysical(address + 24, scratch, 4) || ArdFSDetail::get(scratch) != ArdFSDetail::Commit) return -1;
      break;
    default: return 1;
  }
  ++stage;
  return 0;
}
/**
 * @brief Locate and validate the flash region used by the two-bank journal.
 * @return True on success; false if validation, resource allocation or the operation fails.
 */
inline bool ArdFSVolume::configure() {
  if (_configured) return true;
#if defined(ESP8266)
  _address = FS_PHYS_ADDR;
  _size = FS_PHYS_SIZE;
  if (ESP.getFlashChipSize() > ESP.getFlashChipRealSize() || _address < ESP.getSketchSize() || _address > ESP.getFlashChipRealSize() || _size > ESP.getFlashChipRealSize() - _address) return false;
#elif defined(ESP32)
  _partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "ardfs");
  if (!_partition) _partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "littlefs");
  if (!_partition) _partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "spiffs");
  if (!_partition || _partition->encrypted) {
    return false;
  }
  _size = _partition->size;
#endif
  _bankSize = (_size / (2 * ArdFSDetail::Sector)) * ArdFSDetail::Sector;
  _configured = _bankSize >= 8 * ArdFSDetail::Sector && _bankSize / ArdFSDetail::Sector <= UINT16_MAX && _address % ArdFSDetail::Sector == 0;
  return _configured;
}
inline bool ArdFSVolume::readPhysical(uint32_t address, void* bytes, size_t n) {
  if (address > _size || n > _size - address) return false;
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
  if (n > 256 || address > _size || n > _size - address) return false;
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
  if (address % ArdFSDetail::Sector || address > _size || ArdFSDetail::Sector > _size - address) return false;
#if defined(ESP8266)
  bool ok = ESP.flashEraseSector((_address + address) / ArdFSDetail::Sector);
#else
  bool ok = esp_partition_erase_range(_partition, address, ArdFSDetail::Sector) == ESP_OK;
#endif
  yield();
  return ok;
}
// Each bank owns its map. Logical addresses remain stable when an unused tail
// sector is retired; existing committed records are never shifted in place.
inline void ArdFSVolume::clearMaps() {
  memset(_maps, 0, sizeof(_maps));
  _maps[0].slot = _maps[1].slot = UINT16_MAX;
}
__attribute__((noinline)) inline uint32_t ArdFSVolume::bankLimit(const BankMap& map) const {
  return _bankSize - (map.modern ? (1U + map.count) * ArdFSDetail::Sector : 0U);
}
inline uint32_t ArdFSVolume::physical(uint32_t address, size_t length) const {
  if (address >= _bankSize * 2) return UINT32_MAX;
  bool second = address >= _bankSize;
  uint32_t bank = second ? _bankSize : 0, offset = address - bank;
  const auto& map = _maps[second];
  uint32_t limit = bankLimit(map);
  if (offset >= limit || length > limit - offset) return UINT32_MAX;
  if (map.modern && offset >= ArdFSDetail::Sector) {
    uint32_t sector = offset / ArdFSDetail::Sector + 1;
    for (unsigned i = 0; i < map.count; ++i) if (map.bad[i] <= sector) ++sector;
    offset = sector * ArdFSDetail::Sector + offset % ArdFSDetail::Sector;
  }
  return bank + offset;
}
inline bool ArdFSVolume::retire(uint32_t address) {
  if (!_working || address < _workBank + ArdFSDetail::Sector ||
      address >= _workBank + bankLimit(*_workMap) || _workMap->count == 32) return false;
  uint32_t sector = (physical(address, 1) - _workBank) / ArdFSDetail::Sector;
  unsigned at = 0;
  while (at < _workMap->count && _workMap->bad[at] < sector) ++at;
  memmove(_workMap->bad + at + 1, _workMap->bad + at, (_workMap->count - at) * sizeof(_workMap->bad[0]));
  _workMap->bad[at] = sector; ++_workMap->count;
  return true;
}
inline bool ArdFSVolume::fitRecord(ArdFSFile::Writer& h) {
  uint32_t limit = bankLimit(*_workMap), overhead = ArdFSDetail::Header + h.pathLength;
  if (h.record >= limit || overhead > limit - h.record) return false;
  uint32_t capacity = limit - h.record - overhead;
  if (h.expected == SIZE_MAX) {
    if (h.length > capacity) return false;
    h.capacity = std::min(h.capacity, capacity);
    h.extent = ArdFSDetail::aligned(overhead + h.capacity);
  }
  return h.extent <= limit - h.record;
}
inline bool ArdFSVolume::readRaw(uint32_t address, void* bytes, size_t n) {
  auto* out = static_cast<uint8_t*>(bytes);
  while (n) {
    size_t maximum = 256;
#if defined(ESP8266)
    maximum -= address & 3U;
#endif
    size_t count = std::min<size_t>(n, std::min<size_t>(maximum, ArdFSDetail::Sector - address % ArdFSDetail::Sector));
    uint32_t target = physical(address, count);
    if (target == UINT32_MAX || !readPhysical(target, out, count)) return false;
    out += count; address += count; n -= count;
  }
  return true;
}
inline bool ArdFSVolume::eraseRaw(uint32_t address) {
  if (address % ArdFSDetail::Sector) return false;
  uint32_t target = physical(address, ArdFSDetail::Sector);
  return target != UINT32_MAX && erasePhysical(target);
}
inline bool ArdFSVolume::decodeMap(uint8_t* h, BankMap& map, unsigned slot) {
  if (ArdFSDetail::get(h) != ArdFSDetail::BankMagic || ArdFSDetail::get(h + 12) != _bankSize ||
      ArdFSDetail::get(h + 24) != ArdFSDetail::Commit) return false;
  uint32_t version = ArdFSDetail::get(h + 4), checksum = ArdFSDetail::get(h + 16);
  if (version == 1) {
    if (slot || checksum != ~crc(h, 16)) return false;
    map = BankMap();
  } else if (version == 2) {
    ArdFSDetail::put(h + 16, UINT32_MAX); ArdFSDetail::put(h + 24, UINT32_MAX);
    if (checksum != ~crc(h, 128) || h[32] > 32 || h[33] > 3 || (h[33] & (1U << (slot / 32)))) return false;
    map.modern = true; map.count = h[32]; map.metadataBad = h[33];
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    memcpy(map.bad, h + 36, map.count * sizeof(map.bad[0]));
#endif
    for (unsigned i = 0; i < map.count; ++i) {
#if !defined(__BYTE_ORDER__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
      map.bad[i] = uint16_t(h[36 + i * 2]) | uint16_t(h[37 + i * 2]) << 8;
#endif
      if (map.bad[i] < 2 || map.bad[i] >= _bankSize / ArdFSDetail::Sector ||
          (i && map.bad[i] <= map.bad[i - 1])) return false;
    }
    if (map.count + 3U > _bankSize / ArdFSDetail::Sector) return false;
    map.revision = ArdFSDetail::get(h + 20);
  } else return false;
  map.valid = true; map.slot = slot; map.generation = ArdFSDetail::get(h + 8);
  return true;
}
inline void ArdFSVolume::encodeMap(uint8_t* h) const {
  memset(h, 255, 128);
  ArdFSDetail::put(h, ArdFSDetail::BankMagic); ArdFSDetail::put(h + 4, 2);
  ArdFSDetail::put(h + 8, _workMap->generation); ArdFSDetail::put(h + 12, _bankSize);
  ArdFSDetail::put(h + 20, _workMap->revision + 1);
  h[32] = _workMap->count; h[33] = _workMap->metadataBad;
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  memcpy(h + 36, _workMap->bad, _workMap->count * sizeof(_workMap->bad[0]));
#else
  for (unsigned i = 0; i < _workMap->count; ++i) {
    h[36 + i * 2] = uint8_t(_workMap->bad[i]); h[37 + i * 2] = uint8_t(_workMap->bad[i] >> 8);
  }
#endif
  ArdFSDetail::put(h + 16, ~crc(h, 128));
}
inline bool ArdFSVolume::nextMapSector(ArdFSFile::MapCommit& h) {
  unsigned failed = h.slot / 32, other = failed ^ 1U;
  _workMap->metadataBad |= 1U << failed;
  if (_workMap->metadataBad & (1U << other)) return false;
  // Keep the surviving committed copy; its remaining slots are an emergency log.
  if (_workMap->slot != UINT16_MAX && _workMap->slot / 32 == other) {
    if (_workMap->slot % 32 == 31) return false;
    h.slot = _workMap->slot + 1; h.stage = 2;
  } else { h.slot = other * 32; h.stage = 1; }
  return true;
}
inline int ArdFSVolume::publishMapStep(ArdFSFile::MapCommit& h, uint8_t* buffer) {
  uint8_t* image = buffer;
  uint8_t* scratch = buffer + 128;
  if (!h.stage) {
    if (_workMap->slot == UINT16_MAX) {
      h.slot = (_workMap->metadataBad & 1) ? 32 : 0; h.stage = 2;
    } else if (_workMap->metadataBad) {
      if (_workMap->slot % 32 == 31) return -1;
      h.slot = _workMap->slot + 1; h.stage = 2;
    } else { h.slot = (_workMap->slot / 32 ^ 1U) * 32; h.stage = 1; }
    return 0;
  }
  uint32_t address = _workBank + h.slot * 128U;
  if (h.stage == 1) {
    if (!erasePhysical(address)) return nextMapSector(h) ? 0 : -1;
    h.stage = 2; return 0;
  }
  if (h.stage == 2) {
    if (_workMap->metadataBad) {
      // Skip a torn emergency-log entry without erasing the only valid copy.
      if (!readPhysical(address, scratch, 128)) return nextMapSector(h) ? 0 : -1;
      if (!ArdFSDetail::erased(scratch, 128)) {
        if (h.slot % 32 == 31) return -1;
        ++h.slot; return 0;
      }
    }
    encodeMap(image); h.stage = 3; return 0;
  }
  int status = publishStep(address, image, scratch, h.stage, 128);
  if (status < 0) return nextMapSector(h) ? 0 : -1;
  if (!status) return 0;
  _workMap->slot = h.slot; ++_workMap->revision; _workMap->valid = true;
  if (!h.mirror && !_workMap->metadataBad) {
    h.mirror = true;
    unsigned other = h.slot / 32 ^ 1U;
    h.slot = other * 32; h.stage = _workMap->revision == 1 ? 2 : 1;
    return 0;
  }
  return 1;
}

// Synchronous compatibility wrappers drain the same bounded state machines.
inline bool ArdFSVolume::begin() {
  int status;
  do { status = beginStep(); } while (status == 0);
  return status == 1;
}
inline int ArdFSVolume::beginStep() {
  if (_mounted) return 1;
  uint8_t bytes[256];
  switch (_mountStage) {
    case 0:
      if (!configure()) { _error = -22; return -1; }
      _working = false; clearMaps();
      _headerErrors[0] = _headerErrors[1] = 0; _headerScan = 0; _headerObserved = false;
      _entries.clear(); _scanEntry.reset(); _mountStage = 1; return 0;
    case 1:
      {
        if (_headerScan == 128) { _mountStage = 3; return 0; }
        unsigned bank = _headerScan / 64, slot = _headerScan++ % 64;
        if (!readPhysical(bank * _bankSize + slot * 128U, bytes, 128)) {
          _headerErrors[bank] |= 1U << (slot / 32); return 0;
        }
        auto& old = _maps[bank];
        if (!ArdFSDetail::erased(bytes, 128)) _headerObserved = true;
        BankMap decoded = {};
        if (decodeMap(bytes, decoded, slot)) {
          if (!old.valid || int32_t(decoded.generation - old.generation) > 0 ||
              (decoded.generation == old.generation && int32_t(decoded.revision - old.revision) > 0)) old = decoded;
        }
        return 0;
      }
    case 3:
      {
        for (unsigned i = 0; i < 2; ++i) {
          uint8_t errors = _headerErrors[i];
          if (_maps[i].valid && !_maps[i].modern) errors &= 1;
          if (_maps[i].valid && _maps[i].modern) errors &= 1U << (_maps[i].slot / 32);
          if (errors & ~_maps[i].metadataBad) { _error = -5; return -1; }
        }
        int first = _maps[1].valid && (!_maps[0].valid || int32_t(_maps[1].generation - _maps[0].generation) > 0) ? 1 : 0;
        if (!_maps[first].valid) {
          if (_headerObserved) { _error = -5; return -1; }
          _scanAt = 0; _mountStage = 8; return 0;
        }
        _active = first * _bankSize; _generation = _maps[first].generation;
        _next = ArdFSDetail::Sector; _mountStage = 4; return 0;
      }
    case 8:
      // Only a completely erased partition is eligible for automatic first-use format.
      if (_scanAt == _bankSize * 2) { _error = -84; return -1; }
      if (!readPhysical(_scanAt, bytes, sizeof(bytes))) { _error = -5; return -1; }
      if (!ArdFSDetail::erased(bytes, sizeof(bytes))) { _error = -5; return -1; }
      _scanAt += sizeof(bytes); return 0;
    case 4:
      if (_next + ArdFSDetail::Header > bankLimit(_maps[_active != 0])) { _mountStage = 7; return 0; }
      if (!readRaw(_active + _next, bytes, ArdFSDetail::Header)) { _error = -5; return -1; }
      if (ArdFSDetail::get(bytes) == UINT32_MAX) { _mountStage = 7; return 0; }
      _scanExtent = ArdFSDetail::get(bytes + 4); _scanName = ArdFSDetail::get(bytes + 8); _scanLength = ArdFSDetail::get(bytes + 12);
      if (!_scanName || _scanName > 72 || _scanLength > bankLimit(_maps[_active != 0]) - ArdFSDetail::Header - _scanName ||
          ArdFSDetail::get(bytes) != ArdFSDetail::RecordMagic || _scanExtent != ArdFSDetail::aligned(ArdFSDetail::Header + _scanName + _scanLength) || _scanExtent > bankLimit(_maps[_active != 0]) - _next) {
        _next = bankLimit(_maps[_active != 0]); _mountStage = 7; return 0;
      }
      if (ArdFSDetail::get(bytes + 24) != ArdFSDetail::Commit) { _next += _scanExtent; return 0; }
      _scanCrc = crc(bytes, 16); _scanExpected = ArdFSDetail::get(bytes + 16);
      _scanEntry.reset(); _scanAt = 0; _mountStage = 5; return 0;
    case 5:
      _scanEntry.reset(Entry::create(_scanName));
      if (!_scanEntry) { _error = -12; return -1; }
      _mountStage = 6; return 0;
    case 6:
      {
        size_t count = std::min<size_t>(sizeof(bytes), _scanName + _scanLength - _scanAt);
        if (count) {
          if (!readRaw(_active + _next + ArdFSDetail::Header + _scanAt, bytes, count)) { _error = -5; return -1; }
          _scanCrc = crc(bytes, count, _scanCrc);
          if (!_scanAt) memcpy(_scanEntry->path, bytes, _scanName);
          _scanAt += count; return 0;
        }
        if (~_scanCrc == _scanExpected) {
          _scanEntry->address = _active + _next + ArdFSDetail::Header + _scanName;
          _scanEntry->length = _scanLength;
          if (!_entries.insert(_scanEntry.release())) {
            _error = _entries.count == 32 ? -28 : -12; return -1;
          }
        }
        _next += _scanExtent; _mountStage = 4; return 0;
      }
    default:
      _scanEntry.reset(); _mounted = true; _error = 0; return 1;
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
      if (!configure()) { _error = -22; return -1; }
      _formatBuffer.reset(new (std::nothrow) ArdFSFile::MapBuffer);
      if (!_formatBuffer) { _error = -12; return -1; }
      _formatMap = {};
      _working = false; clearMaps();
      _maps[0].modern = _maps[1].modern = true;
      _mounted = false; _mountStage = 0; _formatAt = 0; _formatStage = 1; return 0;
    case 1:
      if (_formatAt == _bankSize * 2) { _formatStage = 2; return 0; }
      if (erasePhysical(_formatAt)) {
        _scanAt = 0; _formatStage = 4; return 0;
      }
      _scanAt = UINT32_MAX; _formatStage = 4; return 0;
    case 4:
      {
        auto& bytes = _formatBuffer->data;
        bool ok = _scanAt != UINT32_MAX && readPhysical(_formatAt + _scanAt, bytes, sizeof(bytes));
        if (ok) ok = ArdFSDetail::erased(bytes, sizeof(bytes));
        if (ok) {
          _scanAt += sizeof(bytes);
          if (_scanAt < ArdFSDetail::Sector) return 0;
        } else {
          auto& map = _maps[_formatAt >= _bankSize];
          unsigned sector = (_formatAt - (_formatAt >= _bankSize ? _bankSize : 0)) / ArdFSDetail::Sector;
          if (sector < 2) map.metadataBad |= 1U << sector;
          else if (map.count < 32) map.bad[map.count++] = sector;
          else { _formatBuffer.reset(); _formatStage = 0; _error = -5; return -1; }
        }
        _formatAt += ArdFSDetail::Sector; _formatStage = 1; return 0;
      }
    case 2:
      _workBank = _maps[0].metadataBad == 3 ? _bankSize : 0;
      _workMap = &_maps[_workBank != 0]; _workMap->generation = 1;
      if (_workMap->metadataBad == 3 || bankLimit(*_workMap) < ArdFSDetail::Sector * 3) {
        _formatBuffer.reset(); _formatStage = 0; _error = -5; return -1;
      }
      _formatStage = 3; return 0;
    default:
      {
        int status = publishMapStep(_formatMap, _formatBuffer->data);
        if (status < 0) { _formatBuffer.reset(); _formatStage = 0; _error = -5; return -1; }
        if (!status) return 0;
        _formatBuffer.reset(); _formatStage = 0; _error = 0; return 1;
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
// Clear the successor marker after the final data-sector mapping is known.
inline int ArdFSVolume::terminateStep(ArdFSFile::Writer& h) {
  uint32_t tail = h.bank + h.record + (h.mutationSource ? 0 : ArdFSDetail::aligned(ArdFSDetail::Header + h.pathLength + h.length));
  if (tail - h.bank >= bankLimit(*_workMap)) return 1;
  if (h.reserveStage) {
    h.reserveStage = 0;
    if (eraseRaw(tail)) { ++h.sectorRetries; return 0; }
  } else {
    if (readRaw(tail, h.bytes, ArdFSDetail::Header)) {
      if (ArdFSDetail::erased(h.bytes, ArdFSDetail::Header)) { h.sectorRetries = 0; return 1; }
      if (h.sectorRetries < 2) { h.reserveStage = 1; return 0; }
    }
  }
  h.sectorRetries = 0;
  return retire(tail) ? 0 : -1;
}

inline int ArdFSVolume::prepareStep(ArdFSFile::Writer& h) {
  switch (h.prepareStage) {
    case 0:
      {
        uint32_t extent = ArdFSDetail::aligned(ArdFSDetail::Header + h.pathLength + h.capacity);
        if (extent > _bankSize - ArdFSDetail::Sector * 2) return -1;
        h.bank = _active;
        h.record = _next;
        _staged.clear();
        if (h.mutationSource || !_maps[_active != 0].modern || _next > bankLimit(_maps[_active != 0]) || extent > bankLimit(_maps[_active != 0]) - _next) {
          h.compacting = true;
          h.bank = _active ? 0 : _bankSize;
          h.record = ArdFSDetail::Sector;
          h.prepareStage = 1; h.workAt = 0;
        } else h.prepareStage = 8;
        h.reusingBank = h.compacting && int32_t(_maps[h.bank != 0].generation - _generation) > 0;
        _workBank = h.bank; _workMap = &_maps[h.bank != 0]; h.initialBadCount = _workMap->count; _workMap->modern = true;
        if (h.compacting) { _workMap->slot = UINT16_MAX; _workMap->revision = 0; }
        _workMap->generation = _generation + (h.compacting ? 1 : 0); _working = true;
        uint32_t required = h.mutationSource ? 0 : extent;
        if (h.compacting) for (const Entry* e = _entries.head; e; e = e->next) {
          const char* target = h.copyPath(e->path);
          if (target) required += ArdFSDetail::aligned(ArdFSDetail::Header + strlen(target) + e->length);
        }
        if (required > bankLimit(*_workMap) - ArdFSDetail::Sector) return -1;
        return 0;
      }
    case 1:
      if (h.workAt == 2) {
        if (_workMap->metadataBad == 3) return -1;
        h.prepareStage = 2; return 0;
      }
      if (h.reserveStage) {
        // A previous abandoned publication may outrank the live bank. Its map
        // must be gone before any record beneath that header is overwritten.
        if (!readPhysical(h.bank + h.workAt * ArdFSDetail::Sector + (h.reserveStage - 1) * sizeof(h.bytes), h.bytes, sizeof(h.bytes)) ||
            !ArdFSDetail::erased(h.bytes, sizeof(h.bytes))) return -1;
        if (++h.reserveStage <= ArdFSDetail::Sector / sizeof(h.bytes)) return 0;
        h.reserveStage = 0; ++h.workAt;
        return 0;
      }
      if (!(_workMap->metadataBad & (1U << h.workAt)) &&
          !erasePhysical(h.bank + h.workAt * ArdFSDetail::Sector))
        _workMap->metadataBad |= 1U << h.workAt;
      if (h.reusingBank) h.reserveStage = 1;
      else ++h.workAt;
      return 0;
    case 2:
      {
        // At most 32 paths; metadata work is independent of file sizes.
        while (h.copyIndex < _entries.count && !h.copyPath(_entries.at(h.copyIndex)->path)) ++h.copyIndex;
        if (h.copyIndex == _entries.count) { h.prepareStage = 8; return 0; }
        const Entry& e = *_entries.at(h.copyIndex);
        h.copy.reset(ArdFSFile::Writer::create(h.copyPath(e.path)));
        if (!h.copy) return -1;
        auto& copy = *h.copy;
        copy.volume = this;
        copy.sectorBuffer = h.sectorBuffer;
        copy.bank = h.bank;
        copy.record = h.record;
        copy.capacity = e.length; copy.expected = e.length;
        copy.address = copy.bank + copy.record + ArdFSDetail::Header + copy.pathLength;
        copy.extent = ArdFSDetail::aligned(ArdFSDetail::Header + copy.pathLength + copy.capacity);
        if (!fitRecord(copy)) return -1;
        h.prepareStage = 4;
        return 0;
      }
    case 4:
      startSectorBuffer(*h.copy);
      h.prepareStage = 5;
      return 0;
    case 5:
      {
        int status = writeBufferedStep(*h.copy);
        if (status != 1) return status < 0 ? -1 : 0;
      }
      if (h.copy->length == h.copy->capacity) { h.prepareStage = 7; return 0; }
      {
        size_t n = std::min<size_t>(sizeof(h.bytes), h.copy->capacity - h.copy->length);
        if (!readRaw(_entries.at(h.copyIndex)->address + h.copy->length, h.bytes, n)) return -1;
        bufferInput(*h.copy, h.bytes, n);
        return 0;
      }
    case 7:
      {
        int status = finishRecordStep(*h.copy);
        if (status < 0) return -1;
        if (status == 1) {
          if (!_staged.set(h.copy->path, h.copy->address, h.copy->length)) return -1;
          h.record += h.copy->extent;
          h.copy.reset();
          ++h.copyIndex;
          h.prepareStage = 2;
        }
        return 0;
      }
    case 8:
      if (h.mutationSource) { h.prepareStage = 10; return 1; }
      h.extent = ArdFSDetail::aligned(ArdFSDetail::Header + h.pathLength + h.capacity);
      if (!fitRecord(h)) return -1;
      h.address = h.bank + h.record + ArdFSDetail::Header + h.pathLength;
      h.prepareStage = 9; return 0;
    case 9:
      if (!_staged.set(h.path, h.address, 0)) return -1;
      startSectorBuffer(h);
      h.prepareStage = 10;
      return 0;
    default: return 1;
  }
}

inline void ArdFSVolume::bufferInput(ArdFSFile::Writer& h, const uint8_t* bytes, size_t n) {
  memcpy(h.bytes, bytes, n);
  h.pendingAt = 0; h.pendingSize = n;
  h.checksum = crc(bytes, n, h.checksum);
  h.length += n;
}
// Keep the physical sector image until its program/read-back cycle succeeds.
// The 256-byte writer scratch holds one accepted input slice across boundaries.
inline void ArdFSVolume::startSectorBuffer(ArdFSFile::Writer& h) {
  h.sectorRetries = 0; h.sectorStage = 2;
  h.sectorAddress = h.bank + h.record;
  h.sectorUsed = ArdFSDetail::Header + h.pathLength;
  memset(h.sectorBuffer->data, 0xff, ArdFSDetail::Sector);
  memcpy(h.sectorBuffer->data + ArdFSDetail::Header, h.path, h.pathLength);
}
inline int ArdFSVolume::drainSectorStep(ArdFSFile::Writer& h) {
  if (h.sectorStage == 2) {
    h.sectorPhysical = physical(h.sectorAddress, ArdFSDetail::Sector);
    if (h.sectorPhysical != UINT32_MAX && erasePhysical(h.sectorPhysical)) {
      h.sectorAt = 0; h.sectorStage = 0;
      return 0;
    }
  } else {
    if (h.sectorAt == h.sectorUsed) {
      if (!h.sectorStage) { h.sectorAt = 0; h.sectorStage = 1; return 0; }
      if (h.sectorFinal) { h.sectorDone = true; return 1; }
      h.sectorAddress += ArdFSDetail::Sector;
      h.sectorUsed = h.sectorAt = 0; h.sectorStage = 2; h.sectorRetries = 0;
      memset(h.sectorBuffer->data, 0xff, ArdFSDetail::Sector);
      return 1;
    }
    size_t n = std::min<size_t>(256, h.sectorUsed - h.sectorAt);
    bool ok;
    if (!h.sectorStage) {
      ok = writePhysical(h.sectorPhysical + h.sectorAt, h.sectorBuffer->data + h.sectorAt, n);
    } else {
      uint8_t verify[256];
      ok = readPhysical(h.sectorPhysical + h.sectorAt, verify, n) &&
        !memcmp(verify, h.sectorBuffer->data + h.sectorAt, n);
    }
    if (ok) { h.sectorAt += n; return 0; }
  }
  // Erase/program/read-back failures share the same bounded retry policy.
  if (++h.sectorRetries >= 2) {
    if (!retire(h.sectorAddress) || !fitRecord(h)) return -1;
    h.sectorRetries = 0;
  }
  h.sectorStage = 2;
  return 0;
}
inline int ArdFSVolume::writeBufferedStep(ArdFSFile::Writer& h, bool final) {
  if (h.sectorDone) return 1;
  if (h.sectorUsed == ArdFSDetail::Sector || h.sectorFinal) {
    int status = drainSectorStep(h);
    if (status != 1 || h.sectorDone) return status;
    return 0;
  }
  if (h.pendingAt < h.pendingSize) {
    size_t n = std::min<size_t>(h.pendingSize - h.pendingAt, ArdFSDetail::Sector - h.sectorUsed);
    memcpy(h.sectorBuffer->data + h.sectorUsed, h.bytes + h.pendingAt, n);
    h.pendingAt += n; h.sectorUsed += n;
    return 0;
  }
  h.pendingAt = h.pendingSize = 0;
  if (final) {
    if (!h.sectorUsed) { h.sectorDone = true; return 1; }
    h.sectorFinal = true; return 0;
  }
  return 1;
}

inline int ArdFSVolume::finishRecordStep(ArdFSFile::Writer& h) {
  if (!h.sectorDone) {
    int status = writeBufferedStep(h, true);
    return status < 0 ? -1 : 0;
  }
  uint8_t* header = h.sectorBuffer->data;
  switch (h.finishStage) {
    case 0:
      h.verified = makeHeader(header, ArdFSDetail::RecordMagic, ArdFSDetail::aligned(ArdFSDetail::Header + h.pathLength + h.length), h.pathLength, h.length);
      h.checksum = ArdCRC::combine(~crc(h.path, h.pathLength, h.verified), ~h.checksum, h.length);
      ArdFSDetail::put(header + 16, h.checksum);
      h.workAt = 0;
      h.finishStage = 1;
      return 0;
    case 1:
      {
        uint32_t total = h.pathLength + h.length;
        if (h.workAt == total) {
          if (~h.verified != h.checksum) return -1;
          h.finishStage = 3;
          return 0;
        }
        size_t n = std::min<size_t>(sizeof(h.bytes), total - h.workAt);
        if (!readRaw(h.bank + h.record + ArdFSDetail::Header + h.workAt, h.bytes, n)) return -1;
        h.verified = crc(h.bytes, n, h.verified);
        h.workAt += n;
        return 0;
      }
    default: return publishStep(physical(h.bank + h.record, ArdFSDetail::Header), header, h.bytes, h.finishStage, ArdFSDetail::Header);
  }
}

inline ArdFSFile ArdFSVolume::open(const char* path, const char* mode, size_t length, bool deferred) {
  ArdFSFile f;
  if (!_mounted || !path || !mode || (*mode != 'r' && *mode != 'w') ||
      !*path || strlen(path) > 72 || (*mode == 'w' && (_writer || (length != SIZE_MAX && length > bankLimit(_maps[_active != 0]) - ArdFSDetail::Sector - ArdFSDetail::Header - strlen(path))))) {
    _error = -22;
    return f;
  }
  if (*mode == 'w') f._handle.reset(ArdFSFile::Writer::create(path));
  else f._handle.reset(new (std::nothrow) ArdFSFile::Handle);
  if (!f._handle) { _error = -12; return f; }
  auto& h = *f._handle;
  h.volume = this;
  if (h.writing) {
    auto& h = static_cast<ArdFSFile::Writer&>(*f._handle);
    if (!exists(path) && _entries.count >= 32) { f._handle.reset(); _error = -28; return f; }
    h.expected = length;
    h.capacity = length == SIZE_MAX ? std::min<uint32_t>(bankLimit(_maps[_active != 0]), _bankSize - ArdFSDetail::Sector) - ArdFSDetail::Sector - ArdFSDetail::Header - h.pathLength : length;
    if (length == SIZE_MAX) {
      for (const Entry* e = _entries.head; e; e = e->next) {
        if (!strcmp(e->path, h.path)) continue;
        uint32_t used = ArdFSDetail::aligned(ArdFSDetail::Header + e->pathLength + e->length);
        if (used > h.capacity) { f._handle.reset(); _error = -28; return f; }
        h.capacity -= used;
      }
    }
    if (!h.allocateBuffer()) { f._handle.reset(); _error = -12; return f; }
    _writer = true;
    if (!deferred) {
      int status;
      do { status = f.prepareStep(); } while (status == 0);
      if (status < 0) { f.close(); _error = -5; return f; }
    }
  } else {
    auto it = _entries.find(path);
    if (!it) { f._handle.reset(); _error = -2; return f; }
    h.address = it->address;
    h.length = it->length;
  }
  _error = 0;
  return f;
}

inline void ArdFSFile::HandleDeleter::operator()(Handle* handle) const noexcept {
  if (!handle) return;
  if (handle->writing) delete static_cast<Writer*>(handle);
  else delete handle;
}

inline ArdFSFile::operator bool() const { return bool(_handle); }
inline size_t ArdFSFile::size() const { return _handle ? _handle->length : 0; }
inline int ArdFSFile::read(uint8_t* p, size_t n) {
  if (!_handle || _handle->writing) return -1;
  auto& h = *_handle;
  n = std::min<size_t>(n, h.length - h.offset);
  if (!h.volume->readRaw(h.address + h.offset, p, n)) return -1;
  h.offset += n;
  return n;
}
inline size_t ArdFSFile::queueWrite(const uint8_t* p, size_t n) {
  if (!_handle || !_handle->writing || !n || n > 256) return 0;
  auto& h = static_cast<Writer&>(*_handle);
  if (h.pendingSize || h.mutationSource || h.flushStage || h.failed || h.committed || h.prepareStage != 10 || h.finishStage || n > h.capacity - h.length) return 0;
  ArdFSVolume::bufferInput(h, p, n);
  return n;
}
inline size_t ArdFSFile::write(const uint8_t* p, size_t n) {
  if (!_handle || !_handle->writing) return 0;
  auto& h = static_cast<Writer&>(*_handle);
  if (h.mutationSource || h.flushStage || h.failed || h.committed || h.prepareStage != 10 || h.finishStage != 0 || n > h.capacity - h.length) { h.failed = true; return 0; }
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
  if (!_handle) return -1;
  if (!_handle->writing) return 1;
  auto& h = static_cast<Writer&>(*_handle);
  if (h.failed) return -1;
  if (h.mutationSource || h.committed) return 1;
  if (h.prepareStage != 10) return -1;
  int status = h.volume->writeBufferedStep(h);
  if (status < 0) { h.failed = true; h.volume->_error = -5; }
  return status;
}

inline int ArdFSFile::prepareStep() {
  if (!_handle) return -1;
  if (!_handle->writing) return 1;
  auto& h = static_cast<Writer&>(*_handle);
  if (h.failed) return -1;
  int status = h.volume->prepareStep(h);
  if (status < 0) { h.failed = true; h.volume->_error = -5; }
  return status;
}
inline int ArdFSFile::flushStep() {
  if (!_handle) return -1;
  if (!_handle->writing) return 1;
  auto& h = static_cast<Writer&>(*_handle);
  if (h.committed) return 1;
  if (h.failed || h.prepareStage != 10 || (h.expected != SIZE_MAX && h.length != h.expected)) return -1;
  auto& v = *h.volume;
  if (!h.flushStage) h.flushStage = 1;
  int status = 1;
  switch (h.flushStage) {
    case 1:
      if (!h.mutationSource) status = v.writeBufferedStep(h, true);
      break;
    case 2:
      status = v.terminateStep(h);
      break;
    case 3:
      if (!h.mutationSource) {
        status = v.finishRecordStep(h);
        if (status == 1) v._staged.find(h.path)->length = h.length;
      }
      break;
    case 4:
      if (h.compacting || v._workMap->count != h.initialBadCount)
        status = v.publishMapStep(h.map, h.sectorBuffer->data);
      break;
    default:
      if (h.compacting) {
        v._active = h.bank;
        ++v._generation;
        v._entries.clear();
      }
      v._entries.merge(v._staged);
      v._next = h.record + (h.mutationSource ? 0 : ArdFSDetail::aligned(ArdFSDetail::Header + h.pathLength + h.length));
      h.committed = true;
      h.sectorBuffer = nullptr; h.ownedBuffer.reset();
      return 1;
  }
  if (status < 0) { h.failed = true; v._error = -5; return -1; }
  if (status == 1) ++h.flushStage;
  return 0;
}
inline bool ArdFSFile::flush() {
  int status;
  do { status = flushStep(); } while (status == 0);
  return status == 1;
}
inline bool ArdFSFile::close() {
  if (_handle && _handle->writing) {
    auto& h = static_cast<Writer&>(*_handle);
    // An abandoned body has no published header. Force compaction before another
    // live append, otherwise a reboot could stop scanning before that append.
    if (!h.committed && !h.compacting) h.volume->_next = h.volume->_bankSize;
    h.volume->_writer = false;
    h.volume->_staged.clear();
    h.volume->_working = false;
  }
  _handle.reset();
  return true;
}

inline size_t ArdFSFile::position() const { return _handle ? (_handle->writing ? _handle->length : _handle->offset) : 0; }
inline bool ArdFSFile::seek(size_t position) {
  if (!_handle || _handle->writing || position > _handle->length) return false;
  _handle->offset = position;
  return true;
}
inline size_t ArdFSVolume::usedBytes() const {
  size_t used = 0;
  if (_mounted) for (const Entry* e = _entries.head; e; e = e->next)
    used += ArdFSDetail::aligned(ArdFSDetail::Header + e->pathLength + e->length);
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
  if (!_mounted || _writer || !source || !*source || strlen(source) > 72 ||
      (destination && (!*destination || strlen(destination) > 72))) { _error = -22; return f; }
  if (!_entries.find(source)) { _error = -2; return f; }
  if (destination && _entries.find(destination)) { _error = -17; return f; }
  f._handle.reset(ArdFSFile::Writer::create(destination ? destination : ""));
  if (!f._handle) { _error = -12; return f; }
  auto& h = static_cast<ArdFSFile::Writer&>(*f._handle);
  h.mutationSource = _entries.find(source)->path;
  if (!h.allocateBuffer()) { f._handle.reset(); _error = -12; return f; }
  h.volume = this; h.expected = 0;
  _writer = true; _error = 0;
  return f;
}

inline ArdFSFile::Writer* ArdFSFile::Writer::create(const char* path) {
  size_t length = strlen(path);
  void* memory = ArdAllocation::allocate(sizeof(Writer) + length + 1);
  if (!memory) return nullptr;
  auto* writer = ::new (memory) Writer;
  char* name = static_cast<char*>(memory) + sizeof(Writer);
  memcpy(name, path, length + 1);
  writer->path = name; writer->pathLength = length;
  return writer;
}

inline const char* ArdFSFile::Writer::copyPath(const char* source) const {
  if (mutationSource) {
    if (source != mutationSource) return source;
    return pathLength ? path : nullptr;
  }
  return !strcmp(source, path) ? nullptr : source;
}
