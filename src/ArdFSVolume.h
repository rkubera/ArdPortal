// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>
#include "ArdAllocation.h"
#include <memory>
#include <vector>
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
  ArdFSFile& operator=(ArdFSFile&&) = default;
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
  ~ArdFSFile() { close(); }
  /**
   * @brief Check whether this journal file handle is open.
   * @return True if this file handle is open; false otherwise.
   */
  explicit operator bool() const;
  /**
   * @brief Read the buffered file length.
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
   * @brief Append bytes to the writable file buffer.
   * Input: const uint8_t* p, the source buffer.
   * Input: size_t n, the number of bytes to transfer or erase.
   * @return Number of bytes appended, or zero when the write cannot be accepted.
   */
  size_t write(const uint8_t*, size_t);
  /**
   * @brief Commit dirty buffered file contents to the journal.
   * @return True on a successful commit or when no commit is needed; false on failure.
   */
  bool flush();
  /**
   * @brief Flush the file if needed and release its handle.
   * @return Whether the flush completed successfully.
   */
  bool close();
private:
  friend class ArdFSVolume;
  struct Handle { ARDPORTAL_NO_THROW_ALLOCATION  ArdFSVolume* volume; String path; String data; size_t offset=0; bool writing=false, dirty=false;
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
  ~ArdFSVolume() { end(); }
  /**
   * @brief Initialize the component and prepare its runtime state.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool begin();
  /**
   * @brief Unmount the volume and release its active state.
   * @return No value.
   */
  void end() {
    _mounted=false; _entries.clear();
  }
  /**
   * @brief Format the storage volume, discarding its existing records.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool format();
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
   * @return File handle; its boolean conversion is false when opening fails.
   */
  ArdFSFile open(const char*, const char*);
  /**
   * @brief Read the last storage error code.
   * @return The last storage error code.
   */
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
  /**
   * @brief Scan committed journal records and rebuild the live file index.
   * Input: uint32_t address, the byte offset within the volume.
   * Input: std::vector<Entry>&.
   * Input: uint32_t&.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool scan(uint32_t, std::vector<Entry>&, uint32_t&);
  /**
   * @brief Append new file contents to the journal, compacting the bank when necessary.
   * Input: const String&.
   * Input: const String&.
   * @return True when the record is committed; false on space, allocation or flash failure.
   */
  bool commit(const String&, const String&);
  /**
   * @brief Append the supplied data or journal record to the current destination.
   * Input: uint32_t address, the byte offset within the volume.
   * Input: uint32_t&.
   * Input: const String&.
   * Input: const String&.
   * Input: std::vector<Entry>&.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool append(uint32_t,uint32_t&,const String&,const String&,std::vector<Entry>&);
  /**
   * @brief Copy live records to the alternate bank and atomically commit a new generation with the replacement file.
   * Input: const String&.
   * Input: const String&.
   * @return True when the new bank is committed; false on allocation or flash failure.
   */
  bool compact(const String&, const String&);
  /**
   * @brief Update the journal CRC-32 checksum over a byte range.
   * Input: const void* bytes, the source buffer.
   * Input: size_t n, the number of bytes to transfer or erase.
   * Input: uint32_t address, the byte offset within the volume.
   * @return Updated CRC-32 accumulator; callers apply the final complement when committing a record.
   */
  static uint32_t crc(const void*, size_t, uint32_t=0xffffffff);
};
