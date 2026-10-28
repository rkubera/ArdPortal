// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>
#include <functional>
namespace ArdUILanguageData { enum class Key : uint16_t; }
#include "JsonCodec.h"
#include "JournalReader.h"
#include "JournalStream.h"

// Shared cooperative JSON/raw-file transactions, including format and repair.
// Each loop transfers at most 256 logical bytes. Filesystem metadata operations
// follow the selected backend: native flash steps or synchronous LittleFS calls.
// Streaming JSON validation retains structural/key metadata, not payload values.
// Individual core flash operations and application callbacks remain synchronous.
// One transaction at a time; use separate paths for unrelated documents.
template<class Volume, class FileHandle>
class ArdStorage {
public:
  struct Result {
    bool ok = false, found = false, changed = false, recovered = false;
    bool redundant = false, repaired = false;
    uint8_t repairAttempts = 0;
    String error;
  };
  using Callback = std::function<void(const Result&)>;
  /**
   * @brief Initialize the component and prepare its runtime state.
   * @return No value.
   */
  void begin();
  /**
   * @brief Advance the component work; call repeatedly from the Arduino main loop.
   * @return No value.
   */
  void loop();
  // The source must remain immutable until completion. Syntax is checked incrementally.
  // Each call returns exactly min(maximum,length-offset) bytes, or empty on failure.
  using JsonSource = std::function<String(size_t offset, size_t maximum)>;
  /** @brief Queue a JSON document for incremental validation and streamed journaled storage.
   * @param path Journal file path.
   * @param length Exact serialized byte count.
   * @param source Immutable source returning requested slices until callback completion; empty signals failure.
   * @param callback Receives completion/error from loop().
   * @return True if accepted, not confirmation that data has been committed. */
  bool writeJsonConfig(const String& path, size_t length, JsonSource source, Callback callback);

  // Raw files have no JSON envelope or document-size limit. Capacity is bounded
  // by one flash bank and the other live files. Sources must remain immutable;
  // verification requests the bytes again. Embedded NUL bytes are supported.
  using FileSource = JsonSource;
  using FileSink = std::function<bool(size_t offset, const uint8_t* bytes, size_t length)>;
  /** @brief Validate both JSON copies, then stream the selected payload to the sink. */
  bool readJsonConfig(const String& path, FileSink sink, Callback callback, bool repair = false);
  /** @brief Queue a raw binary file write of a known length; completion is reported from loop(). */
  bool writeFile(const String& path, size_t length, FileSource source, Callback callback);
  /** @brief Stream a raw file to a sink; false from the sink aborts with an error. */
  bool readFile(const String& path, FileSink sink, Callback callback);
  // Raw physical paths; JSON journal copies must be managed separately.
  bool remove(const String& path, Callback callback);
  bool rename(const String& source, const String& destination, Callback callback);
  size_t fileCount() const { return _volume.fileCount(); }
  bool fileInfo(size_t index, String& path, size_t& size) const { return _volume.fileInfo(index, path, size); }
  size_t totalBytes() const { return _volume.totalBytes(); }
  size_t usedBytes() const { return _volume.usedBytes(); }
  size_t freeBytes() const { return _volume.freeBytes(); }

