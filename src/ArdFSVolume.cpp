// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#include "ArdFSVolume.h"
#if defined(ESP8266)
#include <flash_hal.h>
#endif
#include <algorithm>
namespace {
constexpr uint32_t BankMagic = 0x31445241, RecordMagic = 0x31524341, Commit = 0x51ac0ffe;
constexpr uint32_t Sector = 4096, Header = 32;
/**
 * @brief Round a journal extent up to a flash sector boundary.
 * @param n Number of bytes or elements to process.
 * @return The smallest whole-sector extent containing the requested byte count.
 */
uint32_t aligned(uint32_t n) {
  return (n + Sector - 1) / Sector * Sector;
}
// Fields are explicitly little endian, independent of compiler struct packing.
/**
 * @brief Encode a 32-bit word into little-endian journal bytes.
 * @param p Destination for four bytes.
 * @param n Word to encode.
 * @return No value.
 */
void put(uint8_t* p, uint32_t n) {
  for (int i = 0; i < 4; ++i) p[i] = uint8_t(n >> (8 * i));
}
/**
 * @brief Decode a little-endian 32-bit journal word.
 * @param p Pointer to four source bytes.
 * @return Decoded word.
 */
uint32_t get(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
}
void ArdFSVolume::Index::clear() {
  while (head) { Entry* next = head->next; delete head; head = next; }
  count = 0;
}
ArdFSVolume::Entry* ArdFSVolume::Index::find(const char* path) const {
  for (Entry* e = head; e; e = e->next) if (e->path == path) return e;
  return nullptr;
}
ArdFSVolume::Entry* ArdFSVolume::Index::at(size_t index) const {
  Entry* e = head;
  while (e && index--) e = e->next;
  return e;
}
bool ArdFSVolume::Index::set(const String& path, uint32_t address, uint32_t length) {
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
void ArdFSVolume::Index::merge(Index& other) {
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
uint32_t ArdFSVolume::crc(const void* bytes, size_t length, uint32_t c) {
  auto p = static_cast<const uint8_t*>(bytes);
  for (size_t i = 0; i < length; ++i) {
    c ^= p[i];
    for (int j = 0; j < 8; ++j) c = (c >> 1) ^ (0xedb88320U & (0U - (c & 1)));
  }
  return c;
}
// Shared layout and atomic publication for record and bank headers.
uint32_t ArdFSVolume::makeHeader(uint8_t* bytes, uint32_t magic, uint32_t a, uint32_t b, uint32_t c) {
  memset(bytes, 255, Header);
  put(bytes, magic); put(bytes + 4, a); put(bytes + 8, b); put(bytes + 12, c);
  return crc(bytes, 16);
}
int ArdFSVolume::publishStep(uint32_t address, const uint8_t* header, uint8_t* scratch, uint8_t& stage) {
  switch (stage) {
    case 3:
      if (!writeRaw(address, header, Header)) return -1;
      break;
    case 4:
      if (!readRaw(address, scratch, Header) || memcmp(header, scratch, Header)) return -1;
      break;
    case 5:
      put(scratch, Commit);
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
bool ArdFSVolume::configure() {
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
  _bankSize = (_size / (2 * Sector)) * Sector;
  _configured = _bankSize >= 8 * Sector && _address % Sector == 0;
  return _configured;
}
bool ArdFSVolume::readRaw(uint32_t address, void* bytes, size_t n) {
  if (address > _size || n > _size - address) return false;
#if defined(ESP8266)
  // Read aligned words even when a document starts after an odd-length path.
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
bool ArdFSVolume::writeRaw(uint32_t address, const void* bytes, size_t n) {
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
 * @return True if the aligned sector range was erased; false for invalid alignment, bounds or a flash error.
 */
bool ArdFSVolume::eraseRaw(uint32_t address, size_t n) {
  if (address % Sector || n % Sector || address > _size || n > _size - address) return false;
  for (size_t i = 0; i < n; i += Sector) {
#if defined(ESP8266)
    if (!ESP.flashEraseSector((_address + address + i) / Sector)) return false;
#else
    if (esp_partition_erase_range(_partition, address + i, Sector) != ESP_OK) return false;
#endif
    yield();
  }
  return true;
}
// Synchronous compatibility wrappers drain the same bounded state machines.
bool ArdFSVolume::begin() {
  int status;
  do { status = beginStep(); } while (status == 0);
  return status == 1;
}
int ArdFSVolume::beginStep() {
  if (_mounted) return 1;
  uint8_t bytes[256];
  switch (_mountStage) {
    case 0:
      if (!configure()) { _error = -22; return -1; }
      _entries.clear(); _scanPath = String(); _mountStage = 1; return 0;
    case 1:
    case 2:
      if (!readRaw((_mountStage - 1) * _bankSize, _bankHeaders[_mountStage - 1], Header)) { _error = -5; return -1; }
      ++_mountStage; return 0;
    case 3:
      {
        bool valid[2];
        for (unsigned i = 0; i < 2; ++i) {
          const uint8_t* h = _bankHeaders[i];
          valid[i] = get(h) == BankMagic && get(h + 4) == 1 && get(h + 12) == _bankSize && get(h + 24) == Commit && get(h + 16) == ~crc(h, 16);
        }
        int first = valid[1] && (!valid[0] || int32_t(get(_bankHeaders[1] + 8) - get(_bankHeaders[0] + 8)) > 0) ? 1 : 0;
        if (!valid[first]) { _error = -84; return -1; }
        _active = first * _bankSize;
        _generation = get(_bankHeaders[first] + 8);
        _next = Sector; _mountStage = 4; return 0;
      }
    case 4:
      if (_next + Header > _bankSize) { _mountStage = 7; return 0; }
      if (!readRaw(_active + _next, bytes, Header)) { _error = -5; return -1; }
      if (get(bytes) == UINT32_MAX) { _mountStage = 7; return 0; }
      _scanExtent = get(bytes + 4); _scanName = get(bytes + 8); _scanLength = get(bytes + 12);
      if (!_scanName || _scanName > 72 || _scanLength > _bankSize - Header - _scanName ||
          get(bytes) != RecordMagic || _scanExtent != aligned(Header + _scanName + _scanLength) || _scanExtent > _bankSize - _next) {
        _next = _bankSize; _mountStage = 7; return 0;
      }
      if (get(bytes + 24) != Commit) { _next += _scanExtent; return 0; }
      _scanCrc = crc(bytes, 16); _scanExpected = get(bytes + 16);
      _scanPath = String(); _scanAt = 0; _mountStage = 5; return 0;
    case 5:
      if (!_scanPath.reserve(_scanName)) { _error = -12; return -1; }
      _mountStage = 6; return 0;
    case 6:
      {
        size_t count = std::min<size_t>(sizeof(bytes), _scanName + _scanLength - _scanAt);
        if (count) {
          if (!readRaw(_active + _next + Header + _scanAt, bytes, count)) { _error = -5; return -1; }
          _scanCrc = crc(bytes, count, _scanCrc);
          for (size_t i = 0; i < count && _scanAt + i < _scanName; ++i) _scanPath += char(bytes[i]);
          _scanAt += count; return 0;
        }
        if (~_scanCrc == _scanExpected) {
          if (!_entries.set(_scanPath, _active + _next + Header + _scanName, _scanLength)) {
            _error = _entries.count == 32 ? -28 : -12; return -1;
          }
        }
        _next += _scanExtent; _mountStage = 4; return 0;
      }
    default:
      _scanPath = String(); _mounted = true; _error = 0; return 1;
  }
}
bool ArdFSVolume::format() {
  int status;
  do { status = formatStep(); } while (status == 0);
  return status == 1;
}
int ArdFSVolume::formatStep() {
  uint8_t h[Header];
  switch (_formatStage) {
    case 0:
      if (!configure()) { _error = -22; return -1; }
      _mounted = false; _mountStage = 0; _formatAt = 0; _formatStage = 1; return 0;
    case 1:
      if (_formatAt == _bankSize * 2) { _formatStage = 2; return 0; }
      if (!eraseRaw(_formatAt, Sector)) { _error = -5; return -1; }
      _formatAt += Sector; return 0;
    case 2:
      put(h + 16, ~makeHeader(h, BankMagic, 1, 1, _bankSize));
      if (!writeRaw(0, h, Header)) { _error = -5; return -1; }
      _formatStage = 3; return 0;
    case 3:
      put(h, Commit);
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
bool ArdFSVolume::exists(const char* path) {
  bool found = _mounted && _entries.find(path);
  _error = found ? 0 : -2;
  return found;
}
// Reserve the successor marker first; then inspect/erase the record in steps.
int ArdFSVolume::reserveStep(ArdFSFile::Handle& h) {
  switch (h.reserveStage) {
    case 0:
      h.extent = aligned(Header + h.path.length() + h.capacity);
      if (h.record > _bankSize || h.extent > _bankSize - h.record) return -1;
      h.workAt = 0;
      h.reserveStage = h.record + h.extent < _bankSize ? 1 : 3;
      return 0;
    case 1:
      if (!readRaw(h.bank + h.record + h.extent, h.bytes, Header)) return -1;
      h.reserveStage = 3;
      for (size_t i = 0; i < Header; ++i)
        if (h.bytes[i] != 0xff) { h.reserveStage = 2; break; }
      return 0;
    case 2:
      if (!eraseRaw(h.bank + h.record + h.extent, Sector)) return -1;
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
      if (!eraseRaw(h.bank + h.record + h.workAt, Sector)) return -1;
      h.workAt += Sector;
      return 0;
    default: return 1;
  }
}

int ArdFSVolume::prepareStep(ArdFSFile::Handle& h) {
  switch (h.prepareStage) {
    case 0:
      {
        uint32_t extent = aligned(Header + h.path.length() + h.capacity);
        if (extent > _bankSize - Sector) return -1;
        h.bank = _active;
        h.record = _next;
        _staged.clear();
        if (_next > _bankSize || extent > _bankSize - _next) {
          uint32_t required = extent;
          for (const Entry* e = _entries.head; e; e = e->next)
            if (e->path != h.path) required += aligned(Header + e->path.length() + e->length);
          if (required > _bankSize - Sector) return -1;
          h.compacting = true;
          h.bank = _active ? 0 : _bankSize;
          h.record = Sector;
          h.prepareStage = 1;
        } else h.prepareStage = 8;
        return 0;
      }
    case 1:
      if (!eraseRaw(h.bank, Sector)) return -1;
      h.prepareStage = 2;
      return 0;
    case 2:
      {
        // At most 32 paths; metadata work is independent of file sizes.
        while (h.copyIndex < _entries.count && _entries.at(h.copyIndex)->path == h.path) ++h.copyIndex;
        if (h.copyIndex == _entries.count) { h.prepareStage = 8; return 0; }
        const Entry& e = *_entries.at(h.copyIndex);
        h.copy.reset(new (std::nothrow) ArdFSFile::Handle);
        if (!h.copy) return -1;
        auto& copy = *h.copy;
        copy.volume = this;
        copy.path = e.path;
        if (copy.path != e.path) return -1;
        copy.bank = h.bank;
        copy.record = h.record;
        copy.capacity = copy.length = e.length;
        copy.address = copy.bank + copy.record + Header + copy.path.length();
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
      if (!writeRaw(h.copy->bank + h.copy->record + Header, h.copy->path.c_str(), h.copy->path.length())) return -1;
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
        int status = reserveStep(h);
        if (status < 0) return -1;
        if (status == 1) {
          h.address = h.bank + h.record + Header + h.path.length();
          h.prepareStage = 9;
        }
        return 0;
      }
    case 9:
      if (!writeRaw(h.bank + h.record + Header, h.path.c_str(), h.path.length())) return -1;
      if (!_staged.set(h.path, h.address, 0)) return -1;
      h.prepareStage = 10;
      return 0;
    default: return 1;
  }
}

int ArdFSVolume::finishRecordStep(ArdFSFile::Handle& h) {
  switch (h.finishStage) {
    case 0:
      h.verified = makeHeader(h.header, RecordMagic, aligned(Header + h.path.length() + h.length), h.path.length(), h.length);
      h.verifiedBody = UINT32_MAX;
      h.workAt = 0;
      h.finishStage = 1;
      return 0;
    case 1:
      if (!readRaw(h.bank + h.record + Header, h.bytes, h.path.length()) ||
          memcmp(h.bytes, h.path.c_str(), h.path.length())) return -1;
      h.verified = crc(h.bytes, h.path.length(), h.verified);
      h.finishStage = 2;
      return 0;
    case 2:
      {
        if (h.workAt == h.length) {
          if (h.verifiedBody != h.checksum) return -1;
          put(h.header + 16, ~h.verified);
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

ArdFSFile ArdFSVolume::open(const char* path, const char* mode, size_t length, bool deferred) {
  ArdFSFile f;
  if (!_mounted || !path || !mode || (*mode != 'r' && *mode != 'w') ||
      !*path || strlen(path) > 72 || (*mode == 'w' && (_writer || (length != SIZE_MAX && length > _bankSize - Sector - Header - strlen(path))))) {
    _error = -22;
    return f;
  }
  f._handle.reset(new (std::nothrow) ArdFSFile::Handle);
  if (!f._handle) { _error = -12; return f; }
  auto& h = *f._handle;
  h.volume = this;
  h.writing = *mode == 'w';
  if (h.writing) {
    // Readers only need the resolved address and length; avoid a path copy.
    h.path = path;
    if (h.path.length() != strlen(path)) { f._handle.reset(); _error = -12; return f; }
    if (!exists(path) && _entries.count >= 32) { f._handle.reset(); _error = -28; return f; }
    h.expected = length;
    h.capacity = length == SIZE_MAX ? _bankSize - Sector - Header - h.path.length() : length;
    if (length == SIZE_MAX) {
      for (const Entry* e = _entries.head; e; e = e->next) {
        if (e->path == h.path) continue;
        uint32_t used = aligned(Header + e->path.length() + e->length);
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

ArdFSFile::operator bool() const { return bool(_handle); }
size_t ArdFSFile::size() const { return _handle ? _handle->length : 0; }
int ArdFSFile::read(uint8_t* p, size_t n) {
  if (!_handle || _handle->writing) return -1;
  auto& h = *_handle;
  n = std::min<size_t>(n, h.length - h.offset);
  if (!h.volume->readRaw(h.address + h.offset, p, n)) return -1;
  h.offset += n;
  return n;
}
size_t ArdFSFile::write(const uint8_t* p, size_t n) {
  if (!_handle || !_handle->writing) return 0;
  auto& h = *_handle;
  if (h.failed || h.committed || h.prepareStage != 10 || h.finishStage != 0 || n > h.capacity - h.length) { h.failed = true; return 0; }
  if (!h.volume->writeRaw(h.address + h.length, p, n)) { h.failed = true; return 0; }
  h.checksum = ArdFSVolume::crc(p, n, h.checksum);
  h.length += n;
  return n;
}
int ArdFSFile::prepareStep() {
  if (!_handle) return -1;
  auto& h = *_handle;
  if (!h.writing) return 1;
  if (h.failed) return -1;
  int status = h.volume->prepareStep(h);
  if (status < 0) { h.failed = true; h.volume->_error = -5; }
  return status;
}
int ArdFSFile::flushStep() {
  if (!_handle) return -1;
  auto& h = *_handle;
  if (!h.writing || h.committed) return 1;
  if (h.failed || h.prepareStage != 10 || (h.expected != SIZE_MAX && h.length != h.expected)) return -1;
  auto& v = *h.volume;
  switch (h.flushStage) {
    case 0:
      {
        // Reserve all index memory before any commit marker is published.
        v._staged.find(h.path.c_str())->length = h.length;
        int status = v.finishRecordStep(h);
        if (status < 0) { h.failed = true; v._error = -5; return -1; }
        if (status == 1) h.flushStage = h.compacting ? 1 : 6;
        return 0;
      }
    case 1:
      put(h.header + 16, ~ArdFSVolume::makeHeader(h.header, BankMagic, 1, v._generation + 1, v._bankSize));
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
      v._next = h.record + aligned(Header + h.path.length() + h.length);
      h.committed = true;
      return 1;
  }
}
bool ArdFSFile::flush() {
  int status;
  do { status = flushStep(); } while (status == 0);
  return status == 1;
}
bool ArdFSFile::close() {
  if (_handle && _handle->writing) {
    auto& h = *_handle;
    // An abandoned body has no published header. Force compaction before another
    // live append, otherwise a reboot could stop scanning before that append.
    if (!h.committed && !h.compacting) h.volume->_next = h.volume->_bankSize;
    h.volume->_writer = false;
    h.volume->_staged.clear();
  }
  _handle.reset();
  return true;
}
