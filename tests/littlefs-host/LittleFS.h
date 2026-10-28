#pragma once
#include "Arduino.h"
#include <climits>
#include <string>
namespace littlefs_test {
inline std::map<std::string, std::vector<uint8_t>> files;
inline int calls = 0, failAt = -1;
inline bool off = false, failMount = false, failFormat = false, autoFormat = true;
inline size_t maxTransfer = 0;
inline unsigned formats = 0;
inline bool io() {
  if (off) return false;
  if (++calls == failAt) { off = true; return false; }
  return true;
}
}
struct FSInfo { size_t totalBytes = 1024 * 1024, usedBytes = 0; };
struct LittleFSConfig { explicit LittleFSConfig(bool automatic) { littlefs_test::autoFormat = automatic; } };
namespace fs {
class File {
  std::vector<uint8_t>* _data = nullptr;
  std::string _path;
  size_t _at = 0, _index = 0;
  bool _directory = false, _error = false;
public:
  File() = default;
  explicit File(const std::string& path, bool directory = false) : _path(path), _directory(directory) {
    auto it = littlefs_test::files.find(path);
    if (it != littlefs_test::files.end()) _data = &it->second;
  }
  explicit operator bool() const { return _data || _directory; }
  size_t size() const { return _data ? _data->size() : 0; }
  size_t position() const { return _at; }
  bool seek(size_t at) { if (!_data || at > size() || !littlefs_test::io()) return false; _at = at; return true; }
  int read(uint8_t* bytes, size_t n) {
    littlefs_test::maxTransfer = std::max(littlefs_test::maxTransfer, n);
    if (!_data || !littlefs_test::io()) return -1;
    n = std::min(n, size() - _at); if (n) memcpy(bytes, _data->data() + _at, n); _at += n; return n;
  }
  size_t write(const uint8_t* bytes, size_t n) {
    littlefs_test::maxTransfer = std::max(littlefs_test::maxTransfer, n);
    if (!_data || !littlefs_test::io()) { _error = true; return 0; }
    _data->insert(_data->end(), bytes, bytes+n); _at += n; return n;
  }
  void flush() { if (!littlefs_test::io()) _error = true; }
  int getWriteError() const { return _error; }
  void close() { if (_data || _directory) littlefs_test::io(); _data = nullptr; _directory = false; }
  bool isDirectory() const { return _directory; }
  const char* path() const { return _path.c_str(); }
  File openNextFile() {
    if (!_directory || _index >= littlefs_test::files.size() || !littlefs_test::io()) return {};
    auto it = littlefs_test::files.begin(); std::advance(it, _index++); return File(it->first);
  }
};
class Dir {
  size_t _at = 0;
  std::string _name;
public:
  bool next() {
    if (_at >= littlefs_test::files.size() || !littlefs_test::io()) return false;
    auto it = littlefs_test::files.begin(); std::advance(it, _at++); _name = it->first; return true;
  }
  bool isFile() const { return true; }
  String fileName() const { return _name.c_str(); }
  size_t fileSize() const { return littlefs_test::files.at(_name).size(); }
};
}
struct LittleFSStub {
  bool begin(bool automatic = false) { littlefs_test::autoFormat = automatic; return littlefs_test::io() && !littlefs_test::failMount && !automatic; }
  void setConfig(const LittleFSConfig&) {}
  void end() {}
  bool format() {
    ++littlefs_test::formats;
    if (!littlefs_test::io() || littlefs_test::failFormat) return false;
    littlefs_test::files.clear(); return true;
  }
  size_t totalBytes() { return 1024 * 1024; }
  size_t usedBytes() { size_t n = 0; for (auto& p : littlefs_test::files) n += p.second.size(); return n; }
  bool info(FSInfo& value) { value.usedBytes = usedBytes(); return true; }
  bool exists(const char* path) { return littlefs_test::io() && littlefs_test::files.count(path); }
  bool remove(const char* path) { return littlefs_test::io() && littlefs_test::files.erase(path); }
  bool rename(const char* from, const char* to) {
    if (!littlefs_test::io() || !littlefs_test::files.count(from)) return false;
    littlefs_test::files[to] = std::move(littlefs_test::files[from]); littlefs_test::files.erase(from); return true;
  }
  fs::File open(const char* path, const char* mode = "r") {
    if (!littlefs_test::io()) return {};
    if (!strcmp(path, "/")) return fs::File(path, true);
    if (*mode == 'w') littlefs_test::files[path].clear();
    if (!littlefs_test::files.count(path)) return {};
    return fs::File(path);
  }
  fs::Dir openDir(const char*) { return {}; }
};
inline LittleFSStub LittleFS;
