#pragma once
#include "../../src/ArdFSVolume.h"
// Standalone volume tests exercise the same privileged queue access as the engine.
// These fixtures do not include or instantiate the production storage engine.
template<> class ArdStorage<ArdFSVolume, ArdFSFile> {
public:
  static size_t queueWrite(ArdFSFile& file, const uint8_t* bytes, size_t count) {
    return file.queueWrite(bytes, count);
  }
};
using ArdFSWriterAccess = ArdStorage<ArdFSVolume, ArdFSFile>;
