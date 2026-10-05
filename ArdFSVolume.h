// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>
#include <memory>
#include <vector>
#if defined(ESP32)
#include <esp_partition.h>
#endif

class ArdFSVolume;
class ArdFSFile {
public:
  ArdFSFile() = default;
  ArdFSFile(ArdFSFile&&) = default;
  ArdFSFile& operator=(ArdFSFile&&) = default;
  ArdFSFile(const ArdFSFile&) = delete;
  ~ArdFSFile() { close(); }
  explicit operator bool() const;
  size_t size() const;
  int read(uint8_t*, size_t);
  size_t write(const uint8_t*, size_t);
  bool flush();
  bool close();
private:
  friend class ArdFSVolume;
  struct Handle { ArdFSVolume* volume; String path; String data; size_t offset=0; bool writing=false, dirty=false;
  };
  std::unique_ptr<Handle> _handle;
};
// ArdFS uses its own append-only records and two atomic compaction banks.
class ArdFSVolume {
public:
  ArdFSVolume() = default;
  ArdFSVolume(const ArdFSVolume&) = delete;
  ArdFSVolume& operator=(const ArdFSVolume&) = delete;
  ~ArdFSVolume() { end(); }
  bool begin();
  void end() {
    _mounted=false; _entries.clear();
  }
  bool format();
  bool exists(const char*);
  ArdFSFile open(const char*, const char*);
  int lastError() const { return _error; }
private:
  friend class ArdFSFile;
  struct Entry { String path; uint32_t address, length; };
  std::vector<Entry> _entries;
  bool _mounted=false, _configured=false;
  int _error=0;
  uint32_t _address=0,_size=0,_bankSize=0,_active=0,_generation=0,_next=0;
#if defined(ESP32)
  const esp_partition_t* _partition=nullptr;
#endif
  bool configure();
  bool readRaw(uint32_t, void*, size_t);
  bool writeRaw(uint32_t, const void*, size_t);
  bool eraseRaw(uint32_t, size_t);
  bool scan(uint32_t, std::vector<Entry>&, uint32_t&);
  bool commit(const String&, const String&);
  bool append(uint32_t,uint32_t&,const String&,const String&,std::vector<Entry>&);
  bool compact(const String&, const String&);
  static uint32_t crc(const void*, size_t, uint32_t=0xffffffff);
};
