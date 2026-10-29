// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <utility>

// Thin filesystem adapter for the shared cooperative storage engine.
// LittleFS core calls (mount, format, flush, rename) are synchronous. Payload
// transfers remain bounded by the engine; a temporary file protects the old copy.
template<class Volume, class FileHandle> class ArdStorage;
class ArdLittleFSVolume;
class ArdLittleFSFile {
public:
  ArdLittleFSFile() = default;
  ArdLittleFSFile(const ArdLittleFSFile&) = delete;
  ArdLittleFSFile(ArdLittleFSFile&& other) noexcept {
    *this = std::move(other);
  }
  ArdLittleFSFile& operator=(ArdLittleFSFile&& other) noexcept {
    if (this != &other) {
      close();
      _file = std::move(other._file);
      _path = std::move(other._path);
      _other = std::move(other._other);
      _expected = other._expected;
      _length = other._length;
      _mode = other._mode;
      _stage = other._stage;
      _open = other._open;
      _failed = other._failed;
      other._open = false;
    }
    return *this;
  }
  ~ArdLittleFSFile() {
    close();
  }
  explicit operator bool() const {
    return _open;
  }
  size_t size() const {
    return _open ? (_mode == Write ? _length : _file.size()) : 0;
  }
  size_t position() const {
    return _open ? (_mode == Write ? _length : _file.position()) : 0;
  }
  bool seek(size_t at) {
    return _open && _mode == Read && at <= size() && _file.seek(at);
  }
  int read(uint8_t* bytes, size_t count) {
    return _open && _mode == Read ? _file.read(bytes, count) : -1;
  }
  size_t write(const uint8_t* bytes, size_t count) {
    if (!_open || _mode != Write || _stage || _failed || (_expected != SIZE_MAX && count > _expected - _length)) return 0;
    size_t written = _file.write(bytes, count);
    _length += written;
    if (written != count) _failed = true;
    return written;
  }
  int writeStep() {
    return 1;
  }
  int prepareStep() {
    return _open ? 1 : -1;
  }
  int flushStep() {
    if (!_open || _failed) return -1;
    if (_mode == Read || _stage == 3) return 1;
    if (_mode != Write) {
      bool ok = _mode == Remove ? LittleFS.remove(_path.c_str()) : LittleFS.rename(_path.c_str(), _other.c_str());
      if (!ok) {
        _failed = true;
        return -1;
      }
      _stage = 3;
      return 1;
    }
    if (_expected != SIZE_MAX && _length != _expected) return -1;
    switch (_stage) {
      case 0:
        _file.flush();
        if (_file.getWriteError()) {
          _failed = true;
          return -1;
        }
        _stage = 1;
        return 0;
      case 1:
        _file.close();
        _stage = 2;
        return 0;
      default:
        if (!LittleFS.rename(_other.c_str(), _path.c_str())) {
          _failed = true;
          return -1;
        }
        _stage = 3;
        return 1;
    }
  }
  bool close() {
    _file.close();
    _open = false;
    return true;
  }
private:
  friend class ArdLittleFSVolume;
  friend class ArdStorage<ArdLittleFSVolume, ArdLittleFSFile>;
  size_t queueWrite(const uint8_t* bytes, size_t n) {
    return write(bytes, n);
  }
  enum Mode : uint8_t { Read,
                        Write,
                        Remove,
                        Rename };
  fs::File _file;
  String _path, _other;
  size_t _expected = SIZE_MAX, _length = 0;
  Mode _mode = Read;
  uint8_t _stage = 0;
  bool _open = false, _failed = false;
};

