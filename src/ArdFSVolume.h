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
namespace ArdFSDetail {
struct Entry {
  ARDPORTAL_NO_THROW_ALLOCATION
  ArdFSVolume* volume;
  char* path;
  uint16_t* sectors;
  uint32_t address = 0, length = 0;
  uint16_t count = 0, dataOffset = 0;
  uint8_t pathLength;
  Entry* next = nullptr;
  static Entry* create(ArdFSVolume* volume, size_t nameLength, size_t slots = 0);
};
}
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
    if (this != &other) {
      close();
      _handle = std::move(other._handle);
    }
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
   * @brief Accept bytes into the uncommitted write queue/cache; flush commits them.
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
  // Drain queued bytes/program and verify cache pages; -1 error, 0 pending, 1 ready.
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
  // Readers own a cursor and an exact sector list; writers share a 256-byte cache and 256-byte relocation scratch.
  struct Handle {
    ARDPORTAL_NO_THROW_ALLOCATION
    ArdFSVolume* volume = nullptr;
    uint32_t address = 0, length = 0;
    size_t offset = 0;
    bool writing = false;
    uint16_t count = 0;
    uint32_t epoch = 0;
    const uint16_t* sectors() const {
      return reinterpret_cast<const uint16_t*>(this + 1);
    }
    uint16_t* sectors() {
      return reinterpret_cast<uint16_t*>(this + 1);
    }
  };
  template<size_t N> struct Buffer {
    ARDPORTAL_NO_THROW_ALLOCATION
    alignas(4) uint8_t data[N];
  };
  struct SectorBuffer {
    ARDPORTAL_NO_THROW_ALLOCATION
    alignas(4) uint8_t data[256], replay[256];
  };
  using MapBuffer = Buffer<256>;
  struct MapCommit {
    uint32_t slot;
    uint16_t left, oldSector, at;
    uint8_t stage, target;
    bool mirror, uncertain, rotating;
  };
  struct Writer : Handle {
    const char* path;
    // Points into the stable live index until the new bank is published.
    const char* mutationSource = nullptr;
    ArdFSDetail::Entry* entry = nullptr;
    std::unique_ptr<Writer> copy;
    std::unique_ptr<SectorBuffer> ownedBuffer;
    SectorBuffer* sectorBuffer = nullptr;
    uint32_t bank = 0, capacity = 0;
    size_t expected = SIZE_MAX;
    uint32_t checksum = 0xffffffff;
    uint32_t workAt = 0, extent = 0, verified = 0;
    uint32_t sectorAddress = 0, replaySource = 0;
    uint16_t replayAt = 0, replayLimit = 0;
    MapCommit map = {};
    uint16_t allocationLeft = 0, node = 0;
    uint16_t sectorUsed = 0, sectorAt = 0, pendingAt = 0, pendingSize = 0;
    uint8_t pathLength = 0, copyIndex = 0;
    bool published = false, publishingRoot = false;
    bool failed = false, committed = false, compacting = false, reusingBank = false;
    uint8_t prepareStage = 0, reserveStage = 0, finishStage = 0, flushStage = 0;
    // Low bits: 0/1 program/verify, 2 erase, 3 allocate, 5 replay read,
    // 8 erase verification, 9 root metadata read. Bit 4 selects relocation.
    uint8_t sectorStage = 0, sectorRetries = 0;
    bool sectorFinal = false, sectorDone = false;
    alignas(4) uint8_t bytes[256];
    const char* copyPath(const char* source) const;
    static Writer* create(const char* path);
    bool allocateBuffer();
    Writer() {
      writing = true;
    }
  };
  struct HandleDeleter {
    void operator()(Handle* handle) const noexcept;
  };
  std::unique_ptr<Handle, HandleDeleter> _handle;
};
// ArdFS uses copy-on-write fragments and atomic catalog/bank checkpoints.
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
  bool requiresRemount() const {
    return !_mounted;
  }
  /**
   * @brief Unmount the volume and release its active state.
   * @return No value.
   */
  void end() {
    _mounted = false;
    _formatBuffer.reset();
    _entries.clear();
    _staged.clear();
    _orphans.clear();
    _uses[0].reset();
    _uses[1].reset();
    _readers[0] = _readers[1] = 0;
    _transaction = nullptr;
    _writer = false;
    ++_epoch;
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
  // Unlink updates the catalog; rename copies the requested file.
  // Migration or collection can move live files into the other bank.
  // A null destination removes source; an existing destination is rejected.
  ArdFSFile mutate(const char* source, const char* destination = nullptr);
  size_t fileCount() const {
    return _mounted ? _entries.count : 0;
  }
  bool fileInfo(size_t index, String& path, size_t& size) const;
  // Active-bank capacity; usage includes pending writes and pinned readers.
  size_t totalBytes() const {
    return _mounted ? bankLimit(_maps[_active != 0]) - 4096 : 0;
  }
  size_t usedBytes() const;
  size_t freeBytes() const {
    return totalBytes() - usedBytes();
  }
  // Upper bound for one record; other live files and sector rounding may reduce it.
  size_t maxFileBytes(size_t pathBytes) const {
    return _mounted ? fragmentCapacity(pathBytes) : 0;
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
  using Entry = ArdFSDetail::Entry;
  // A bounded index needs neither contiguous growth nor relocation of paths.
  struct Index {
    Entry* head = nullptr;
    size_t count = 0;
    ~Index() {
      clear();
    }
    void clear();
    Entry* find(const char* path) const;
    Entry* at(size_t index) const;
    bool insert(Entry* entry);
    void merge(Index& other);
    void erase(const char* path);
  };
  struct BankMap {
    // Keep frequently accessed words in the short-offset range on Xtensa.
    uint32_t generation, revision, erases, slot;
    uint16_t nextSector, meta[2];
    uint8_t count;
    bool valid, rotate;
    alignas(4) uint16_t bad[32];
  };
  bool _mounted = false, _configured = false, _writer = false;
  uint8_t _mountStage = 0, _formatStage = 0, _catalogCount = 0, _catalogAt = 0;
  int _error = 0;
  BankMap* _workMap = nullptr;
  uint32_t _bankSize = 0, _workBank = 0, _active = 0, _generation = 0;
#if defined(ESP32)
  const esp_partition_t* _partition = nullptr;
#else
  uint32_t _address = 0;
#endif
  ArdAllocation::Pointer<uint8_t> _uses[2];
  uint32_t _epoch = 1;
  ArdFSFile::Writer* _transaction = nullptr;
  Index _entries, _staged, _orphans;
  std::unique_ptr<Entry> _scanEntry;
  uint16_t _readers[2] = {};
  bool _headerObserved = false, _checkDamage = false;
  uint32_t _headerScan = 0;
  uint32_t _scanExtent = 0, _scanName = 0, _scanLength = 0, _scanAt = 0;
  uint32_t _scanCrc = 0, _scanExpected = 0, _formatAt = 0;
  uint32_t _scanMetaExpected = 0, _scanRootRevision = 0;
  uint16_t _catalog[32] = {};
  std::unique_ptr<ArdFSFile::MapBuffer> _formatBuffer;
  BankMap _maps[2] = {};
  ArdFSFile::MapCommit _formatMap = {};
  void clearMaps();
  static void destroyEntry(Entry* entry);
  bool initializeUses(uint32_t bank);
  uint32_t freeSectors(uint32_t bank) const;
  int allocateFragment(ArdFSFile::Writer& h, uint16_t& sector);
  uint32_t collectionSectors(const ArdFSFile::Writer& h) const;
  bool retireFragment(ArdFSFile::Writer& h);
  bool retireSector(uint16_t sector);
  bool isBad(const BankMap& map, uint16_t sector) const;
  int allocateSector(uint16_t& left, uint16_t& sector);
  bool readFragments(uint32_t address, const uint16_t* sectors, size_t count, size_t offset, void* bytes, size_t n);
  bool readEntry(const Entry& entry, size_t offset, void* bytes, size_t n);
  int readEntryChunk(const Entry& entry, uint32_t& at, uint32_t& sum, uint8_t* bytes);
  bool sourceChecksum(const Entry& entry, uint8_t* header, uint32_t& checksum);
  int copyPayload(ArdFSFile::Writer& h, const Entry& source);
  int prepareFragment(ArdFSFile::Writer& h);
  int finishFragment(ArdFSFile::Writer& h);
  void buildFragmentHeader(ArdFSFile::Writer& h);
  void overlayFragmentHeader(ArdFSFile::Writer& h, uint8_t* bytes, size_t offset, size_t length);
  int retrySector(ArdFSFile::Writer& h);
  static uint32_t fragmentPrefix(const uint8_t* header, uint32_t generation);
  void buildCatalog(const ArdFSFile::Writer& h);
  void publishFragments(ArdFSFile::Writer& h);
  int mountFragments(uint8_t* bytes);
  int orphanRoot();
  uint32_t fragmentCapacity(size_t pathBytes) const;
  uint32_t dataSectors(const BankMap& map) const;
  static uint32_t fragmentCount(size_t pathBytes, size_t length);
  static uint32_t framedNode(uint32_t position);
  uint32_t bankLimit(const BankMap& map) const;
  bool readPhysical(uint32_t, void*, size_t);
  bool writePhysical(uint32_t, const void*, size_t);
  bool erasePhysical(uint32_t);
  int checkPhysical(uint32_t address, const void* expected, size_t n);
  int eraseStep(uint32_t address, uint16_t& at);
  int programStep(uint32_t address, const void* bytes, size_t n, uint8_t& stage);
  int decodeMap(uint8_t* bytes, BankMap& map, uint32_t bank, uint32_t slot);
  void encodeMap(uint8_t* bytes, uint32_t slot) const;
  void completeMap(ArdFSFile::MapCommit& h);
  void cancelMap(ArdFSFile::MapCommit& h);
  int publishMapStep(ArdFSFile::MapCommit&, uint8_t* buffer);
  bool nextMapSector(ArdFSFile::MapCommit&);
  /**
   * @brief Locate and validate the flash region used by the two-bank journal.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool configure();
  int mountStep();
  /**
   * @brief Read a bounded flash range in small transfers, yielding between chunks.
   * Input: uint32_t address, the byte offset within the volume.
   * Input: void* bytes, the destination buffer.
   * Input: size_t n, the number of bytes to read.
   * @return True if all bytes were read; false for invalid bounds or a flash error.
   */
  // Reserve fragments, stream the payload, then publish the catalog.
  int prepareStep(ArdFSFile::Writer&);
  void startSectorBuffer(ArdFSFile::Writer&);
  static void bufferInput(ArdFSFile::Writer&, const uint8_t*, size_t);
  int writeBufferedStep(ArdFSFile::Writer&, bool final = false);
  int drainSectorStep(ArdFSFile::Writer&);
  int commitStep(uint32_t address, uint8_t* scratch, uint8_t& stage);
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
