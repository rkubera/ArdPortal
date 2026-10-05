// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#include "ArdFS.h"
#define ARDUI_DEFINE_STORAGE_MESSAGES
#include "Language.h"
#undef ARDUI_DEFINE_STORAGE_MESSAGES

namespace {
ArdJSON::Limits journalLimits() {
  ArdJSON::Limits limits; limits.maxInputBytes = 20000; limits.maxOutputBytes = 20000;
  limits.maxStringBytes = ArdFS::MaxBytes; limits.maxDepth = 1; limits.maxNodes = 5;
  return limits;
}
}
void ArdFS::begin() {
  if (_mountDone || _phase != Phase::Idle) return;
  _phase = Phase::Mount;
}
bool ArdFS::start(const String& path, Callback callback) {
  if (!_mountDone || !_mounted || busy() || !callback || path.length() < 2 ||
      path.length() > 64 || path[0] != '/' || path.indexOf("..") >= 0) return false;
  _path = path; _callback = callback; _slot = 0; _best = -1;
  _buffer = String(); _bestData = String(); _input = String();
  for (int i = 0; i < 2; ++i) { _valid[i] = _present[i] = _match[i] = false; _generation[i] = 0; }
  _phase = Phase::OpenRead; return true;
}
bool ArdFS::read(const String& path, Callback callback) {
  if (!start(path, callback)) return false;
  _writing = false; return true;
}
bool ArdFS::write(const String& path, const String& json, Callback callback) {
  if (!json.length() || json.length() > MaxBytes || !start(path, callback)) return false;
  _writing = true; _input = json; return true;
}
bool ArdFS::format(Callback callback) {
  if (!_mountDone || busy() || !callback) return false;
#if defined(ESP8266)
  if (ESP.getFlashChipSize() > ESP.getFlashChipRealSize()) return false;
#endif
  _callback = callback; _formatRequested = true; _mountDone = false; _phase = Phase::End; return true;
}
uint32_t ArdFS::checksum(uint32_t generation, const String& data) {
  ArdCooperativeBudget budget;uint32_t crc = UINT32_MAX;
  for (size_t i = 0; i < data.length() + 4; ++i) {
    if((i&255)==0)budget.checkpoint();
    uint8_t byte = i < 4 ? uint8_t(generation >> (i * 8)) : uint8_t(data[i - 4]);
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1)));
  }
  return ~crc;
}
void ArdFS::finish(bool ok, const String& error, bool changed) {
  Result result; result.ok = ok; result.error = error; result.changed = changed;
  result.found = _best >= 0;
  result.recovered = result.found && ((_present[0] && !_valid[0]) || (_present[1] && !_valid[1]));
  if (ok && !_writing && !_formatRequested) result.data = std::move(_bestData);
  _error = error; _buffer = String(); _input = String(); _bestData = String(); _phase = Phase::Idle;
  _formatRequested = false;
  Callback callback = std::move(_callback); _callback = nullptr;
  if (callback) callback(result);
}
void ArdFS::loop() {
  using ArdJSON::JSON; using ArdJSON::JSONVar;
  uint8_t chunk[256];
  switch (_phase) {
    case Phase::Mount:
#if defined(ESP8266)
      if (ESP.getFlashChipSize() > ESP.getFlashChipRealSize()) {
        _mountDone = true; finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_194)); return;
      }
      _mounted = _volume.begin();
#else
      _mounted = _volume.begin();
#endif
      if (_mounted) { _mountDone = true; _phase = Phase::Idle; }
      else _phase = Phase::End;
      return;
    case Phase::End: _volume.end(); _mounted = false; _phase = Phase::Format; return;
    case Phase::Format:
      if (!_volume.format()) { _mountDone = true; finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_195)); return; }
      _phase = Phase::Remount; return;
    case Phase::Remount:
#if defined(ESP32)
      _mounted = _volume.begin();
#else
      _mounted = _volume.begin();
