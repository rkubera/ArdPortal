// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>
#include "ArdAllocation.h"
#include <memory>
#if defined(ESP32)
#include <esp_partition.h>
#endif

template<class Volume, class FileHandle> class ArdStorage;
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
  // Absolute read cursor, including EOF. Writers remain append-only.
  bool seek(size_t position);
  size_t position() const;
  /**
   * @brief Read bytes from the file handle and advance its cursor.
   * Input: uint8_t* p, the destination buffer.
   * Input: size_t n, the number of bytes to transfer or erase.
   * @return Number of bytes read; a negative value indicates failure.
   */
  int read(uint8_t*, size_t);
  /**
   * @brief Accept bytes into the uncommitted sector buffer; flush commits them.
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
  // Drain queued bytes/verify a full sector; -1 error, 0 pending, 1 ready.
  int writeStep();
  int flushStep();
  /**
   * @brief Release the handle; an uncommitted write is abandoned.
   * @return True after releasing the handle.
   */
  bool close();
private:
  friend class ArdFSVolume;
  friend class ArdStorage<ArdFSVolume, ArdFSFile>;
  // Engine-only input queue; application writes use write().
  size_t queueWrite(const uint8_t*, size_t);
  // Readers own only the cursor. Writers also allocate one shared 4 KiB sector buffer.
  struct Handle {
    ARDPORTAL_NO_THROW_ALLOCATION
    ArdFSVolume* volume = nullptr;
    uint32_t address = 0, length = 0;
    size_t offset = 0;
    bool writing = false;
  };
  template<size_t N> struct Buffer {
    ARDPORTAL_NO_THROW_ALLOCATION
    uint8_t data[N];
  };
  using SectorBuffer = Buffer<4096>;
  using MapBuffer = Buffer<256>;
  struct MapCommit { uint16_t slot; uint8_t stage; bool mirror; };
  struct Writer : Handle {
    const char* path;
    uint8_t pathLength = 0;
    // Points into the stable live index until the new bank is published.
    const char* mutationSource = nullptr;
    uint32_t bank = 0, record = 0, capacity = 0;
    size_t expected = SIZE_MAX;
    uint32_t checksum = 0xffffffff;
    bool failed = false, committed = false, compacting = false, reusingBank = false;
    uint8_t prepareStage = 0, reserveStage = 0, finishStage = 0, flushStage = 0;
    uint32_t workAt = 0, extent = 0, verified = 0;
    size_t copyIndex = 0;
    std::unique_ptr<Writer> copy;
    std::unique_ptr<SectorBuffer> ownedBuffer;
    SectorBuffer* sectorBuffer = nullptr;
    uint32_t sectorAddress = 0, sectorPhysical = 0;
    uint16_t sectorUsed = 0, sectorAt = 0, pendingAt = 0, pendingSize = 0;
    uint8_t sectorStage = 0, sectorRetries = 0;
    bool sectorFinal = false, sectorDone = false;
    uint8_t initialBadCount = 0;
    MapCommit map = {};
    uint8_t bytes[256];
    const char* copyPath(const char* source) const;
    static Writer* create(const char* path);
    bool allocateBuffer();
    Writer() { writing = true; }
  };
  struct HandleDeleter {
    void operator()(Handle* handle) const noexcept;
  };
  std::unique_ptr<Handle, HandleDeleter> _handle;
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
    _working = false;
    _formatBuffer.reset();
    _entries.clear();
    _mountStage = _formatStage = 0;
    _scanEntry.reset();
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
  // Mutations copy live files to the other bank before atomic publication.
  // A null destination removes source; an existing destination is rejected.
  ArdFSFile mutate(const char* source, const char* destination = nullptr);
  size_t fileCount() const { return _mounted ? _entries.count : 0; }
  bool fileInfo(size_t index, String& path, size_t& size) const;
  // Bank capacity and sector-rounded live record usage, excluding bank header.
  size_t totalBytes() const { return _mounted ? bankLimit(_maps[_active != 0]) - 4096 : 0; }
  size_t usedBytes() const;
  size_t freeBytes() const { return totalBytes() - usedBytes(); }
  // Upper bound for one record; other live files and sector rounding may reduce it.
  size_t maxFileBytes(size_t pathBytes) const {
    return _mounted && pathBytes && pathBytes <= 72 ? bankLimit(_maps[_active != 0]) - 4096 - 32 - pathBytes : 0;
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
    char* path;
    uint32_t address, length;
    uint8_t pathLength;
    static Entry* create(size_t nameLength);
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
    bool set(const char* path, uint32_t address, uint32_t length);
    bool insert(Entry* entry);
    void merge(Index& other);
  };
  struct BankMap {
    uint16_t bad[32];
    uint8_t count, metadataBad;
    bool modern, valid;
    uint16_t slot;
    uint32_t generation, revision;
  };
  bool _mounted = false, _configured = false, _writer = false;
  bool _working = false, _headerObserved = false;
  uint8_t _headerErrors[2] = {};
  uint16_t _headerScan = 0;
  int _error = 0;
  uint8_t _mountStage = 0, _formatStage = 0;
  uint32_t _workBank = 0;
  uint32_t _address = 0, _size = 0, _bankSize = 0, _active = 0, _generation = 0, _next = 0;
  uint32_t _scanExtent = 0, _scanName = 0, _scanLength = 0, _scanAt = 0;
  uint32_t _scanCrc = 0, _scanExpected = 0, _formatAt = 0;
  std::unique_ptr<Entry> _scanEntry;
  Index _entries;
  Index _staged;
  std::unique_ptr<ArdFSFile::MapBuffer> _formatBuffer;
  BankMap _maps[2] = {};
  BankMap* _workMap = nullptr;
  ArdFSFile::MapCommit _formatMap = {};
  void clearMaps();
  uint32_t bankLimit(const BankMap& map) const;
  uint32_t physical(uint32_t address, size_t length) const;
  bool retire(uint32_t address);
  bool fitRecord(ArdFSFile::Writer&);
  bool readPhysical(uint32_t, void*, size_t);
  bool writePhysical(uint32_t, const void*, size_t);
  bool erasePhysical(uint32_t);
  bool decodeMap(uint8_t* bytes, BankMap& map, unsigned slot);
  void encodeMap(uint8_t* bytes) const;
  int publishMapStep(ArdFSFile::MapCommit&, uint8_t* buffer);
  bool nextMapSector(ArdFSFile::MapCommit&);
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
  /**
   * @brief Erase complete flash sectors, yielding between sectors.
   * Input: uint32_t address, the byte offset within the volume.
   * Input: size_t n, the number of bytes to transfer or erase.
   * @return True if the aligned sector range was erased; false for invalid alignment, bounds or a flash error.
   */
  bool eraseRaw(uint32_t);
  // Reserve an erased record, stream its body, then publish its commit marker.
  int prepareStep(ArdFSFile::Writer&);
  int terminateStep(ArdFSFile::Writer&);
  int finishRecordStep(ArdFSFile::Writer&);
  void startSectorBuffer(ArdFSFile::Writer&);
  static void bufferInput(ArdFSFile::Writer&, const uint8_t*, size_t);
  int writeBufferedStep(ArdFSFile::Writer&, bool final = false);
  int drainSectorStep(ArdFSFile::Writer&);
  int publishStep(uint32_t address, const uint8_t* header, uint8_t* scratch, uint8_t& stage, size_t length);
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

#include "ArdFSVolumeImpl.h"
