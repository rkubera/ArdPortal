// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ArdFSStorageLimits.h"
#include <Arduino.h>
#if defined(ARDFS_HOST_TEST)
#include <ArdFSHostVolume.h>
#else
#include "ArdFSVolume.h"
#endif
#include <functional>
#include "JsonCodec.h"
#include "JournalStream.h"

// Cooperative JSON storage. Each loop advances one phase; transfers use at most
// 256 bytes. writeJson() validates envelopes and computes CRC incrementally;
// read()/write() retain their legacy full-document JSON path. Core flash operations
// and producer callbacks remain synchronous.
// One transaction at a time; use separate paths for unrelated documents.
class ArdFS {
public:
  struct Result {
    bool ok = false, found = false, changed = false, recovered = false;
    String data, error;
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
  /**
   * @brief Request or perform a read through the component API.
   * @param path Journal file path.
   * @param callback Completion or event handler supplied to this operation.
   * @return True if the operation was accepted; loop() later reports its result through callback. False if busy or input cannot be accepted.
   */
  bool read(const String& path, Callback callback);
  /**
   * @brief Request or perform a write through the component API.
   * @param path Journal file path.
   * @param json JSON document text to persist.
   * @param callback Completion or event handler supplied to this operation.
   * @return True if the operation was accepted; loop() later reports its result through callback. False if busy or input cannot be accepted.
   */
  bool write(const String& path, const String& json, Callback callback);
  // The source must be a validated JSON serialization and remain immutable until completion.
  // Each call returns exactly min(maximum,length-offset) bytes, or empty on failure.
  using JsonSource = std::function<String(size_t offset,size_t maximum)>;
  bool writeJson(const String& path,size_t length,JsonSource source,Callback callback);

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
  bool ready() const { return _mountDone; }
  /**
   * @brief Check whether the storage volume is mounted successfully.
   * @return True when the volume is mounted and available.
   */
  bool mounted() const { return _mounted; }
  /**
   * @brief Check whether initialization or a queued operation still requires loop() work.
   * @return True while initialization or an operation is pending.
   */
  bool busy() const { return !_mountDone || _phase != Phase::Idle; }
  /**
   * @brief Expose the most recent error reported by this component.
   * @return Reference to the last error text; empty when no error is recorded.
   */
  const String& error() const { return _error; }
  /**
   * @brief Read the number of successful persistent commits.
   * @return Number of completed persistent commits.
   */
  uint32_t commits() const { return _commits; }
  /**
   * @brief Read the number of writes skipped because the saved document was unchanged.
   * @return Number of unchanged writes avoided.
   */
  uint32_t skipped() const { return _skipped; }
  static constexpr size_t MaxBytes = ArdFSMaxDocumentBytes;
private:
  enum class Phase { Mount, End, Format, Remount, Idle, OpenRead, Read, Validate,
                     Prepare, SourceCrc, OpenWrite, Write, Flush, CloseWrite, OpenVerify, Verify, CloseVerify };
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
  JsonSource _source;
  ArdJournalStream _journal;
  bool _sourceLegacy=false,_sourceMatch=false,_sourceFailed=false;
  size_t _sourceLength=0,_sourceAt=0,_sourceCompareAt=0,_escapedLength=0,_journalHeaderAt=0,_rawAt=0,_escapeAt=0,_escapeLength=0;
  uint8_t _journalStage=0;
  uint32_t _sourceCrc=0,_sourceGeneration=0;
  String _sourceCompare,_raw,_journalHeader;
  char _escape[6]={};
  void resetJournalOutput();
  String journalOutput();

  /**
   * @brief Choose the primary or journal path for a configuration slot.
   * @param slot Journal slot index: primary or alternate.
   * @return The resulting text; an empty value indicates no available text or failure where applicable.
   */
  String slotPath(int slot) const { return slot ? _path + ".journal" : _path; }
  /**
   * @brief Prepare a new operation after checking that the component is idle.
   * @param path Journal file path.
   * @param callback Completion or event handler supplied to this operation.
   * @return True if the operation was accepted; loop() later reports its result through callback. False if busy or input cannot be accepted.
   */
  bool start(const String& path, Callback callback);
  /**
   * @brief Complete the operation and report its stored result.
   * @param ok Whether the completed operation succeeded.
   * @param error Output error text; populated when the operation fails.
   * @param changed Whether persistent contents changed during this operation.
   * @return No value.
   */
  void finish(bool ok, const String& error = "", bool changed = false);
  /**
   * @brief Compute the checksum covering the journal generation and document bytes.
   * @param generation Journal generation number included in the checksum.
   * @param data Data buffer or value used by the operation.
   * @return The checksum covering the journal generation and document bytes.
   */
  static uint32_t checksum(uint32_t generation, const String& data);
};
