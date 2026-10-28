// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "Language.h"
/**
 * @brief Initialize the component and prepare its runtime state.
 * @return No value.
 */
template<class Volume, class FileHandle>
void ArdStorage<Volume, FileHandle>::begin() {
  if (_mountDone || _phase != Phase::Idle) return;
  _phase = Phase::Mount;
}
/**
 * @brief Prepare a new operation after checking that the component is idle.
 * @param path Journal file path.
 * @param callback Completion or event handler supplied to this operation.
 * @return True if the operation was accepted; loop() later reports its result through callback. False if busy or input cannot be accepted.
 */
template<class Volume, class FileHandle>
bool ArdStorage<Volume, FileHandle>::start(const String& path, Callback&& callback) {
  if (!_mountDone || !_mounted || busy() || !callback || path.length() < 2 || path.length() > 64 || path[0] != '/' || path.indexOf("..") >= 0) return false;
  _rawFile = _sourceFailed = false;
  _path = path;
  if (_path != path) return false;
  _callback = std::move(callback);
  _slot = 0;
  _best = -1;
  _repairRequested = _repairing = false;
  _repairAttempts = 0;
  _repairFailed = _repairDone = false;
  _repairError = String();
  _reader.reset();
  _repairFile.close();
  for (int i = 0; i < 2; ++i) {
    _valid[i] = _present[i] = _match[i] = false;
    _generation[i] = 0;
    _legacy[i] = false;
    _length[i] = _physicalLength[i] = 0;
  }
  _phase = Phase::OpenRead;
  return true;
}
template<class Volume, class FileHandle>
bool ArdStorage<Volume, FileHandle>::writeJsonConfig(const String& path, size_t length, JsonSource source, Callback callback) {
  if (!length || length > _volume.maxFileBytes(path.length() + 8) || !source || !start(path, std::move(callback))) return false;
  _writing = true;
  _source = std::move(source);
  _sourceLength = length;
  _sourceAt = 0;
  return true;
}
template<class Volume, class FileHandle>
bool ArdStorage<Volume, FileHandle>::readJsonConfig(const String& path, FileSink sink, Callback callback, bool repair) {
  if (!sink || !start(path, std::move(callback))) return false;
  _writing = false;
  _repairRequested = repair;
  _sink = std::move(sink);
  return true;
}
template<class Volume, class FileHandle>
bool ArdStorage<Volume, FileHandle>::writeFile(const String& path, size_t length, FileSource source, Callback callback) {
  if (length == SIZE_MAX || !source || !start(path, std::move(callback))) return false;
  _writing = true;
  _rawFile = true;
  _source = std::move(source);
  _sourceLength = _expected = length;
  _sourceFailed = false;
  _slot = 0;
  _offset = 0;
  resetJournalOutput();
  _phase = Phase::OpenWrite;
  return true;
}
template<class Volume, class FileHandle>
bool ArdStorage<Volume, FileHandle>::readFile(const String& path, FileSink sink, Callback callback) {
  if (!sink || !start(path, std::move(callback))) return false;
  _writing = false;
  _rawFile = true;
  _sink = std::move(sink);
  _phase = Phase::OpenFileRead;
  return true;
}
template<class Volume, class FileHandle>
bool ArdStorage<Volume, FileHandle>::remove(const String& path, Callback callback) {
  return rename(path, String(), std::move(callback));
}
template<class Volume, class FileHandle>
bool ArdStorage<Volume, FileHandle>::rename(const String& source, const String& destination, Callback callback) {
  if (destination.length() && (destination.length() < 2 || destination.length() > 64 || destination[0] != '/' || destination.indexOf("..") >= 0)) return false;
  if (!start(source, std::move(callback))) return false;
  _file = _volume.mutate(source.c_str(), destination.length() ? destination.c_str() : nullptr);
  _phase = Phase::PrepareMutation;
  return true;
}
template<class Volume, class FileHandle>
void ArdStorage<Volume, FileHandle>::resetJournalOutput() {
  _journalStage = 0;
  _journalHeaderAt = 0;
  _sourceAt = 0;
  _rawAt = 0;
  _escapeAt = _escapeLength = 0;
  _raw = String();
}
template<class Volume, class FileHandle>
String ArdStorage<Volume, FileHandle>::sourcePart(size_t offset, size_t maximum) {
  size_t count = _sourceLength - offset;
  if (count > maximum) count = maximum;
  String part = _source(offset, count);
  if (part.length() != count) { _sourceFailed = true; return String(); }
  return part;
}
template<class Volume, class FileHandle>
bool ArdStorage<Volume, FileHandle>::openSlot(FileHandle& file, int slot, OpenMode mode, ArdUILanguageData::Key failure) {
  const String path = slotPath(slot);
  if (path.length() != _path.length() + (slot ? 8 : 0)) { fail(ArdUILanguage::Key::s_198); return false; }
  if (mode == OpenMode::Scan) {
    _present[slot] = _volume.exists(path.c_str());
    if (_volume.lastError() && _volume.lastError() != -2) { fail(failure); return false; }
    if (!_present[slot]) return true;
  }
  const bool writing = mode == OpenMode::Write;
  file = _volume.open(path.c_str(), writing ? "w" : "r", writing ? _expected : SIZE_MAX, true);
  if (!file) { fail(failure); return false; }
  return true;
}
template<class Volume, class FileHandle>
size_t ArdStorage<Volume, FileHandle>::journalOutput() {
  if (_rawFile) {
    if (_sourceAt == _sourceLength) return 0;
    String part = sourcePart(_sourceAt, 256);
    const size_t count = part.length();
    memcpy(_decoded, part.c_str(), count);
    _sourceAt += count;
    return count;
  }
  size_t output = 0;
  while (output < 256 && _journalStage < 3) {
    if (_journalStage == 0) {
      if (_journalHeaderAt < _journalHeader.length()) {
        _decoded[output++] = _journalHeader.c_str()[_journalHeaderAt++];
        continue;
      }
      _journalStage = 1;
    }
    if (_journalStage == 1) {
      if (_escapeAt < _escapeLength) {
        _decoded[output++] = _escape[_escapeAt++];
        continue;
      }
      if (_rawAt >= _raw.length()) {
        if (_sourceAt >= _sourceLength) {
          _journalStage = 2;
          _rawAt = 0;
          continue;
        }
        _raw = sourcePart(_sourceAt, 128);
        _rawAt = 0;
        if (_sourceFailed) return 0;
      }
      uint8_t c = _raw.c_str()[_rawAt++];
      ++_sourceAt;
      _escapeAt = 0;
      if (c == '"' || c == '\\') {
        _escape[0] = '\\';
        _escape[1] = c;
        _escapeLength = 2;
      } else if (c < 32) {
        static const char hex[] = "0123456789abcdef";
        _escape[0] = '\\';
        _escape[1] = 'u';
        _escape[2] = _escape[3] = '0';
        _escape[4] = hex[c >> 4];
        _escape[5] = hex[c & 15];
        _escapeLength = 6;
      } else {
        _escape[0] = c;
        _escapeLength = 1;
      }
    }
    if (_journalStage == 2) {
      _decoded[output++] = _rawAt++ == 0 ? '"' : '}';
      if (_rawAt >= 2) _journalStage = 3;
    }
  }
  return output;
}
/**
 * @brief Queue formatting of the storage volume; existing records will be discarded.
 * @param callback Completion or event handler supplied to this operation.
 * @return True if the operation was accepted; loop() later reports its result through callback. False if busy or input cannot be accepted.
 */