  /**
   * @brief Queue formatting of the storage volume; existing records will be discarded.
   * @param callback Completion or event handler supplied to this operation.
   * @return True if the operation was accepted; loop() later reports its result through callback. False if busy or input cannot be accepted.
   */
  bool format(Callback callback);
  /**
   * @brief Check whether initial storage mounting has completed.
   * @return True once initial mounting has finished, even if mounting failed.
   */
  bool ready() const {
    return _mountDone;
  }
  /**
   * @brief Check whether the storage volume is mounted successfully.
   * @return True when the volume is mounted and available.
   */
  bool mounted() const {
    return _mounted;
  }
  /**
   * @brief Check whether initialization or a queued operation still requires loop() work.
   * @return True while initialization or an operation is pending.
   */
  bool busy() const {
    return !_mountDone || _phase != Phase::Idle;
  }
  /**
   * @brief Expose the most recent error reported by this component.
   * @return Reference to the last error text; empty when no error is recorded.
   */
  const String& error() const {
    return _error;
  }
  /**
   * @brief Read the number of successful persistent commits.
   * @return Number of completed persistent commits.
   */
  uint32_t commits() const {
    return _commits;
  }
  /**
   * @brief Read the number of writes skipped because the saved document was unchanged.
   * @return Number of unchanged writes avoided.
   */
  uint32_t skipped() const {
    return _skipped;
  }
  // SIZE_MAX means no fixed document limit; usable capacity depends on the volume.
  static constexpr size_t MaxBytes = SIZE_MAX;
private:
  enum class Phase { Mount,
                     End,
                     Format,
                     Remount,
                     Idle,
                     PrepareMutation,
                     FlushMutation,
                     OpenFileRead,
                     ReadFile,
                     OpenRead,
                     Read,
                     Validate,
                     OpenPayload,
                     ReadPayload,
                     DeliverPayload,
                     ReadRepair,
                     WriteRepair,
                     Prepare,
                     RepairWait,
                     SourceCrc,
                     OpenWrite,
                     PrepareWrite,
                     Write,
                     Flush,
                     CloseWrite,
                     OpenVerify,
                     Verify,
                     CloseVerify };
  // Xtensa L8UI has an 8-bit byte offset. Keep flags and frequently addressed
  // Strings ahead of the large parser and transfer buffers.
  Phase _phase = Phase::Idle;
  bool _mountDone = false, _mounted = false, _writing = false, _formatRequested = false;
  bool _valid[2] = {false, false}, _present[2] = {false, false}, _match[2] = {false, false};
  bool _rawFile = false;
  bool _legacy[2] = {false, false}, _legacyMatch = false;
  bool _repairFailed = false, _repairDone = false;
  bool _repairRequested = false, _repairing = false;
  bool _sourceMatch = false, _sourceFailed = false;
  uint8_t _repairAttempts = 0, _journalStage = 0;
  char _escape[6] = {};
  String _path, _error, _repairError, _rawSourceCompare, _sourceCompare, _raw, _journalHeader;
  FileHandle _file, _repairFile;
  Callback _callback;
  JsonSource _source;
  FileSink _sink;
  uint32_t _generation[2] = {0, 0}, _commits = 0, _skipped = 0;
  int _slot = 0, _best = -1;
  size_t _offset = 0, _expected = 0;
  size_t _decodedSize = 0, _payloadAt = 0, _decodedAt = 0;
  uint32_t _runningFileCrc = UINT32_MAX, _fileCrc[2] = {0, 0};
  size_t _rawCompareAt = 0;
  size_t _length[2] = {0, 0}, _physicalLength[2] = {0, 0};
  uint32_t _repairRetrySince = 0;
  size_t _sourceLength = 0, _sourceAt = 0, _sourceCompareAt = 0, _escapedLength = 0, _journalHeaderAt = 0, _rawAt = 0, _escapeAt = 0, _escapeLength = 0;
  uint32_t _sourceCrc = 0, _sourceGeneration = 0;
  ArdJournalReader _reader;
  uint8_t _decoded[260] = {};
  // Keep hot parser offsets independent of the backend volume size.
  Volume _volume;
  void resetJournalOutput();
  size_t journalOutput();
  String sourcePart(size_t offset, size_t maximum);
  enum class OpenMode : uint8_t { Read, Write, Scan };
  bool openSlot(FileHandle& file, int slot, OpenMode mode, ArdUILanguageData::Key failure);

  /**
   * @brief Choose the primary or journal path for a configuration slot.
   * @param slot Journal slot index: primary or alternate.
   * @return The resulting text; an empty value indicates no available text or failure where applicable.
   */
  String slotPath(int slot) const {
    return slot ? _path + ".journal" : _path;
  }
  /**
   * @brief Prepare a new operation after checking that the component is idle.
   * @param path Journal file path.
   * @param callback Completion or event handler supplied to this operation.
   * @return True if the operation was accepted; loop() later reports its result through callback. False if busy or input cannot be accepted.
   */
  bool start(const String& path, Callback&& callback);
  /**
   * @brief Complete the operation and report its stored result.
   * @param ok Whether the completed operation succeeded.
   * @param error Output error text; populated when the operation fails.
   * @param changed Whether persistent contents changed during this operation.
   * @return No value.
   */
  void fail(ArdUILanguageData::Key message);
  void finish(bool ok, const String& error = "", bool changed = false);
  /**
   * @brief Compute the checksum covering the journal generation and document bytes.
   * @param generation Journal generation number included in the checksum.
   * @param data Data buffer or value used by the operation.
   * @return The checksum covering the journal generation and document bytes.
   */
  static uint32_t checksum(uint32_t generation, const String& data);
};

#include "ArdStorageImpl.h"
