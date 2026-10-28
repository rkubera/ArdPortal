#pragma once
// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#if defined(ESP8266)
#include <flash_hal.h>
#endif
#include <algorithm>
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
  for (Entry* e = head; e; e = e->next) if (e->path == path) return e;
  return nullptr;
}
inline ArdFSVolume::Entry* ArdFSVolume::Index::at(size_t index) const {
  Entry* e = head;
  while (e && index--) e = e->next;
  return e;
}
inline bool ArdFSVolume::Index::set(const String& path, uint32_t address, uint32_t length) {
  Entry* e = find(path.c_str());
  if (!e) {
    if (count == 32) return false;
    e = new (std::nothrow) Entry;
    if (!e) return false;
    e->path = path;
    if (e->path != path) { delete e; return false; }
    e->next = head; head = e; ++count;
  }
  e->address = address; e->length = length;
  return true;
}
inline void ArdFSVolume::Index::merge(Index& other) {
  while (other.head) {
    Entry* e = other.head; other.head = e->next;
    Entry* old = find(e->path.c_str());
    if (old) { old->address = e->address; old->length = e->length; delete e; }
    else { e->next = head; head = e; ++count; }
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
inline int ArdFSVolume::publishStep(uint32_t address, const uint8_t* header, uint8_t* scratch, uint8_t& stage) {
  switch (stage) {
    case 3:
      if (!writeRaw(address, header, ArdFSDetail::Header)) return -1;
      break;
    case 4:
      if (!readRaw(address, scratch, ArdFSDetail::Header) || memcmp(header, scratch, ArdFSDetail::Header)) return -1;
      break;
    case 5:
      ArdFSDetail::put(scratch, ArdFSDetail::Commit);
      if (!writeRaw(address + 24, scratch, 4)) return -1;
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
  _configured = _bankSize >= 8 * ArdFSDetail::Sector && _address % ArdFSDetail::Sector == 0;
  return _configured;
}
inline bool ArdFSVolume::readRaw(uint32_t address, void* bytes, size_t n) {
  if (address > _size || n > _size - address) return false;
#if defined(ESP8266)
  // Read ArdFSDetail::aligned words even when a document starts after an odd-length path.
  alignas(4) uint32_t words[64];
  auto* output = static_cast<uint8_t*>(bytes);
  while (n) {
    uint32_t absolute = _address + address, skip = absolute & 3U;
    size_t count = std::min<size_t>(n, sizeof(words) - skip);
    size_t rounded = (skip + count + 3) & ~size_t(3);
    if (!ESP.flashRead(absolute - skip, words, rounded)) return false;
    memcpy(output, reinterpret_cast<uint8_t*>(words) + skip, count);
    output += count;
    address += count;
    n -= count;
    yield();
  }
  return true;
#else
  auto* output = static_cast<uint8_t*>(bytes);
  while (n) {
    size_t count = std::min<size_t>(n, 256);
    if (esp_partition_read(_partition, address, output, count) != ESP_OK) return false;
    address += count;
    output += count;
    n -= count;
    yield();
  }
  return true;
#endif
}
/**
 * @brief Write a bounded flash range in small transfers, yielding between chunks.
 * @param address Byte offset within the configured flash region.
 * @param bytes Byte buffer to read or write.
 * @param n Number of bytes or elements to process.
 * @return True if all requested bytes were written; false for invalid bounds or a flash error.
 */
inline bool ArdFSVolume::writeRaw(uint32_t address, const void* bytes, size_t n) {
  if (address > _size || n > _size - address) return false;
  auto* input = static_cast<const uint8_t*>(bytes);
  while (n) {
    size_t count = std::min<size_t>(n, 256);
#if defined(ESP8266)
    if (!ESP.flashWrite(_address + address, input, count)) return false;
#else
    if (esp_partition_write(_partition, address, input, count) != ESP_OK) return false;
#endif
    address += count;
    input += count;
    n -= count;
    yield();
  }
  return true;
}
/**
 * @brief Erase complete flash sectors, yielding between sectors.
 * @param address Byte offset within the configured flash region.
 * @param n Number of bytes or elements to process.
 * @return True if the ArdFSDetail::aligned sector range was erased; false for invalid alignment, bounds or a flash error.
 */
inline bool ArdFSVolume::eraseRaw(uint32_t address, size_t n) {
  if (address % ArdFSDetail::Sector || n % ArdFSDetail::Sector || address > _size || n > _size - address) return false;
  for (size_t i = 0; i < n; i += ArdFSDetail::Sector) {
#if defined(ESP8266)
    if (!ESP.flashEraseSector((_address + address + i) / ArdFSDetail::Sector)) return false;
#else
    if (esp_partition_erase_range(_partition, address + i, ArdFSDetail::Sector) != ESP_OK) return false;
#endif
    yield();
  }
  return true;
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
      _entries.clear(); _scanPath = String(); _mountStage = 1; return 0;
    case 1:
    case 2:
      if (!readRaw((_mountStage - 1) * _bankSize, _bankHeaders[_mountStage - 1], ArdFSDetail::Header)) { _error = -5; return -1; }
      ++_mountStage; return 0;
    case 3:
      {
        bool valid[2];
        for (unsigned i = 0; i < 2; ++i) {
          const uint8_t* h = _bankHeaders[i];
          valid[i] = ArdFSDetail::get(h) == ArdFSDetail::BankMagic && ArdFSDetail::get(h + 4) == 1 && ArdFSDetail::get(h + 12) == _bankSize && ArdFSDetail::get(h + 24) == ArdFSDetail::Commit && ArdFSDetail::get(h + 16) == ~crc(h, 16);
        }
        int first = valid[1] && (!valid[0] || int32_t(ArdFSDetail::get(_bankHeaders[1] + 8) - ArdFSDetail::get(_bankHeaders[0] + 8)) > 0) ? 1 : 0;
        if (!valid[first]) { _error = -84; return -1; }
        _active = first * _bankSize;
        _generation = ArdFSDetail::get(_bankHeaders[first] + 8);
        _next = ArdFSDetail::Sector; _mountStage = 4; return 0;
      }
    case 4:
      if (_next + ArdFSDetail::Header > _bankSize) { _mountStage = 7; return 0; }
      if (!readRaw(_active + _next, bytes, ArdFSDetail::Header)) { _error = -5; return -1; }
      if (ArdFSDetail::get(bytes) == UINT32_MAX) { _mountStage = 7; return 0; }
      _scanExtent = ArdFSDetail::get(bytes + 4); _scanName = ArdFSDetail::get(bytes + 8); _scanLength = ArdFSDetail::get(bytes + 12);
      if (!_scanName || _scanName > 72 || _scanLength > _bankSize - ArdFSDetail::Header - _scanName ||
          ArdFSDetail::get(bytes) != ArdFSDetail::RecordMagic || _scanExtent != ArdFSDetail::aligned(ArdFSDetail::Header + _scanName + _scanLength) || _scanExtent > _bankSize - _next) {
        _next = _bankSize; _mountStage = 7; return 0;
      }
      if (ArdFSDetail::get(bytes + 24) != ArdFSDetail::Commit) { _next += _scanExtent; return 0; }
      _scanCrc = crc(bytes, 16); _scanExpected = ArdFSDetail::get(bytes + 16);
      _scanPath = String(); _scanAt = 0; _mountStage = 5; return 0;
    case 5:
      if (!_scanPath.reserve(_scanName)) { _error = -12; return -1; }
      _mountStage = 6; return 0;
    case 6:
      {
        size_t count = std::min<size_t>(sizeof(bytes), _scanName + _scanLength - _scanAt);
        if (count) {
          if (!readRaw(_active + _next + ArdFSDetail::Header + _scanAt, bytes, count)) { _error = -5; return -1; }
          _scanCrc = crc(bytes, count, _scanCrc);
          for (size_t i = 0; i < count && _scanAt + i < _scanName; ++i) _scanPath += char(bytes[i]);
          _scanAt += count; return 0;
        }
        if (~_scanCrc == _scanExpected) {
          if (!_entries.set(_scanPath, _active + _next + ArdFSDetail::Header + _scanName, _scanLength)) {
            _error = _entries.count == 32 ? -28 : -12; return -1;
          }
        }
        _next += _scanExtent; _mountStage = 4; return 0;
      }
    default:
      _scanPath = String(); _mounted = true; _error = 0; return 1;
  }
}
inline bool ArdFSVolume::format() {
  int status;
  do { status = formatStep(); } while (status == 0);
  return status == 1;
}
inline int ArdFSVolume::formatStep() {
  uint8_t h[ArdFSDetail::Header];
  switch (_formatStage) {
    case 0:
      if (!configure()) { _error = -22; return -1; }
      _mounted = false; _mountStage = 0; _formatAt = 0; _formatStage = 1; return 0;
    case 1:
      if (_formatAt == _bankSize * 2) { _formatStage = 2; return 0; }
      if (!eraseRaw(_formatAt, ArdFSDetail::Sector)) { _error = -5; return -1; }
      _formatAt += ArdFSDetail::Sector; return 0;
    case 2:
      ArdFSDetail::put(h + 16, ~makeHeader(h, ArdFSDetail::BankMagic, 1, 1, _bankSize));
      if (!writeRaw(0, h, ArdFSDetail::Header)) { _error = -5; return -1; }
      _formatStage = 3; return 0;
    case 3:
      ArdFSDetail::put(h, ArdFSDetail::Commit);
      if (!writeRaw(24, h, 4)) { _error = -5; return -1; }
      _formatStage = 0; _error = 0; return 1;
    default: _error = -22; return -1;
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
// Reserve the successor marker first; then inspect/erase the record in steps.
inline int ArdFSVolume::reserveStep(ArdFSFile::Writer& h) {
  switch (h.reserveStage) {
    case 0:
      h.extent = ArdFSDetail::aligned(ArdFSDetail::Header + h.path.length() + h.capacity);
      if (h.record > _bankSize || h.extent > _bankSize - h.record) return -1;
      h.workAt = 0;
      h.reserveStage = h.record + h.extent < _bankSize ? 1 : 3;
      return 0;
    case 1:
      if (!readRaw(h.bank + h.record + h.extent, h.bytes, ArdFSDetail::Header)) return -1;
      h.reserveStage = 3;
      for (size_t i = 0; i < ArdFSDetail::Header; ++i)
        if (h.bytes[i] != 0xff) { h.reserveStage = 2; break; }
      return 0;
    case 2:
      if (!eraseRaw(h.bank + h.record + h.extent, ArdFSDetail::Sector)) return -1;
      h.reserveStage = 3;
      return 0;
    case 3:
      if (h.workAt == h.extent) { h.reserveStage = 5; return 1; }
      if (!readRaw(h.bank + h.record + h.workAt, h.bytes, sizeof(h.bytes))) return -1;
      for (uint8_t b : h.bytes)
        if (b != 0xff) { h.workAt = 0; h.reserveStage = 4; return 0; }
      h.workAt += sizeof(h.bytes);
      return 0;
    case 4:
      if (h.workAt == h.extent) { h.reserveStage = 5; return 1; }
      if (!eraseRaw(h.bank + h.record + h.workAt, ArdFSDetail::Sector)) return -1;
      h.workAt += ArdFSDetail::Sector;
      return 0;
    default: return 1;
  }
}

inline int ArdFSVolume::prepareStep(ArdFSFile::Writer& h) {
  switch (h.prepareStage) {
    case 0:
      {
        uint32_t extent = ArdFSDetail::aligned(ArdFSDetail::Header + h.path.length() + h.capacity);
        if (extent > _bankSize - ArdFSDetail::Sector) return -1;
        h.bank = _active;
        h.record = _next;
        _staged.clear();
        if (h.mutationSource || _next > _bankSize || extent > _bankSize - _next) {
          uint32_t required = h.mutationSource ? 0 : extent;
          for (const Entry* e = _entries.head; e; e = e->next) {
            const String* target = h.copyPath(e->path);
            if (target) required += ArdFSDetail::aligned(ArdFSDetail::Header + target->length() + e->length);
          }
          if (required > _bankSize - ArdFSDetail::Sector) return -1;
          h.compacting = true;
          h.bank = _active ? 0 : _bankSize;
          h.record = ArdFSDetail::Sector;
          h.prepareStage = 1;
        } else h.prepareStage = 8;
        return 0;
      }
    case 1:
      if (!eraseRaw(h.bank, ArdFSDetail::Sector)) return -1;
      h.prepareStage = 2;
      return 0;
    case 2:
      {
        // At most 32 paths; metadata work is independent of file sizes.
        while (h.copyIndex < _entries.count && !h.copyPath(_entries.at(h.copyIndex)->path)) ++h.copyIndex;
        if (h.copyIndex == _entries.count) { h.prepareStage = 8; return 0; }
        const Entry& e = *_entries.at(h.copyIndex);
        h.copy.reset(new (std::nothrow) ArdFSFile::Writer);
        if (!h.copy) return -1;
        auto& copy = *h.copy;
        copy.volume = this;
        const String& target = *h.copyPath(e.path);
        copy.path = target;
        if (copy.path != target) return -1;
        copy.bank = h.bank;
        copy.record = h.record;
        copy.capacity = copy.length = e.length;
        copy.address = copy.bank + copy.record + ArdFSDetail::Header + copy.path.length();
        h.copyAt = 0;
        h.prepareStage = 3;
        return 0;
      }
    case 3:
      {
        int status = reserveStep(*h.copy);
        if (status == 1) h.prepareStage = 4;
        return status < 0 ? -1 : 0;
      }
    case 4:
      if (!writeRaw(h.copy->bank + h.copy->record + ArdFSDetail::Header, h.copy->path.c_str(), h.copy->path.length())) return -1;
      h.prepareStage = 5;
      return 0;
    case 5:
      if (h.copyAt == h.copy->length) { h.prepareStage = 7; return 0; }
      h.copyCount = std::min<size_t>(sizeof(h.bytes), h.copy->length - h.copyAt);
      if (!readRaw(_entries.at(h.copyIndex)->address + h.copyAt, h.bytes, h.copyCount)) return -1;
      h.prepareStage = 6;
      return 0;
    case 6:
      if (!writeRaw(h.copy->address + h.copyAt, h.bytes, h.copyCount)) return -1;
      h.copy->checksum = crc(h.bytes, h.copyCount, h.copy->checksum);
      h.copyAt += h.copyCount;
      h.prepareStage = 5;
      return 0;
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
      {
        if (h.mutationSource) {
          // Terminate the copied log, including an empty bank after deletion.
          if (h.record < _bankSize && !eraseRaw(h.bank + h.record, ArdFSDetail::Sector)) return -1;
          h.prepareStage = 10; return 1;
        }
        int status = reserveStep(h);
        if (status < 0) return -1;
        if (status == 1) {
          h.address = h.bank + h.record + ArdFSDetail::Header + h.path.length();
          h.prepareStage = 9;
        }
        return 0;
      }
    case 9:
      if (!writeRaw(h.bank + h.record + ArdFSDetail::Header, h.path.c_str(), h.path.length())) return -1;
      if (!_staged.set(h.path, h.address, 0)) return -1;
      h.prepareStage = 10;
      return 0;
    default: return 1;
  }
}

inline int ArdFSVolume::finishRecordStep(ArdFSFile::Writer& h) {
  switch (h.finishStage) {
    case 0:
      h.verified = makeHeader(h.header, ArdFSDetail::RecordMagic, ArdFSDetail::aligned(ArdFSDetail::Header + h.path.length() + h.length), h.path.length(), h.length);
      h.verifiedBody = UINT32_MAX;
      h.workAt = 0;
      h.finishStage = 1;
      return 0;
    case 1:
      if (!readRaw(h.bank + h.record + ArdFSDetail::Header, h.bytes, h.path.length()) ||
          memcmp(h.bytes, h.path.c_str(), h.path.length())) return -1;
      h.verified = crc(h.bytes, h.path.length(), h.verified);
      h.finishStage = 2;
      return 0;
    case 2:
      {
        if (h.workAt == h.length) {
          if (h.verifiedBody != h.checksum) return -1;
          ArdFSDetail::put(h.header + 16, ~h.verified);
          h.finishStage = 3;
          return 0;
        }
        size_t n = std::min<size_t>(sizeof(h.bytes), h.length - h.workAt);
        if (!readRaw(h.address + h.workAt, h.bytes, n)) return -1;
        h.verified = crc(h.bytes, n, h.verified);
        h.verifiedBody = crc(h.bytes, n, h.verifiedBody);
        h.workAt += n;
        return 0;
      }
    default: return publishStep(h.bank + h.record, h.header, h.bytes, h.finishStage);
  }
}

inline ArdFSFile ArdFSVolume::open(const char* path, const char* mode, size_t length, bool deferred) {
  ArdFSFile f;
  if (!_mounted || !path || !mode || (*mode != 'r' && *mode != 'w') ||
      !*path || strlen(path) > 72 || (*mode == 'w' && (_writer || (length != SIZE_MAX && length > _bankSize - ArdFSDetail::Sector - ArdFSDetail::Header - strlen(path))))) {
    _error = -22;
    return f;
  }
  if (*mode == 'w') f._handle.reset(new (std::nothrow) ArdFSFile::Writer);
  else f._handle.reset(new (std::nothrow) ArdFSFile::Handle);
  if (!f._handle) { _error = -12; return f; }
  auto& h = *f._handle;
  h.volume = this;
  if (h.writing) {
    auto& h = static_cast<ArdFSFile::Writer&>(*f._handle);
    h.path = path;
    if (h.path.length() != strlen(path)) { f._handle.reset(); _error = -12; return f; }
    if (!exists(path) && _entries.count >= 32) { f._handle.reset(); _error = -28; return f; }
    h.expected = length;
    h.capacity = length == SIZE_MAX ? _bankSize - ArdFSDetail::Sector - ArdFSDetail::Header - h.path.length() : length;
    if (length == SIZE_MAX) {
      for (const Entry* e = _entries.head; e; e = e->next) {
        if (e->path == h.path) continue;
        uint32_t used = ArdFSDetail::aligned(ArdFSDetail::Header + e->path.length() + e->length);
        if (used > h.capacity) { f._handle.reset(); _error = -28; return f; }
        h.capacity -= used;
      }
    }
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
inline size_t ArdFSFile::write(const uint8_t* p, size_t n) {
  if (!_handle || !_handle->writing) return 0;
  auto& h = static_cast<Writer&>(*_handle);
  if (h.mutationSource || h.failed || h.committed || h.prepareStage != 10 || h.finishStage != 0 || n > h.capacity - h.length) { h.failed = true; return 0; }
  if (!h.volume->writeRaw(h.address + h.length, p, n)) { h.failed = true; return 0; }
  h.checksum = ArdFSVolume::crc(p, n, h.checksum);
  h.length += n;
  return n;
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
  switch (h.flushStage) {
    case 0:
      {
        if (h.mutationSource) { h.flushStage = 1; return 0; }
        // Reserve all index memory before any commit marker is published.
        v._staged.find(h.path.c_str())->length = h.length;
        int status = v.finishRecordStep(h);
        if (status < 0) { h.failed = true; v._error = -5; return -1; }
        if (status == 1) h.flushStage = h.compacting ? 1 : 6;
        return 0;
      }
    case 1:
      ArdFSDetail::put(h.header + 16, ~ArdFSVolume::makeHeader(h.header, ArdFSDetail::BankMagic, 1, v._generation + 1, v._bankSize));
      h.flushStage = 3;
      return 0;
    case 3:
    case 4:
    case 5:
      if (v.publishStep(h.bank, h.header, h.bytes, h.flushStage) < 0) {
        h.failed = true; v._error = -5; return -1;
      }
      return 0;
    default:
      if (h.compacting) {
        v._active = h.bank;
        ++v._generation;
        v._entries.clear();
      }
      v._entries.merge(v._staged);
      v._next = h.record + (h.mutationSource ? 0 : ArdFSDetail::aligned(ArdFSDetail::Header + h.path.length() + h.length));
      h.committed = true;
      return 1;
  }
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
    used += ArdFSDetail::aligned(ArdFSDetail::Header + e->path.length() + e->length);
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
  f._handle.reset(new (std::nothrow) ArdFSFile::Writer);
  if (!f._handle) { _error = -12; return f; }
  auto& h = static_cast<ArdFSFile::Writer&>(*f._handle);
  h.mutationSource = &_entries.find(source)->path;
  if (destination) h.path = destination;
  if ((destination && h.path != destination)) {
    f._handle.reset(); _error = -12; return f;
  }
  h.volume = this; h.expected = 0;
  _writer = true; _error = 0;
  return f;
}

inline const String* ArdFSFile::Writer::copyPath(const String& source) const {
  if (mutationSource) {
    if (&source != mutationSource) return &source;
    return path.length() ? &path : nullptr;
  }
  return source == path ? nullptr : &source;
}
