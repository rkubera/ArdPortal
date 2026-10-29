#pragma once
#include "../../src/ArdFSVolume.h"
// Standalone volume tests exercise the same privileged queue access as the engine.
// These fixtures do not include or instantiate the production storage engine.
template<> class ArdStorage<ArdFSVolume, ArdFSFile> {
public:
  static uint32_t dataAddress(ArdFSFile& file) {
    return file._handle->writing ? static_cast<ArdFSFile::Writer&>(*file._handle).entry->address : file._handle->address;
  }
  static std::vector<uint16_t> sectors(ArdFSFile& file) {
    if (!file) return {};
    if (file._handle->writing) { auto* e=static_cast<ArdFSFile::Writer&>(*file._handle).entry; return {e->sectors,e->sectors+e->count}; }
    return {file._handle->sectors(), file._handle->sectors() + file._handle->count};
  }
  static size_t queueWrite(ArdFSFile& file, const uint8_t* bytes, size_t count) {
    return file.queueWrite(bytes, count);
  }
};
using ArdFSWriterAccess = ArdStorage<ArdFSVolume, ArdFSFile>;
