// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>
#if defined(ARDFS_HOST_TEST)
#include <ArdFSHostVolume.h>
#else
#include "ArdFSVolume.h"
#endif
#include <functional>
#include "JsonCodec.h"

// Cooperative JSON storage. Each loop advances one phase; transfers use at most
// 256 bytes. JSON parsing, CRC and serialization run synchronously in RAM. Core flash erase/program/sync/mount remain synchronous.
// One transaction at a time; use separate paths for unrelated documents.
class ArdFS {
public:
  struct Result {
    bool ok = false, found = false, changed = false, recovered = false;
    String data, error;
  };
  using Callback = std::function<void(const Result&)>;
  void begin();
  void loop();
  bool read(const String& path, Callback callback);
  bool write(const String& path, const String& json, Callback callback);
  bool format(Callback callback);
  bool ready() const { return _mountDone; }
  bool mounted() const { return _mounted; }
  bool busy() const { return !_mountDone || _phase != Phase::Idle; }
  const String& error() const { return _error; }
  uint32_t commits() const { return _commits; }
  uint32_t skipped() const { return _skipped; }
  static constexpr size_t MaxBytes = 8192;
private:
  enum class Phase { Mount, End, Format, Remount, Idle, OpenRead, Read, Validate,
                     Prepare, OpenWrite, Write, Flush, CloseWrite, OpenVerify, Verify, CloseVerify };
  Phase _phase = Phase::Idle;
  bool _mountDone = false, _mounted = false, _writing = false, _formatRequested = false;
  bool _valid[2] = {false, false}, _present[2] = {false, false}, _match[2] = {false, false};
  uint32_t _generation[2] = {0, 0}, _commits = 0, _skipped = 0;
  int _slot = 0, _best = -1;
  size_t _offset = 0, _expected = 0;
  String _path, _input, _buffer, _bestData, _error;
  ArdJsonCodec _json;
  ArdFSVolume _volume;
  ArdFSFile _file;
  Callback _callback;
  String slotPath(int slot) const { return slot ? _path + ".journal" : _path; }
  bool start(const String& path, Callback callback);
  void finish(bool ok, const String& error = "", bool changed = false);
  static uint32_t checksum(uint32_t generation, const String& data);
};