template<class Volume, class FileHandle>
bool ArdStorage<Volume, FileHandle>::format(Callback callback) {
  if (!_mountDone || busy() || !callback) return false;
#if defined(ESP8266)
  if (ESP.getFlashChipSize() > ESP.getFlashChipRealSize()) return false;
#endif
  _callback = std::move(callback);
  _formatRequested = true;
  _mountDone = false;
  _phase = Phase::End;
  return true;
}
/**
 * @brief Compute the checksum covering the journal generation and document bytes.
 * @param generation Journal generation number included in the checksum.
 * @param data Data buffer or value used by the operation.
 * @return The checksum covering the journal generation and document bytes.
 */
template<class Volume, class FileHandle>
uint32_t ArdStorage<Volume, FileHandle>::checksum(uint32_t generation, const String& data) {
  ArdCooperativeBudget budget;
  uint32_t crc = UINT32_MAX;
  for (size_t i = 0; i < data.length() + 4; ++i) {
    if ((i & 255) == 0) budget.checkpoint();
    uint8_t byte = i < 4 ? uint8_t(generation >> (i * 8)) : uint8_t(data[i - 4]);
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1)));
  }
  return ~crc;
}
/**
 * @brief Complete the operation and report its stored result.
 * @param ok Whether the completed operation succeeded.
 * @param error Output error text; populated when the operation fails.
 * @param changed Whether persistent contents changed during this operation.
 * @return No value.
 */