class ArdLittleFSVolume {
public:
  // No automatic format on an ambiguous mount error: preserve existing files.
  int beginStep() {
    if (_mounted) return 1;
#if defined(ESP8266)
    LittleFS.setConfig(LittleFSConfig(false));
    _mounted = LittleFS.begin();
#else
    _mounted = LittleFS.begin(false);
#endif
    _error = _mounted ? 0 : -5;
    return _mounted ? 1 : -1;
  }
  void end() {
    LittleFS.end();
    _mounted = false;
  }
  int formatStep() {
    bool ok = LittleFS.format();
    _error = ok ? 0 : -5;
    return ok ? 1 : -1;
  }
  int lastError() const {
    return _error;
  }
  bool exists(const char* path) {
    bool found = _mounted && path && LittleFS.exists(path);
    _error = found ? 0 : -2;
    return found;
  }
  // LittleFS owns capacity accounting, rather than reserving a second flash bank.
  size_t maxFileBytes(size_t) const {
    return totalBytes();
  }
  size_t totalBytes() const {
    if (!_mounted) return 0;
#if defined(ESP8266)
    FSInfo info;
    return LittleFS.info(info) ? info.totalBytes : 0;
#else
    return LittleFS.totalBytes();
#endif
  }
  size_t usedBytes() const {
    if (!_mounted) return 0;
#if defined(ESP8266)
    FSInfo info;
    return LittleFS.info(info) ? info.usedBytes : 0;
#else
    return LittleFS.usedBytes();
#endif
  }
  size_t freeBytes() const {
    size_t total = totalBytes(), used = usedBytes();
    return used <= total ? total - used : 0;
  }
  size_t fileCount() const {
    size_t count = 0;
    String path;
    size_t bytes;
    while (fileInfo(count, path, bytes)) ++count;
    return count;
  }
  // Root files, matching ArdPortal's flat /ardportal.json[.journal] storage.
  // Temporary files are internal and excluded from enumeration.
  bool fileInfo(size_t index, String& path, size_t& bytes) const {
    if (!_mounted) return false;
#if defined(ESP8266)
    auto directory = LittleFS.openDir("/");
    while (directory.next()) {
      if (!directory.isFile()) continue;
      String name = directory.fileName();
      if (temporary(name)) continue;
      if (index--) continue;
      path = name;
      bytes = directory.fileSize();
      return path == name;
    }
#else
    auto directory = LittleFS.open("/");
    for (auto file = directory.openNextFile(); file; file = directory.openNextFile()) {
      if (file.isDirectory()) continue;
      String name = file.path();
      if (temporary(name)) continue;
      if (index--) continue;
      path = name;
      bytes = file.size();
      return path == name;
    }
#endif
    return false;
  }
  ArdLittleFSFile open(const char* path, const char* mode, size_t length = SIZE_MAX, bool = false) {
    ArdLittleFSFile result;
    if (!_mounted || !path || !*path || !mode || (*mode != 'r' && *mode != 'w') || temporary(String(path))) {
      _error = -22;
      return result;
    }
    if (*mode == 'w') {
      result._mode = ArdLittleFSFile::Write;
      result._path = path;
      result._other = result._path + ".ardtmp";
      if (result._path != path || result._other.length() != result._path.length() + 7) {
        _error = -12;
        return result;
      }
      result._expected = length;
      result._file = LittleFS.open(result._other.c_str(), "w");
    } else result._file = LittleFS.open(path, "r");
    result._open = bool(result._file);
    _error = result._open ? 0 : -5;
    return result;
  }
  ArdLittleFSFile mutate(const char* source, const char* destination = nullptr) {
    ArdLittleFSFile result;
    if (!_mounted || !source || !*source || temporary(String(source)) || (destination && (!*destination || temporary(String(destination))))) {
      _error = -22;
      return result;
    }
    if (!exists(source)) return result;
    if (destination && exists(destination)) {
      _error = -17;
      return result;
    }
    result._path = source;
    if (destination) result._other = destination;
    if (result._path != source || (destination && result._other != destination)) {
      _error = -12;
      return result;
    }
    result._mode = destination ? ArdLittleFSFile::Rename : ArdLittleFSFile::Remove;
    result._open = true;
    _error = 0;
    return result;
  }
private:
  bool _mounted = false;
  int _error = 0;
  static bool temporary(const String& path) {
    return path.length() >= 7 && strcmp(path.c_str() + path.length() - 7, ".ardtmp") == 0;
  }
};