#endif
      _mountDone = true;
      if (_formatRequested) finish(_mounted, _mounted ? "" : ArdUILanguage::storageText(ArdUILanguage::Key::s_196), _mounted);
      else { _phase = Phase::Idle; if (!_mounted) _error = ArdUILanguage::storageText(ArdUILanguage::Key::s_196); }
      return;
    case Phase::Idle: return;
    case Phase::OpenRead:
      _present[_slot] = _volume.exists(slotPath(_slot).c_str());
      if (_volume.lastError() && _volume.lastError() != -2) { finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_199)); return; }
      if (!_present[_slot]) { _phase = Phase::Validate; return; }
      _file = _volume.open(slotPath(_slot).c_str(), "r");
      if (!_file) { finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_199)); return; }
      _expected = _file.size(); _offset = 0;
      if (!_expected || _expected > 20000) { _file.close(); _phase = Phase::Validate; return; }
      if (!_buffer.reserve(_expected)) { _file.close(); finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_198)); return; }
      _phase = Phase::Read; return;
    case Phase::Read: {
      size_t count = _expected - _offset; if (count > sizeof(chunk)) count = sizeof(chunk);
      if (!count) { _file.close(); _phase = Phase::Validate; return; }
      if (_file.read(chunk, count) != int(count)) { _file.close(); finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_199)); return; }
      for (size_t i = 0; i < count; ++i) _buffer += char(chunk[i]);
      _offset += count; return;
    }
    case Phase::Validate: {
      if (_buffer.length()) {
        const JSONVar root = _json.parse(_buffer, nullptr, journalLimits());
        int64_t version = 0, generation = 0, crc = 0;
        if (root.isValid() && root.type() == JSONVar::Type::Object && root.length() == 4 &&
            root["journal"].toInteger(version) && version == 1 &&
            root["generation"].toInteger(generation) && generation >= 0 && uint64_t(generation) <= UINT32_MAX &&
            root["crc32"].toInteger(crc) && crc >= 0 && uint64_t(crc) <= UINT32_MAX &&
            root["data"].type() == JSONVar::Type::String) {
          String data = root["data"].asString();
          if (data.length() <= MaxBytes && checksum(uint32_t(generation), data) == uint32_t(crc)) {
            _valid[_slot] = true; _generation[_slot] = uint32_t(generation); _match[_slot] = data == _input;
            if (_best < 0 || int32_t(_generation[_slot] - _generation[_best]) > 0) {
              _best = _slot; if (!_writing) _bestData = data;
            }
          }
        }
        // Import the existing text JSON configuration once; never binary files.
        else if (_slot == 0 && _buffer.length() <= MaxBytes) {
          const JSONVar legacy = _json.parse(_buffer);
          if (legacy.isValid() && legacy.type() == JSONVar::Type::Object && !legacy.hasOwnProperty("journal")) {
            _valid[0] = true; _generation[0] = 0; _match[0] = _buffer == _input;
            if (_best < 0) { _best = 0; if (!_writing) _bestData = _buffer; }
          }
        }
      }
      _buffer = "";
      if (++_slot < 2) _phase = Phase::OpenRead;
      else if (_writing) _phase = Phase::Prepare;
      else if (_best >= 0 || (!_present[0] && !_present[1])) finish(true);
      else finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_200));
      return;
    }
    case Phase::Prepare: {
      if (_best >= 0 && _match[_best]) { ++_skipped; finish(true); return; }
      if (!_json.parse(_input).isValid()) { finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_200)); return; }
      uint32_t generation = _best >= 0 ? _generation[_best] + 1 : 1;
      JSONVar record = JSONVar::object(); record["journal"] = 1; record["generation"] = generation;
      record["crc32"] = checksum(generation, _input); record["data"] = _input;
      _input = String();
      _buffer = _json.stringify(record, false, nullptr, journalLimits());
      if (!_buffer.length()) { finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_198)); return; }
      _slot = _best == 0 ? 1 : 0; _offset = 0; _phase = Phase::OpenWrite; return;
    }
    case Phase::OpenWrite:
      _file = _volume.open(slotPath(_slot).c_str(), "w");
      if (!_file) { finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_204)); return; }
      _phase = Phase::Write; return;
    case Phase::Write: {
      size_t count = _buffer.length() - _offset; if (count > sizeof(chunk)) count = sizeof(chunk);
      if (!count) { _phase = Phase::Flush; return; }
      if (_file.write(reinterpret_cast<const uint8_t*>(_buffer.c_str()) + _offset, count) != count) {
        _file.close(); finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_204)); return;
      }
      _offset += count; return;
    }
    case Phase::Flush:
      if (!_file.flush()) { _file.close(); finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_204)); return; }
      _phase = Phase::CloseWrite; return;
    case Phase::CloseWrite:
      if (!_file.close()) { finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_204)); return; }
      _phase = Phase::OpenVerify; return;
    case Phase::OpenVerify:
      _file = _volume.open(slotPath(_slot).c_str(), "r");
      if (!_file || _file.size() != _buffer.length()) { _file.close(); finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_204)); return; }
      _offset = 0; _phase = Phase::Verify; return;
    case Phase::Verify: {
      size_t count = _buffer.length() - _offset; if (count > sizeof(chunk)) count = sizeof(chunk);
      if (!count) { _phase = Phase::CloseVerify; return; }
      if (_file.read(chunk, count) != int(count) || memcmp(chunk, _buffer.c_str() + _offset, count)) {
        _file.close(); finish(false, ArdUILanguage::storageText(ArdUILanguage::Key::s_204)); return;
      }
      _offset += count; return;
    }
    case Phase::CloseVerify: _file.close(); ++_commits; finish(true, "", true); return;
  }
}