template<class Volume, class FileHandle>
void ArdStorage<Volume, FileHandle>::fail(ArdUILanguageData::Key message) {
  _file.close();
  finish(false, ArdUILanguage::storageText(message));
}
template<class Volume, class FileHandle>
void ArdStorage<Volume, FileHandle>::finish(bool ok, const String& error, bool changed) {
  const bool delivering = _phase == Phase::OpenPayload || _phase == Phase::ReadPayload || _phase == Phase::DeliverPayload;
  if (_repairing && !ok && !delivering) {
    _file.close(); _repairFile.close();
    if (_repairAttempts < 3) {
      ++_repairAttempts;
      _repairRetrySince = millis();
      _phase = Phase::RepairWait;
    } else {
      _repairFailed = true;
      _repairError = error;
      _phase = Phase::OpenPayload;
    }
    return;
  }
  Result result;
  result.ok = ok;
  result.error = error;
  result.changed = changed;
  result.found = _best >= 0;
  result.redundant = (_valid[0] && _valid[1]) || _repairDone;
  result.repaired = _repairDone;
  result.repairAttempts = _repairAttempts;
  result.recovered = _repairing || (result.found && ((_present[0] && !_valid[0]) || (_present[1] && !_valid[1])));
  _source = nullptr;
  _sink = nullptr;
  _sourceCompare = String();
  _rawSourceCompare = String();
  _reader.reset();
  _repairFile.close();
  _raw = String();
  _journalHeader = String();
  _error = error;
  _phase = Phase::Idle;
  _formatRequested = false;
  _repairRequested = _repairing = false;
  Callback callback = std::move(_callback);
  _callback = nullptr;
  if (callback) callback(result);
}
/**
 * @brief Advance the component work; call repeatedly from the Arduino main loop.
 * @return No value.
 */
