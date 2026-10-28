// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>
#include "ArdAllocation.h"
#include <memory>
#if defined(ESP32)
#include <esp_partition.h>
#endif

class ArdFSVolume;
class ArdFSFile {
public:
  /**
   * @brief Initialize this instance and its owned state.
   * @return No value.
   */
  ArdFSFile() = default;
  /**
   * @brief Initialize this instance and its owned state.
   * Input: ArdFSFile&&.
   * @return No value.
   */
  ArdFSFile(ArdFSFile&&) = default;
  /**
   * @brief Replace this instance state with the supplied value.
   * Input: ArdFSFile&&.
   * @return Reference to this instance after assignment; deleted overloads cannot be called.
   */
  ArdFSFile& operator=(ArdFSFile&& other) noexcept {
    if (this != &other) { close(); _handle = std::move(other._handle); }
    return *this;
  }
  /**
   * @brief Disallow copying or moving this resource-owning instance.
   * Input: const ArdFSFile&.
   * @return No result; this operation is deleted and cannot be called.
   */
  ArdFSFile(const ArdFSFile&) = delete;
  /**
   * @brief Release the resources owned by this instance.
   * @return No value.
   */
  ~ArdFSFile() {
    close();
  }
  /**
   * @brief Check whether this journal file handle is open.
   * @return True if this file handle is open; false otherwise.
   */
  explicit operator bool() const;
  /**
   * @brief Read the file length.
   * @return Number of file bytes, or zero for a closed handle.
   */
  size_t size() const;
  /**
   * @brief Read bytes from the file handle and advance its cursor.
   * Input: uint8_t* p, the destination buffer.
   * Input: size_t n, the number of bytes to transfer or erase.
   * @return Number of bytes read; a negative value indicates failure.
   */
  int read(uint8_t*, size_t);
  /**
   * @brief Write bytes directly to the uncommitted flash record.
   * Input: const uint8_t* p, the source buffer.
   * Input: size_t n, the number of bytes to transfer or erase.
   * @return Number of bytes appended, or zero when the write cannot be accepted.
   */
  size_t write(const uint8_t*, size_t);
  /**
   * @brief Verify and commit the streamed flash record.
   * @return True on a successful commit or when no commit is needed; false on failure.
   */
  bool flush();
  // Cooperative steps: -1 = error, 0 = pending, 1 = complete.
  // Each step performs at most one bounded transfer or one sector erase.
  int prepareStep();
  int flushStep();
  /**
   * @brief Release the handle; an uncommitted write is abandoned.
   * @return True after releasing the handle.
   */
  bool close();
private:
  friend class ArdFSVolume;
  struct Handle {
    ARDPORTAL_NO_THROW_ALLOCATION ArdFSVolume* volume;
    String path;
    uint32_t address = 0, bank = 0, record = 0, capacity = 0, length = 0;
    size_t expected = SIZE_MAX;
    uint32_t checksum = 0xffffffff;
    bool failed = false, committed = false, compacting = false;
    size_t offset = 0;
    bool writing = false;
    uint8_t prepareStage = 0, reserveStage = 0, finishStage = 0, flushStage = 0;
    uint32_t workAt = 0, extent = 0, verified = 0, verifiedBody = 0;
    size_t copyIndex = 0, copyAt = 0, copyCount = 0;
    uint8_t header[32] = {}, bytes[256] = {};
    std::unique_ptr<Handle> copy;
  };
  std::unique_ptr<Handle> _handle;
};
// ArdFS uses its own append-only records and two atomic compaction banks.
class ArdFSVolume {
public:
  /**
   * @brief Initialize this instance and its owned state.
   * @return No value.
   */
  ArdFSVolume() = default;
  /**
   * @brief Disallow copying or moving this resource-owning instance.
   * Input: const ArdFSVolume&.
   * @return No result; this operation is deleted and cannot be called.
   */
  ArdFSVolume(const ArdFSVolume&) = delete;
  /**
   * @brief Replace this instance state with the supplied value.
   * Input: const ArdFSVolume&.
   * @return No result; this operation is deleted and cannot be called.
   */
  ArdFSVolume& operator=(const ArdFSVolume&) = delete;
  /**
   * @brief Release the resources owned by this instance.
   * @return No value.
   */
  ~ArdFSVolume() {
    end();
  }
  /**
   * @brief Initialize the component and prepare its runtime state.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool begin();
  int beginStep();
  /**
   * @brief Unmount the volume and release its active state.
   * @return No value.
   */
  void end() {
    _mounted = false;
    _entries.clear();
    _mountStage = _formatStage = 0;
    _scanPath = String();
  }
  /**
   * @brief Format the storage volume, discarding its existing records.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool format();
  int formatStep();
  /**
   * @brief Check whether the mounted journal contains the requested path.
   * Input: const char* path, the journal file path.
   * @return True if the path exists; false if missing or not mounted.
   */
  bool exists(const char*);
  /**
   * @brief Open a journal path for the requested access mode.
   * Input: const char* path, the journal file path.
   * Input: const char* mode, the read or write access mode.
   * Input: size_t length, exact write length; omitted reserves available capacity.
   * Input: bool deferred, true to prepare the writer through prepareStep().
   * @return File handle; its boolean conversion is false when opening fails.
   */
  ArdFSFile open(const char*, const char*, size_t length = SIZE_MAX, bool deferred = false);
  // Upper bound for one record; other live files and sector rounding may reduce it.
  size_t maxFileBytes(size_t pathBytes) const {
    return _mounted && pathBytes && pathBytes <= 72 ? _bankSize - 4096 - 32 - pathBytes : 0;
  }
  /**
   * @brief Read the last storage error code.
   * @return The last storage error code.
   */
  int lastError() const {
    return _error;
  }
private:
  friend class ArdFSFile;
  struct Entry {
    ARDPORTAL_NO_THROW_ALLOCATION
    String path;
    uint32_t address, length;
    Entry* next = nullptr;
  };
  // A bounded index needs neither contiguous growth nor relocation of paths.
  struct Index {
    Entry* head = nullptr;
    size_t count = 0;
    ~Index() { clear(); }
    void clear();
    Entry* find(const char* path) const;
    Entry* at(size_t index) const;
    bool set(const String& path, uint32_t address, uint32_t length);
    void merge(Index& other);
  };
  Index _entries;
  bool _mounted = false, _configured = false, _writer = false;
  Index _staged;
  int _error = 0;
  uint8_t _mountStage = 0, _formatStage = 0;
  uint8_t _bankHeaders[2][32] = {};
  uint32_t _scanExtent = 0, _scanName = 0, _scanLength = 0, _scanAt = 0;
  uint32_t _scanCrc = 0, _scanExpected = 0, _formatAt = 0;
  String _scanPath;
  uint32_t _address = 0, _size = 0, _bankSize = 0, _active = 0, _generation = 0, _next = 0;
#if defined(ESP32)
  const esp_partition_t* _partition = nullptr;
#endif
  /**
   * @brief Locate and validate the flash region used by the two-bank journal.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool configure();
  /**
   * @brief Read a bounded flash range in small transfers, yielding between chunks.
   * Input: uint32_t address, the byte offset within the volume.
   * Input: void* bytes, the destination buffer.
   * Input: size_t n, the number of bytes to read.
   * @return True if all bytes were read; false for invalid bounds or a flash error.
   */
  bool readRaw(uint32_t, void*, size_t);
  /**
   * @brief Write a bounded flash range in small transfers, yielding between chunks.
   * Input: uint32_t address, the byte offset within the volume.
   * Input: const void* bytes, the source buffer.
   * Input: size_t n, the number of bytes to transfer or erase.
   * @return True if all requested bytes were written; false for invalid bounds or a flash error.
   */
  bool writeRaw(uint32_t, const void*, size_t);
  /**
   * @brief Erase complete flash sectors, yielding between sectors.
   * Input: uint32_t address, the byte offset within the volume.
   * Input: size_t n, the number of bytes to transfer or erase.
   * @return True if the aligned sector range was erased; false for invalid alignment, bounds or a flash error.
   */
  bool eraseRaw(uint32_t, size_t);
  // Reserve an erased record, stream its body, then publish its commit marker.
  int prepareStep(ArdFSFile::Handle&);
  int reserveStep(ArdFSFile::Handle&);
  int finishRecordStep(ArdFSFile::Handle&);
  int publishStep(uint32_t address, const uint8_t* header, uint8_t* scratch, uint8_t& stage);
  static uint32_t makeHeader(uint8_t* bytes, uint32_t magic, uint32_t a, uint32_t b, uint32_t c);
  /**
   * @brief Update the journal CRC-32 checksum over a byte range.
   * Input: const void* bytes, the source buffer.
   * Input: size_t n, the number of bytes to transfer or erase.
   * Input: uint32_t address, the byte offset within the volume.
   * @return Updated CRC-32 accumulator; callers apply the final complement when committing a record.
   */
  static uint32_t crc(const void*, size_t, uint32_t = 0xffffffff);
};
