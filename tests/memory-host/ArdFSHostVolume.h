#pragma once
#include "Arduino.h"
extern std::map<std::string, std::vector<uint8_t>> fakeFiles;
extern bool failFileWrite, failRename;
extern bool failMount, failFormat;
inline size_t fakeWriteCalls = 0, fakeOpenWrites = 0, fakeMaxTransfer = 0;
struct ArdFSFile {
  std::vector<uint8_t>* data = nullptr; size_t offset = 0;
  operator bool() const { return data; }
  size_t size() const { return data ? data->size() : 0; }
  int available() const { return data && offset < data->size(); }
  int read(uint8_t* bytes, size_t count) { fakeMaxTransfer = std::max(fakeMaxTransfer, count); size_t size = std::min(count, data ? data->size() - offset : 0); if (size) memcpy(bytes, data->data() + offset, size); offset += size; return size; }
  int read() { return available() ? (*data)[offset++] : -1; }
  size_t write(const uint8_t* bytes, size_t length) { ++fakeWriteCalls; fakeMaxTransfer = std::max(fakeMaxTransfer, length); if (!data || failFileWrite) return 0; data->insert(data->end(), bytes, bytes + length); return length; }
  bool flush() { return !failFileWrite; }
  bool close() { return true; }
};
struct ArdFSVolume {
  int lastError() const { return 0; }
  bool begin() { return !failMount; }
  bool exists(const char* path) { return fakeFiles.count(path); }
  bool remove(const char* path) { return fakeFiles.erase(path); }
  void end() {}
  bool format() { if(failFormat) return false; fakeFiles.clear(); failMount = false; return true; }
  ArdFSFile open(const char* path, const char* mode) {
    if (*mode == 'w') { ++fakeOpenWrites; fakeFiles[path].clear(); return {&fakeFiles[path], 0}; }
    auto it = fakeFiles.find(path); return it == fakeFiles.end() ? ArdFSFile{} : ArdFSFile{&it->second, 0};
  }
  bool rename(const char* source, const char* destination) { if (failRename) return false; fakeFiles[destination] = fakeFiles[source]; fakeFiles.erase(source); return true; }
};
