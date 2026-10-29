// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once

__attribute__((noinline)) inline uint32_t ArdFSVolume::fragmentCount(size_t pathBytes, size_t length) {
  // Configured banks are bounded to 65,535 sectors; the sum fits 32 bits.
  uint32_t bytes = uint32_t(length) + ArdFSDetail::Header + uint32_t(pathBytes) - 4;
  return (bytes + ArdFSDetail::Sector - 7) / (ArdFSDetail::Sector - 6);
}
// The same V4 address rule serves reads and mount-time extent validation.
__attribute__((noinline)) inline uint32_t ArdFSVolume::framedNode(uint32_t position) {
  return (position - 4) / (ArdFSDetail::Sector - 4);
}
inline uint32_t ArdFSVolume::dataSectors(const BankMap& map) const {
  uint32_t bad = map.count;
  uint32_t total = _bankSize / ArdFSDetail::Sector - 2;
  return bad <= 32 && bad < total ? total - bad : 0;
}
inline uint32_t ArdFSVolume::fragmentCapacity(size_t pathBytes) const {
  if (!pathBytes || pathBytes > 72) return 0;
  uint32_t available = dataSectors(_maps[_active != 0]);
  if (!available) available = dataSectors(_maps[_active == 0]);
  uint32_t slots = std::min<uint32_t>(available, (ArdFSDetail::Sector - ArdFSDetail::Header - pathBytes - 2) / 2);
  if (!slots) return 0;
  return slots * (ArdFSDetail::Sector - 4) + 4 - ArdFSDetail::Header - pathBytes - slots * 2;
}
inline void ArdFSVolume::destroyEntry(Entry* e) {
  if (!e) return;
  auto& v = *e->volume;
  auto* uses = v._uses[e->address >= v._bankSize].get();
  if (uses)
    for (unsigned i = 0; i < e->count; ++i) {
      uint16_t id = e->sectors[i];
      if (id < v._bankSize / ArdFSDetail::Sector && uses[id] && uses[id] != 255) --uses[id];
    }
  delete e;
}
inline bool ArdFSVolume::initializeUses(uint32_t bank) {
  unsigned which = bank != 0;
  size_t count = _bankSize / ArdFSDetail::Sector;
  _uses[which].reset(static_cast<uint8_t*>(ArdAllocation::allocate(count)));
  if (!_uses[which]) return false;
  memset(_uses[which].get(), 0, count);
  for (auto id : _maps[which].meta)
    if (id != UINT16_MAX) _uses[which].get()[id] = 255;
  for (unsigned i = 0; i < _maps[which].count; ++i) _uses[which].get()[_maps[which].bad[i]] = 255;
  return true;
}
inline uint32_t ArdFSVolume::freeSectors(uint32_t bank) const {
  const auto* uses = _uses[bank != 0].get();
  uint32_t count = 0;
  if (uses)
    for (uint32_t i = 0; i < _bankSize / ArdFSDetail::Sector; ++i)
      if (!uses[i]) ++count;
  return count;
}
inline int ArdFSVolume::allocateFragment(ArdFSFile::Writer& h, uint16_t& sector) {
  return allocateSector(h.allocationLeft, sector);
}
inline int ArdFSVolume::allocateSector(uint16_t& left, uint16_t& sector) {
  auto* uses = _uses[_workBank != 0].get();
  unsigned budget = 256;
  const uint32_t end = _bankSize / ArdFSDetail::Sector;
  while (left && budget--) {
    uint16_t id = _workMap->nextSector;
    _workMap->nextSector = id + 1U < end ? id + 1 : 0;
    --left;
    if (!uses[id]) {
      uses[id] = 1;
      sector = id;
      left = end;
      return 1;
    }
  }
  if (left) return 0;
  _error = -28;
  return -1;
}
inline bool ArdFSVolume::isBad(const BankMap& map, uint16_t id) const {
  for (unsigned i = 0; i < map.count; ++i)
    if (map.bad[i] == id) return true;
  return false;
}
inline bool ArdFSVolume::retireFragment(ArdFSFile::Writer& h) {
  return retireSector(h.entry->sectors[h.node]);
}
inline bool ArdFSVolume::retireSector(uint16_t id) {
  if (isBad(*_workMap, id)) return true;
  if (_workMap->count == 32) {
    _error = -28;
    return false;
  }
  unsigned at = 0;
  while (at < _workMap->count && _workMap->bad[at] < id) ++at;
  memmove(_workMap->bad + at + 1, _workMap->bad + at, (_workMap->count - at) * sizeof(uint16_t));
  _workMap->bad[at] = id;
  ++_workMap->count;
  if (_uses[_workBank != 0]) _uses[_workBank != 0].get()[id] = 255;
  return true;
}
inline bool ArdFSVolume::readFragments(uint32_t address, const uint16_t* sectors, size_t count, size_t offset, void* bytes, size_t n) {
  uint32_t bank = address >= _bankSize ? _bankSize : 0;
  size_t position = address % ArdFSDetail::Sector + offset;
  auto* out = static_cast<uint8_t*>(bytes);
  while (n) {
    size_t node = framedNode(position);
    size_t within = position - node * (ArdFSDetail::Sector - 4);
    if (node >= count || sectors[node] >= _bankSize / ArdFSDetail::Sector) return false;
    uint32_t target = bank + sectors[node] * ArdFSDetail::Sector + within;
    size_t maximum = 256;
#if defined(ESP8266)
    maximum -= target & 3U;
#endif
    size_t take = std::min<size_t>(n, std::min<size_t>(maximum, ArdFSDetail::Sector - within));
    if (!readPhysical(target, out, take)) return false;
    out += take;
    position += take;
    n -= take;
  }
  return true;
}
inline bool ArdFSVolume::readEntry(const Entry& e, size_t offset, void* bytes, size_t n) {
  return readFragments(e.address, e.sectors, e.count, offset, bytes, n);
}
// Shared bounded CRC progress for mounting, copying and final read-back.
inline int ArdFSVolume::readEntryChunk(const Entry& entry, uint32_t& at, uint32_t& sum, uint8_t* bytes) {
  size_t n = std::min<size_t>(256, entry.length - at);
  if (!n) return 0;
  if (!readEntry(entry, at, bytes, n)) {
    _error = -5;
    return -1;
  }
  sum = crc(bytes, n, sum);
  at += n;
  return n;
}
inline bool ArdFSVolume::sourceChecksum(const Entry& e, uint8_t* header, uint32_t& checksum) {
  uint32_t at = e.address - e.dataOffset;
  bool ok = readPhysical(at, header, ArdFSDetail::Header);
  if (!ok || ArdFSDetail::getWord(header) != ArdFSDetail::RecordMagic || ArdFSDetail::getWord(header + 24) != ArdFSDetail::Commit || ArdFSDetail::getWord(header + 8) != e.pathLength || ArdFSDetail::getWord(header + 12) != e.length) {
    _error = -5;
    return false;
  }
  uint32_t prefix = ArdFSDetail::getWord(header + 28);
  checksum = ArdFSDetail::getWord(header + 16) ^ ArdCRC::combine(prefix, 0, e.length);
  return true;
}
// Rename and collection share the same checked, bounded payload copy.
inline int ArdFSVolume::copyPayload(ArdFSFile::Writer& h, const Entry& source) {
  int status = writeBufferedStep(h);
  if (status != 1) return status;
  int n = readEntryChunk(source, h.length, h.checksum, h.bytes);
  if (n < 0) return -1;
  if (n) {
    h.pendingAt = 0;
    h.pendingSize = n;
    return 0;
  }
  if (~h.checksum != h.verified) {
    _error = -84;
    return -1;
  }
  return 1;
}
inline uint32_t ArdFSVolume::collectionSectors(const ArdFSFile::Writer& h) const {
  uint32_t needed = h.mutationSource ? 0 : fragmentCount(h.pathLength, h.capacity);
  for (const Entry* e = _entries.head; e; e = e->next) {
    const char* target = h.copyPath(e->path);
    if (target) needed += fragmentCount(strlen(target), e->length);
  }
  return needed;
}
inline int ArdFSVolume::prepareFragment(ArdFSFile::Writer& h) {
  if (!h.prepareStage) {
    h.bank = _active;
    _staged.clear();
    _transaction = &h;
    const Entry* source = h.mutationSource ? _entries.find(h.mutationSource) : nullptr;
    if (source && h.pathLength) {
      h.capacity = source->length;
      h.expected = source->length;
    }
    uint32_t needed = source && !h.pathLength ? 0 : fragmentCount(h.pathLength, h.capacity);
    const auto& active = _maps[_active != 0];
    uint32_t metadata = active.rotate || active.slot == UINT32_MAX || active.slot % 32 == 30 || active.meta[0] == UINT16_MAX || active.meta[1] == UINT16_MAX ? 1 : 0;
    h.compacting = needed + metadata > freeSectors(_active);
    uint32_t other = _active ? 0 : _bankSize;
    const auto& target = _maps[other != 0];
    // Plan collection once for either required recovery or optional maintenance.
    // An oversized optional plan must still allow a write in the live bank.
    const uint32_t sectors = _bankSize / ArdFSDetail::Sector - 2;
    if (h.compacting || (needed && active.erases >= 8U * sectors && !_readers[other != 0])) {
      needed = collectionSectors(h);
      h.compacting = h.compacting || needed <= dataSectors(target);
    }
    if (h.compacting) h.bank = other;
    if (h.compacting && _readers[h.bank != 0]) {
      _error = -16;
      return -1;
    }
    h.reusingBank = h.compacting && int32_t(_maps[h.bank != 0].generation - _generation) > 0;
    _workBank = h.bank;
    _workMap = &_maps[h.bank != 0];
    _workMap->generation = _generation + (h.compacting ? 1 : 0);
    if (h.compacting) {
      _workMap->slot = UINT32_MAX;
      _workMap->revision = 0;
      _workMap->rotate = false;
      _workMap->erases = 0;
      if (source) {
        h.expected = 0;
        h.capacity = 0;
      }
      if (needed > dataSectors(*_workMap)) {
        _error = -28;
        return -1;
      }
      if (!initializeUses(h.bank)) {
        _error = -12;
        return -1;
      }
      h.workAt = 0;
      h.prepareStage = 1;
    } else h.prepareStage = 8;
    return 0;
  }
  if (h.prepareStage == 8) {
    if (h.mutationSource && (h.compacting || !h.pathLength)) {
      h.prepareStage = 10;
      return 1;
    }
    uint32_t slots = fragmentCount(h.pathLength, h.capacity);
    if (!slots || slots > (ArdFSDetail::Sector - ArdFSDetail::Header - h.pathLength - 2) / 2) {
      _error = -28;
      return -1;
    }
    auto* entry = Entry::create(this, h.pathLength, slots);
    if (!entry) {
      _error = -12;
      return -1;
    }
    memcpy(entry->path, h.path, h.pathLength);
    entry->dataOffset = ArdFSDetail::Header + h.pathLength + slots * 2;
    entry->address = h.bank;
    if (!_staged.insert(entry)) {
      _error = -28;
      return -1;
    }
    h.entry = entry;
    h.allocationLeft = _bankSize / ArdFSDetail::Sector;
    h.extent = slots * ArdFSDetail::Sector;
    h.prepareStage = 9;
    return 0;
  }
  if (h.prepareStage == 9) {
    uint16_t id;
    int status = allocateFragment(h, id);
    if (status != 1) return status;
    h.entry->sectors[0] = id;
    h.entry->count = 1;
    h.entry->address = h.bank + id * ArdFSDetail::Sector + h.entry->dataOffset;
    startSectorBuffer(h);
    h.prepareStage = h.mutationSource && !h.compacting ? 11 : 10;
    return 0;
  }
  if (h.prepareStage == 11) {
    const Entry* source = _entries.find(h.mutationSource);
    if (!source || !sourceChecksum(*source, h.bytes, h.verified)) return -1;
    h.prepareStage = 12;
    return 0;
  }
  if (h.prepareStage == 12) {
    const Entry* source = _entries.find(h.mutationSource);
    if (!source) return -1;
    int status = copyPayload(h, *source);
    if (status == 1) h.prepareStage = 10;
    return status;
  }
  return h.prepareStage == 10 ? 1 : -1;
}
inline uint32_t ArdFSVolume::fragmentPrefix(const uint8_t* header, uint32_t generation) {
  alignas(4) uint8_t word[4];
  ArdFSDetail::putWord(word, generation);
  uint32_t c = crc(header, 16);
  c = crc(word, 4, c);
  return crc(header + 20, 4, c);
}
inline void ArdFSVolume::buildFragmentHeader(ArdFSFile::Writer& h) {
  auto* bytes = h.bytes;
  auto& e = *h.entry;
  e.length = h.length;
  uint32_t revision = _workMap->revision + 1;
  makeHeader(bytes, ArdFSDetail::RecordMagic, e.count * ArdFSDetail::Sector | (revision & 4095U), h.pathLength, h.length);
  ArdFSDetail::putWord(bytes + 20, e.dataOffset | (revision & ~4095U));
  uint32_t c = fragmentPrefix(bytes, _workMap->generation);
  c = crc(h.path, h.pathLength, c);
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  c = crc(e.sectors, e.count * sizeof(uint16_t), c);
#else
  for (unsigned i = 0; i < e.count; ++i) {
    uint8_t word[2];
    ArdFSDetail::put16(word, e.sectors[i]);
    c = crc(word, 2, c);
  }
#endif
  ArdFSDetail::putWord(bytes + 28, ~c);
  ArdFSDetail::putWord(bytes + 16, ArdCRC::combine(~c, ~h.checksum, h.length));
}
__attribute__((noinline)) inline void ArdFSVolume::overlayFragmentHeader(ArdFSFile::Writer& h, uint8_t* bytes, size_t offset, size_t length) {
#if defined(ESP32) && defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  // Range copies are smaller on ESP32; the byte loop below is smaller on ESP8266.
  // Metadata is three byte ranges; copy their intersections with this page.
  const void* sources[] = { h.bytes, h.path, h.entry->sectors };
  size_t lengths[] = { ArdFSDetail::Header, h.pathLength, h.entry->count * 2U };
  for (unsigned i = 0; i < 3 && length; ++i) {
    size_t skip = std::min(offset, lengths[i]);
    size_t n = std::min(length, lengths[i] - skip);
    if (n) {
      memcpy(bytes, static_cast<const uint8_t*>(sources[i]) + skip, n);
      bytes += n;
      length -= n;
    }
    offset -= skip;
  }
#else
  size_t end = std::min<size_t>(offset + length, ArdFSDetail::Header + h.pathLength + h.entry->count * 2U);
  for (size_t i = offset; i < end; ++i) {
    if (i < ArdFSDetail::Header) bytes[i - offset] = h.bytes[i];
    else if (i < ArdFSDetail::Header + h.pathLength) bytes[i - offset] = h.path[i - ArdFSDetail::Header];
    else {
      size_t at = i - ArdFSDetail::Header - h.pathLength;
      bytes[i - offset] = uint8_t(h.entry->sectors[at / 2] >> ((at % 2) * 8));
    }
  }
#endif
}
inline int ArdFSVolume::finishFragment(ArdFSFile::Writer& h) {
  switch (h.finishStage) {
    case 0:
      if (!h.sectorDone) {
        int status = writeBufferedStep(h, true);
        return status < 0 ? -1 : 0;
      }
      if (h.extent / ArdFSDetail::Sector > h.entry->count) {
        auto* compact = Entry::create(this, h.pathLength, h.entry->count);
        if (!compact) {
          _error = -12;
          return -1;
        }
        memcpy(compact->path, h.path, h.pathLength);
        compact->count = h.entry->count;
        compact->dataOffset = h.entry->dataOffset;
        compact->address = h.entry->address;
        memcpy(compact->sectors, h.entry->sectors, compact->count * sizeof(uint16_t));
        h.entry->count = 0;  // Transfer the sector references without releasing them.
        _staged.insert(compact);
        h.entry = compact;
        h.extent = compact->count * ArdFSDetail::Sector;
      }
      h.finishStage = 1;
      return 0;
    case 1:
      buildFragmentHeader(h);
      h.node = 0;
      h.sectorAddress = h.entry->address & ~4095U;
      h.sectorUsed = h.entry->count == 1 ? h.entry->dataOffset + h.length : ArdFSDetail::Sector;
      h.sectorAt = 0;
      h.sectorStage = 9;
      h.sectorFinal = true;
      h.sectorDone = false;
      h.publishingRoot = true;
      h.sectorRetries = 0;
      h.finishStage = 2;
      return 0;
    case 2:
      {
        int status = drainSectorStep(h);
        if (status == 1) h.finishStage = 3;
        return status < 0 ? -1 : 0;
      }
    case 3:
      h.verified = UINT32_MAX;
      h.workAt = 0;
      h.finishStage = 4;
      return 0;
    case 4:
      {
        int n = readEntryChunk(*h.entry, h.workAt, h.verified, h.bytes);
        if (n < 0) return -1;
        if (n) return 0;
        if (h.verified != h.checksum) return -1;
        h.finishStage = 5;
        return 0;
      }
    default:
      {
        int status = commitStep(h.entry->address & ~4095U, h.bytes, h.finishStage);
        if (status >= 0) return status;
        h.sectorStage = 0;
        if (retrySector(h) < 0) return -1;
        h.sectorDone = false;
        h.finishStage = h.sectorStage == 3 ? 2 : 5;
        return 0;
      }
  }
}
inline void ArdFSVolume::buildCatalog(const ArdFSFile::Writer& h) {
  _catalogCount = 0;
  if (!h.compacting)
    for (const Entry* e = _entries.head; e; e = e->next) {
      if ((h.entry && !strcmp(e->path, h.path)) || (h.mutationSource && !strcmp(e->path, h.mutationSource))) continue;
      _catalog[_catalogCount++] = (e->address - h.bank) / ArdFSDetail::Sector;
    }
  for (const Entry* e = _staged.head; e; e = e->next)
    _catalog[_catalogCount++] = (e->address - h.bank) / ArdFSDetail::Sector;
}
inline void ArdFSVolume::publishFragments(ArdFSFile::Writer& h) {
  if (h.published) return;
  if (h.compacting) {
    _active = h.bank;
    _generation = _workMap->generation;
    _entries.clear();
  } else if (h.mutationSource) _entries.erase(h.mutationSource);
  h.mutationSource = nullptr;
  _entries.merge(_staged);
  _orphans.clear();
  h.published = true;
}
inline int ArdFSVolume::orphanRoot() {
  auto* e = Entry::create(this, 0, 1);
  if (!e) {
    _error = -12;
    return -1;
  }
  uint16_t id = _catalog[_catalogAt];
  if (_uses[_active != 0].get()[id]) {
    delete e;
    _error = -5;
    return -1;
  }
  e->address = _active + id * ArdFSDetail::Sector;
  e->sectors[0] = id;
  e->count = 1;
  _uses[_active != 0].get()[id] = 1;
  e->next = _orphans.head;
  _orphans.head = e;
  ++_orphans.count;
  _scanEntry.reset();
  ++_catalogAt;
  _mountStage = 10;
  return 0;
}
inline int ArdFSVolume::mountFragments(uint8_t* bytes) {
  switch (_mountStage) {
    case 9:
      {
        auto& map = _maps[_active != 0];
        if (!readPhysical(_active + map.slot * 128U, bytes, 256)) {
          _error = -5;
          return -1;
        }
        if (ArdFSDetail::getWord(bytes + 4) != 4U || ArdFSDetail::getWord(bytes + 8) != _generation || ArdFSDetail::getWord(bytes + 20) != map.revision || decodeMap(bytes, map, _active, map.slot) <= 0) {
          _error = -5;
          return -1;
        }
        _catalogCount = bytes[34];
        _catalogAt = 0;
        for (unsigned i = 0; i < _catalogCount; ++i) _catalog[i] = ArdFSDetail::get16(bytes + 100 + i * 2);
        if (!initializeUses(_active)) {
          _error = -12;
          return -1;
        }
        _mountStage = 10;
        return 0;
      }
    case 10:
      {
        if (_catalogAt == _catalogCount) {
          _scanEntry.reset();
          _mounted = true;
          _error = 0;
          return 1;
        }
        uint32_t root = _active + _catalog[_catalogAt] * ArdFSDetail::Sector;
        if (!readPhysical(root, bytes, ArdFSDetail::Header)) {
          _error = -5;
          return -1;
        }
        _scanExtent = ArdFSDetail::getWord(bytes + 4) & ~4095U;
        _scanName = ArdFSDetail::getWord(bytes + 8);
        _scanLength = ArdFSDetail::getWord(bytes + 12);
        _scanRootRevision = (ArdFSDetail::getWord(bytes + 4) & 4095U) | (ArdFSDetail::getWord(bytes + 20) & ~4095U);
        uint32_t offset = ArdFSDetail::getWord(bytes + 20) & 4095U, count = _scanExtent / ArdFSDetail::Sector;
        if (ArdFSDetail::getWord(bytes) != ArdFSDetail::RecordMagic || ArdFSDetail::getWord(bytes + 24) != ArdFSDetail::Commit || !_scanName || _scanName > 72 || !count || _scanExtent % ArdFSDetail::Sector || offset < ArdFSDetail::Header + _scanName + count * 2 || offset > ArdFSDetail::Sector - 2 || (_scanExtent < offset || _scanLength > _scanExtent - offset - ((count - 1) * 4)) || (framedNode(_scanLength + offset - 1) + 1) != count) return orphanRoot();
        _scanEntry.reset(Entry::create(this, _scanName, count));
        if (!_scanEntry) {
          _error = -12;
          return -1;
        }
        _scanEntry->dataOffset = offset;
        _scanEntry->address = root + offset;
        _scanEntry->length = _scanLength;
        _scanEntry->count = count;
        _scanCrc = fragmentPrefix(bytes, _generation);
        _scanExpected = ArdFSDetail::getWord(bytes + 16);
        _scanMetaExpected = ArdFSDetail::getWord(bytes + 28);
        _scanAt = 0;
        _mountStage = 11;
        return 0;
      }
    case 11:
      {
        auto& e = *_scanEntry;
        size_t total = _scanName + e.count * 2;
        if (_scanAt < total) {
          size_t n = std::min<size_t>(256, total - _scanAt);
          uint32_t address = _active + _catalog[_catalogAt] * ArdFSDetail::Sector + ArdFSDetail::Header + _scanAt;
          size_t maximum = 256;
#if defined(ESP8266)
          maximum -= address & 3U;
#endif
          n = std::min(n, maximum);
          if (!readPhysical(address, bytes, n)) {
            _error = -5;
            return -1;
          }
          _scanCrc = crc(bytes, n, _scanCrc);
          size_t names = _scanAt < _scanName ? std::min<size_t>(n, _scanName - _scanAt) : 0;
          if (names) memcpy(e.path + _scanAt, bytes, names);
          if (n > names) memcpy(reinterpret_cast<uint8_t*>(e.sectors) + _scanAt + names - _scanName, bytes + names, n - names);
          _scanAt += n;
          return 0;
        }
        if (~_scanCrc != _scanMetaExpected || strlen(e.path) != _scanName) return orphanRoot();
        if (int32_t(_scanRootRevision - _maps[_active != 0].revision) > 0) {
          _error = -5;
          return -1;
        }
#if !defined(__BYTE_ORDER__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
        for (unsigned i = 0; i < e.count; ++i) {
          const auto* raw = reinterpret_cast<const uint8_t*>(e.sectors) + i * 2;
          e.sectors[i] = ArdFSDetail::get16(raw);
        }
#endif
        if (e.sectors[0] != _catalog[_catalogAt]) return orphanRoot();
        _scanAt = 0;
        _mountStage = 12;
        return 0;
      }
    case 12:
      {
        auto& e = *_scanEntry;
        auto* uses = _uses[_active != 0].get();
        if (_scanAt < e.count) {
          uint16_t id = e.sectors[_scanAt];
          if (id >= _bankSize / 4096 || uses[id]) {
            _error = -5;
            return -1;
          }
          if (_scanAt) {
            if (!readPhysical(_active + id * 4096U, bytes, 4) || ArdFSDetail::getWord(bytes) != ArdFSDetail::DataMagic) {
              _error = -5;
              return -1;
            }
          }
          uses[id] = 1;
          ++_scanAt;
        }
        if (_scanAt == e.count) {
          _scanAt = 0;
          _mountStage = 13;
        }
        return 0;
      }
    default:
      {
        auto& e = *_scanEntry;
        int n = readEntryChunk(e, _scanAt, _scanCrc, bytes);
        if (n < 0) return -1;
        if (n) return 0;
        if (~_scanCrc == _scanExpected) {
          if (_entries.find(e.path) || !_entries.insert(_scanEntry.release())) {
            _error = -5;
            return -1;
          }
        } else {
          auto* bad = _scanEntry.release();
          bad->next = _orphans.head;
          _orphans.head = bad;
          ++_orphans.count;
        }
        ++_catalogAt;
        _mountStage = 10;
        return 0;
      }
  }
}