template<class Volume, class FileHandle>
void ArdStorage<Volume, FileHandle>::loop() {
  uint8_t chunk[256];
  switch (_phase) {
    case Phase::Mount:
    case Phase::Remount:
      {
        int status = _volume.beginStep();
        if (!status) return;
        const bool remount = _phase == Phase::Remount;
        _mounted = status == 1;
        if (_mounted) {
          _mountDone = true;
          if (_formatRequested) finish(true, "", true);
          else _phase = Phase::Idle;
        } else if (!remount && _volume.lastError() == -84) _phase = Phase::End;
        else {
          _mountDone = true;
          fail(_volume.lastError() == -12 ? ArdUILanguage::Key::s_198 : ArdUILanguage::Key::s_199);
        }
        return;
      }
    case Phase::End:
      _volume.end(); _mounted = false; _phase = Phase::Format; return;
    case Phase::Format:
      {
        int status = _volume.formatStep();
        if (!status) return;
        if (status < 0) { _mountDone = true; fail(ArdUILanguage::Key::s_195); }
        else _phase = Phase::Remount;
        return;
      }
    case Phase::Idle: return;
    case Phase::PrepareMutation:
    case Phase::FlushMutation:
      {
        const bool preparing = _phase == Phase::PrepareMutation;
        int status = preparing ? _file.prepareStep() : _file.flushStep();
        if (status < 0) { fail(ArdUILanguage::Key::s_204); return; }
        if (status == 1) {
          if (preparing) { _best = 0; _phase = Phase::FlushMutation; }
          else { _file.close(); ++_commits; finish(true, "", true); }
        }
        return;
      }
    case Phase::OpenFileRead:
      if (!openSlot(_file, 0, OpenMode::Read, ArdUILanguage::Key::s_199)) return;
      _expected = _file.size();
      _offset = 0;
      _best = 0;
      _phase = Phase::ReadFile;
      return;
    case Phase::ReadFile:
      {
        size_t n = _expected - _offset;
        if (n > sizeof(chunk)) n = sizeof(chunk);
        if (!n) { _file.close(); finish(true); return; }
        if (_file.read(chunk, n) != int(n) || !_sink(_offset, chunk, n)) {
          fail(ArdUILanguage::Key::s_199);
          return;
        }
        _offset += n;
        return;
      }
    case Phase::OpenRead:
      {
        _reader.reset();
        _sourceMatch = _legacyMatch = true;
        _sourceFailed = false;
        _sourceCompare = _rawSourceCompare = String();
        _sourceCompareAt = _rawCompareAt = 0;
        _runningFileCrc = UINT32_MAX;
        if (!openSlot(_file, _slot, OpenMode::Scan, ArdUILanguage::Key::s_199)) return;
        if (!_present[_slot]) { _phase = Phase::Validate; return; }
        _expected = _physicalLength[_slot] = _file.size();
        _offset = 0;
        _phase = Phase::Read;
        return;
      }
    case Phase::Read:
      {
        size_t count = _expected - _offset;
        if (count > sizeof(chunk)) count = sizeof(chunk);
        if (!count) { _file.close(); _phase = Phase::Validate; return; }
        if (_file.read(chunk, count) != int(count)) { fail(ArdUILanguage::Key::s_199); return; }
        auto compare = [&](uint8_t b, size_t at, bool& match, String& cached, size_t& cachedAt) {
          if (!_writing || !match) return true;
          if (at >= _sourceLength) { match = false; return true; }
          if (!cached.length() || at < cachedAt || at - cachedAt >= cached.length()) {
            cachedAt = at;
            cached = sourcePart(at, 256);
            if (_sourceFailed) return false;
          }
          if (b != uint8_t(cached[at - cachedAt])) match = false;
          return true;
        };
        auto decoded = [&](uint8_t b, size_t at) { return compare(b, at, _sourceMatch, _sourceCompare, _sourceCompareAt); };
        for (size_t i = 0; i < count; ++i) {
          _runningFileCrc = ArdJournalStream::update(_runningFileCrc, chunk[i]);
          compare(chunk[i], _offset + i, _legacyMatch, _rawSourceCompare, _rawCompareAt);
          _reader.input(chunk[i], decoded);
        }
        if (_sourceFailed || _reader.json.outOfMemory) {
          fail(ArdUILanguage::Key::s_198); return;
        }
        _offset += count;
        return;
      }
    case Phase::Validate:
      {
        if (_present[_slot] && _reader.finish()) {
          _valid[_slot] = true;
          _legacy[_slot] = _reader.legacy;
          _generation[_slot] = _reader.legacy ? 0 : _reader.generation;
          _length[_slot] = _reader.legacy ? _physicalLength[_slot] : _reader.length;
          _fileCrc[_slot] = _runningFileCrc;
          _match[_slot] = (_reader.legacy ? _legacyMatch : _sourceMatch) && _length[_slot] == _sourceLength;
          if (_best < 0 || int32_t(_generation[_slot] - _generation[_best]) > 0) _best = _slot;
        }
        _sourceCompare = _rawSourceCompare = String();
        _reader.reset();
        if (++_slot < 2) _phase = Phase::OpenRead;
        else if (_writing) _phase = Phase::Prepare;
        else if (_best < 0) {
          if (!_present[0] && !_present[1]) finish(true);
          else fail(ArdUILanguage::Key::s_200);
        }
        else if (_repairRequested && (!_valid[0] || !_valid[1])) {
          _repairing = true; _repairAttempts = 1;
          _slot = 1 - _best;
          _expected = _physicalLength[_best]; _offset = 0;
          _phase = Phase::OpenWrite;
        } else _phase = Phase::OpenPayload;
        return;
      }
    case Phase::OpenPayload:
      _reader.reset();
      if (!openSlot(_file, _best, OpenMode::Read, ArdUILanguage::Key::s_199)) return;
      if (_file.size() != _physicalLength[_best]) {
        fail(ArdUILanguage::Key::s_199); return;
      }
      _offset = _payloadAt = _decodedAt = _decodedSize = 0;
      _runningFileCrc = UINT32_MAX;
      _phase = Phase::ReadPayload;
      return;
    case Phase::ReadPayload:
      {
        size_t n = _physicalLength[_best] - _offset;
        if (n > sizeof(chunk)) n = sizeof(chunk);
        if (!n) {
          _file.close();
          if (_runningFileCrc != _fileCrc[_best] || (!_legacy[_best] && !_reader.finish()) || _payloadAt != _length[_best]) {
            fail(ArdUILanguage::Key::s_199); return;
          }
          finish(!_repairFailed, _repairFailed ? _repairError : String());
          return;
        }
        if (_file.read(chunk, n) != int(n)) { fail(ArdUILanguage::Key::s_199); return; }
        _decodedAt = _decodedSize = 0;
        auto emit = [&](uint8_t b, size_t) {
          if (_decodedSize == sizeof(_decoded)) return false;
          _decoded[_decodedSize++] = b; return true;
        };
        for (size_t i = 0; i < n; ++i) {
          _runningFileCrc = ArdJournalStream::update(_runningFileCrc, chunk[i]);
          if (_legacy[_best]) emit(chunk[i], _offset + i);
          else _reader.input(chunk[i], emit);
        }
        if (_reader.json.failed) {
          fail(_reader.json.outOfMemory ? ArdUILanguage::Key::s_198 : ArdUILanguage::Key::s_199); return;
        }
        _offset += n;
        if (_decodedSize) _phase = Phase::DeliverPayload;
        return;
      }
    case Phase::DeliverPayload:
      {
        size_t n = _decodedSize - _decodedAt;
        if (n > 256) n = 256;
        if (!_sink(_payloadAt, _decoded + _decodedAt, n)) {
          fail(ArdUILanguage::Key::s_198); return;
        }
        _decodedAt += n; _payloadAt += n;
        if (_decodedAt == _decodedSize) _phase = Phase::ReadPayload;
        return;
      }
    case Phase::RepairWait:
      if (int32_t(millis() - _repairRetrySince) < 1000) return;
      _slot = 1 - _best; _offset = 0; _expected = _physicalLength[_best];
      _phase = Phase::OpenWrite;
      return;
    case Phase::ReadRepair:
      {
        int pending = _file.writeStep();
        if (pending < 0) { fail(ArdUILanguage::Key::s_204); return; }
        if (!pending) return;
        if (!_repairFile) {
          if (!openSlot(_repairFile, _best, OpenMode::Read, ArdUILanguage::Key::s_199)) return;
          _runningFileCrc = UINT32_MAX;
          if (_repairFile.size() != _expected) { fail(ArdUILanguage::Key::s_199); return; }
        }
        size_t n = _expected - _offset;
        if (n > 256) n = 256;
        if (!n) {
          _repairFile.close();
          if (_runningFileCrc != _fileCrc[_best]) { fail(ArdUILanguage::Key::s_199); return; }
          _phase = Phase::Flush; return;
        }
        if (_repairFile.read(_decoded, n) != int(n)) { fail(ArdUILanguage::Key::s_199); return; }
        for (size_t i = 0; i < n; ++i) _runningFileCrc = ArdJournalStream::update(_runningFileCrc, _decoded[i]);
        _decodedSize = n;
        _phase = Phase::WriteRepair;
        return;
      }
    case Phase::WriteRepair:
      if (_file.queueWrite(_decoded, _decodedSize) != _decodedSize) { fail(ArdUILanguage::Key::s_204); return; }
      _offset += _decodedSize;
      _phase = Phase::ReadRepair;
      return;
    case Phase::Prepare:
      {
        _sourceGeneration = _best >= 0 ? _generation[_best] + 1 : 1;
        _sourceCrc = UINT32_MAX;
        for (uint8_t i = 0; i < 4; ++i) _sourceCrc = ArdJournalStream::update(_sourceCrc, uint8_t(_sourceGeneration >> (8 * i)));
        _sourceAt = 0;
        _escapedLength = 0;
        _reader.json.reset();
        _phase = Phase::SourceCrc;
        return;
      }
    case Phase::SourceCrc:
      {
        auto ignore = [](ArdJsonStream::Event, uint8_t, unsigned) { return true; };
        if (_sourceAt < _sourceLength) {
          String part = sourcePart(_sourceAt, 256);
          size_t n = part.length();
          if (_sourceFailed) {
            fail(ArdUILanguage::Key::s_198);
            return;
          }
          // Unchanged legacy data need not fit the escaped journal form.
          const size_t capacity = _best >= 0 && _match[_best] ? SIZE_MAX : _volume.maxFileBytes(_path.length() + 8);
          for (size_t i = 0; i < n; ++i) {
            uint8_t c = part.c_str()[i];
            _reader.json.input(c, ignore);
            _sourceCrc = ArdJournalStream::update(_sourceCrc, c);
            size_t escaped = c < 32 ? 6 : (c == '"' || c == '\\' ? 2 : 1);
            if (escaped > capacity || _escapedLength > capacity - escaped) {
              fail(ArdUILanguage::Key::s_204);
              return;
            }
            _escapedLength += escaped;
          }
          if (_reader.json.failed) {
            fail(_reader.json.outOfMemory ? ArdUILanguage::Key::s_198 : ArdUILanguage::Key::s_200); return;
          }
          _sourceAt += n;
          return;
        }
        if (!_reader.json.finish(ignore)) { fail(ArdUILanguage::Key::s_200); return; }
        if (_best >= 0 && _match[_best]) {
          ++_skipped;
          finish(true);
          return;
        }
        char header[128];
        int headerBytes = snprintf(header, sizeof(header), "{\"journal\":1,\"generation\":%lu,\"crc32\":%lu,\"data\":\"", (unsigned long)_sourceGeneration, (unsigned long)~_sourceCrc);
        _journalHeader = header;
        if (headerBytes <= 0 || size_t(headerBytes) >= sizeof(header) || _journalHeader.length() != size_t(headerBytes)) {
          fail(ArdUILanguage::Key::s_198);
          return;
        }
        const size_t capacity = _volume.maxFileBytes(_path.length() + 8);
        const size_t overhead = _journalHeader.length() + 2;
        if (overhead > capacity || _escapedLength > capacity - overhead) {
          fail(ArdUILanguage::Key::s_198);
          return;
        }
        _expected = overhead + _escapedLength;
        _slot = _best == 0 ? 1 : 0;
        _offset = 0;
        resetJournalOutput();
        _phase = Phase::OpenWrite;
        return;
      }
    case Phase::OpenWrite:
      {
        if (!openSlot(_file, _slot, OpenMode::Write, ArdUILanguage::Key::s_204)) return;
        _phase = Phase::PrepareWrite;
        return;
      }
    case Phase::PrepareWrite:
      {
        int status = _file.prepareStep();
        if (status < 0) {
          fail(ArdUILanguage::Key::s_204);
        } else if (status == 1) _phase = _repairing ? Phase::ReadRepair : Phase::Write;
        return;
      }
    case Phase::Write:
    case Phase::Verify:
      {
        const bool verify = _phase == Phase::Verify;
        if (!verify) {
          int pending = _file.writeStep();
          if (pending < 0) { fail(ArdUILanguage::Key::s_204); return; }
          if (!pending) return;
        }
        const size_t count = journalOutput();
        if (_sourceFailed) { fail(ArdUILanguage::Key::s_198); return; }
        if (!count) {
          if (_offset != _expected) { fail(verify ? ArdUILanguage::Key::s_204 : ArdUILanguage::Key::s_198); return; }
          _phase = verify ? Phase::CloseVerify : Phase::Flush;
          return;
        }
        if (verify ? (_file.read(chunk, count) != int(count) || memcmp(chunk, _decoded, count))
                   : _file.queueWrite(_decoded, count) != count) {
          fail(ArdUILanguage::Key::s_204); return;
        }
        _offset += count;
        return;
      }
    case Phase::Flush:
      {
        int status = _file.flushStep();
        if (status < 0) {
          fail(ArdUILanguage::Key::s_204);
        } else if (status == 1) _phase = Phase::CloseWrite;
        return;
      }
    case Phase::CloseWrite:
      if (!_file.close()) {
        fail(ArdUILanguage::Key::s_204);
        return;
      }
      if (_repairing) {
        _repairDone = true;
        ++_commits;
        _phase = Phase::OpenPayload;
      } else _phase = Phase::OpenVerify;
      return;
    case Phase::OpenVerify:
      {
        if (!openSlot(_file, _slot, OpenMode::Read, ArdUILanguage::Key::s_204)) return;
        if (_file.size() != _expected) { fail(ArdUILanguage::Key::s_204); return; }
        _offset = 0;
        resetJournalOutput();
        _phase = Phase::Verify;
        return;
      }
    case Phase::CloseVerify:
      _file.close();
      if (_rawFile) _best = 0;
      ++_commits;
      finish(true, "", true);
      return;
  }
}
