#include "Arduino.h"
#include "ArdFSWriterAccess.h"
#include <cassert>
uint32_t fakeMillis = 0;
SerialStub Serial;
EspStub ESP;
std::vector<uint8_t> ardfsTestFlash(FS_PHYS_SIZE, 0xff);
int ardfsFlashCalls = 0, ardfsFailAt = -1, ardfsPrograms = 0, ardfsErases = 0;
bool ardfsPowerOff = false, ardfsTornOperation = false;
static int bounded(ArdFSFile& f, int operation) {
  int calls = ardfsFlashCalls, writes = ardfsPrograms, erases = ardfsErases;
  int result = operation == 0 ? f.prepareStep() : operation == 1 ? f.writeStep() : f.flushStep();
  assert(ardfsFlashCalls - calls <= 2);
  if (ardfsPrograms != writes || ardfsErases != erases) assert(ardfsFlashCalls - calls <= 1);
  return result;
}
static bool save(ArdFSVolume& v, const char* path, size_t length, unsigned seed) {
  auto f = v.open(path, "w", length, true);
  if (!f) return false;
  int status, limit = 0;
  do { status = bounded(f, 0); assert(++limit < 20000); } while (!status);
  if (status < 0) return false;
  uint8_t bytes[256];
  for (size_t at = 0; at < length;) {
    do { status = bounded(f, 1); assert(++limit < 20000); } while (!status);
    if (status < 0) return false;
    size_t n = std::min<size_t>(sizeof(bytes), length - at);
    for (size_t i = 0; i < n; ++i) bytes[i] = uint8_t((at + i) * 13 + seed);
    assert(f.write(bytes, n) == n);
    memset(bytes, 0, sizeof(bytes)); // No replayable source retained by the caller.
    at += n;
  }
  do { status = bounded(f, 2); assert(++limit < 20000); } while (!status);
  return status == 1;
}
static void check(ArdFSVolume& v, const char* path, size_t length, unsigned seed) {
  auto f = v.open(path, "r"); assert(f && f.size() == length);
  uint8_t bytes[256];
  for (size_t at = 0; at < length;) {
    size_t n = std::min<size_t>(sizeof(bytes), length - at);
    assert(f.read(bytes, n) == int(n));
    for (size_t i = 0; i < n; ++i) assert(bytes[i] == uint8_t((at + i) * 13 + seed));
    at += n;
  }
  assert(f.seek(4000) == (length >= 4000));
}
static void reset() {
  ESP.badProgramSectorsForTest.clear(); ESP.badEraseSectorsForTest.clear(); ESP.badReadSectorsForTest.clear();
  ESP.silentEraseSectorsForTest.clear();
  ardfsPowerOff = false; ardfsFailAt = -1;
  std::fill(ardfsTestFlash.begin(), ardfsTestFlash.end(), 255);
}
static uint32_t crc(const uint8_t* bytes, size_t n, uint32_t c = UINT32_MAX) {
  while (n--) { c ^= *bytes++; for (unsigned bit = 0; bit < 8; ++bit) c = (c >> 1) ^ (0xedb88320U & (0U - (c & 1))); }
  return c;
}
static void legacy() {
  reset();
  auto* h = ardfsTestFlash.data();
  ArdFSDetail::put(h, ArdFSDetail::BankMagic); ArdFSDetail::put(h + 4, 1);
  ArdFSDetail::put(h + 8, 17); ArdFSDetail::put(h + 12, FS_PHYS_SIZE / 2);
  ArdFSDetail::put(h + 16, ~crc(h, 16)); ArdFSDetail::put(h + 24, ArdFSDetail::Commit);
  h += 4096;
  ArdFSDetail::put(h, ArdFSDetail::RecordMagic); ArdFSDetail::put(h + 4, 8192);
  ArdFSDetail::put(h + 8, 4); ArdFSDetail::put(h + 12, 6000);
  memcpy(h + 32, "/old", 4);
  for (size_t i = 0; i < 6000; ++i) h[36 + i] = uint8_t(i * 13 + 7);
  ArdFSDetail::put(h + 16, ~crc(h + 32, 6004, crc(h, 16)));
  ArdFSDetail::put(h + 24, ArdFSDetail::Commit);
}
int main() {
  // Version-1 banks remain readable and migrate through the other bank atomically.
  legacy(); const auto oldFormat = ardfsTestFlash;
  int migrationOperations;
  {
    ArdFSVolume v; assert(v.begin()); check(v, "/old", 6000, 7);
    int before = ardfsFlashCalls; assert(save(v, "/new", 13, 3)); migrationOperations = ardfsFlashCalls - before;
    v.end(); assert(v.begin()); check(v, "/old", 6000, 7); check(v, "/new", 13, 3);
  }
  for (int cut = 1; cut <= migrationOperations; ++cut) {
    ardfsTestFlash = oldFormat; ardfsPowerOff = false; ardfsFailAt = -1;
    { ArdFSVolume v; assert(v.begin()); ardfsTornOperation = true; ardfsFailAt = ardfsFlashCalls + cut; save(v, "/new", 13, 3); }
    ardfsPowerOff = false; ardfsFailAt = -1;
    ArdFSVolume v; assert(v.begin()); check(v, "/old", 6000, 7);
    if (v.exists("/new")) check(v, "/new", 13, 3);
  }
  // Irregular append fragments, unknown final length, and a gap in a later sector.
  reset();
  {
    ArdFSVolume v; assert(v.format() && v.begin());
    ESP.badProgramSectorsForTest = {3};
    auto file = v.open("/fragments", "w"); assert(file);
    uint8_t bytes[256]; size_t supplied = 0;
    while (supplied < 10000) {
      int status; do { status = bounded(file, 1); } while (!status); assert(status == 1);
      size_t n = std::min<size_t>(1 + supplied % 255, 10000 - supplied);
      for (size_t i = 0; i < n; ++i) bytes[i] = uint8_t((supplied + i) * 13 + 11);
      assert(file.write(bytes, n) == n); memset(bytes, 0, sizeof(bytes));
      supplied += n; assert(file.position() == supplied);
    }
    assert(file.flushStep() == 0);
    assert(ArdFSWriterAccess::queueWrite(file, bytes, 1) == 0 && file.position() == 10000);
    assert(file.flush() && file.close());
    v.end(); assert(v.begin()); check(v, "/fragments", 10000, 11);
  }
  // A pending/abandoned tail retirement must not change existing file reads.
  reset();
  {
    ArdFSVolume v; assert(v.format() && v.begin()); assert(save(v, "/keep", 6000, 12));
    ESP.badProgramSectorsForTest = {4};
    auto file = v.open("/abandoned", "w", 9000); assert(file);
    uint8_t bytes[256] = {};
    for (size_t at = 0; at < 9000;) {
      size_t n = std::min<size_t>(sizeof(bytes), 9000 - at);
      assert(file.write(bytes, n) == n); at += n;
    }
    int status; do { status = bounded(file, 1); } while (!status); assert(status == 1);
    check(v, "/keep", 6000, 12);
    file.close(); check(v, "/keep", 6000, 12);
    assert(!v.exists("/abandoned") && save(v, "/after-abandon", 13, 3));
    v.end(); assert(v.begin() && !v.exists("/abandoned"));
    check(v, "/keep", 6000, 12); check(v, "/after-abandon", 13, 3);
  }
  // An abandoned published bank must not retain its newer header while records
  // are overwritten. Cover both explicit and silently ignored metadata erases.
  for (bool silent : {false, true}) {
    reset(); ArdFSVolume v; assert(v.format() && v.begin());
    assert(save(v, "/keep", 1, 9) && save(v, "/source", 1, 3));
    auto operation = v.mutate("/source", "/renamed"); assert(operation);
    int status; do { status = operation.prepareStep(); } while (!status); assert(status == 1);
    const size_t bank = FS_PHYS_SIZE / 2;
    while (ArdFSDetail::get(ardfsTestFlash.data() + bank + 24) != ArdFSDetail::Commit)
      assert(operation.flushStep() == 0);
    operation.close();
    auto& failures = silent ? ESP.silentEraseSectorsForTest : ESP.badEraseSectorsForTest;
    failures = {unsigned(bank / 4096), unsigned(bank / 4096 + 1)};
    auto interrupted = v.mutate("/source", "/other"); assert(interrupted);
    const int programs = ardfsPrograms;
    do { status = bounded(interrupted, 0); } while (!status);
    assert(status == -1 && ardfsPrograms == programs);
    interrupted.close(); check(v, "/keep", 1, 9);
    v.end(); failures.clear(); assert(v.begin()); check(v, "/keep", 1, 9);
  }
  // Formatting retires physical faults and can use the second metadata sector.
  reset(); ESP.badEraseSectorsForTest = {0, 3}; ESP.badReadSectorsForTest = {0};
  { ArdFSVolume v; assert(v.format() && v.begin()); assert(save(v, "/payload", 10000, 3)); v.end(); assert(v.begin()); check(v, "/payload", 10000, 3); }

  // Failed format is retryable; corrupt metadata is never treated as blank flash.
  reset();
  {
    ArdFSVolume v; ESP.badProgramSectorsForTest = {0, 1};
    assert(!v.format()); ESP.badProgramSectorsForTest.clear();
    assert(v.format() && v.begin()); v.end();
    ardfsTestFlash[16] ^= 1; ardfsTestFlash[4096 + 16] ^= 1;
    const auto corrupted = ardfsTestFlash;
    assert(!v.begin() && v.lastError() != -84 && ardfsTestFlash == corrupted);
  }
#if FS_PHYS_SIZE >= 524288U
  // The bounded 32-entry map fails safely when a 33rd data sector is bad.
  reset();
  {
    ArdFSVolume v; assert(v.format() && v.begin());
    for (unsigned sector = 2; sector < 34; ++sector) ESP.badProgramSectorsForTest.push_back(sector);
    assert(save(v, "/payload", 1, 3));
    v.end(); assert(v.begin()); check(v, "/payload", 1, 3);
    ESP.badProgramSectorsForTest.push_back(35);
    assert(!save(v, "/overflow", 1, 7));
    v.end(); assert(v.begin()); check(v, "/payload", 1, 3);
  }
#endif
  // Silent erase failures must not resurrect a valid old record on format/append.
  reset();
  {
    ArdFSVolume v; assert(v.format() && v.begin()); assert(save(v, "/stale", 10000, 99));
    ESP.silentEraseSectorsForTest = {2, 3, 4, 5};
    assert(v.format() && v.begin() && v.fileCount() == 0);
    assert(save(v, "/payload", 10000, 8));
    v.end(); assert(v.begin() && !v.exists("/stale")); check(v, "/payload", 10000, 8);
  }
  reset();
  {
    ArdFSVolume v; assert(v.format() && v.begin()); assert(save(v, "/stale", 10000, 99));
    const std::vector<uint8_t> oldRecord(ardfsTestFlash.begin() + 8192, ardfsTestFlash.begin() + 5 * 4096);
    assert(v.format() && v.begin() && v.fileCount() == 0);
    // A stale valid record appears exactly at a future successor marker.
    std::copy(oldRecord.begin(), oldRecord.end(), ardfsTestFlash.begin() + 4 * 4096);
    ESP.silentEraseSectorsForTest = {4, 5};
    assert(save(v, "/payload", 8152, 8));
    v.end(); assert(v.begin() && !v.exists("/stale")); check(v, "/payload", 8152, 8);
  }
  // Retiring the final partial data sector moves the physical end marker too.
  reset();
  {
    ArdFSVolume v; assert(v.format() && v.begin()); assert(save(v, "/stale", 1, 99));
    const std::vector<uint8_t> oldRecord(ardfsTestFlash.begin() + 8192, ardfsTestFlash.begin() + 3 * 4096);
    assert(v.format() && v.begin());
    std::copy(oldRecord.begin(), oldRecord.end(), ardfsTestFlash.begin() + 4 * 4096);
    ESP.badProgramSectorsForTest = {2};
    assert(save(v, "/payload", 1, 8));
    v.end(); assert(v.begin() && !v.exists("/stale")); check(v, "/payload", 1, 8);
  }
  // Recheck the entire payload at commit, including sectors verified much earlier.
  reset();
  {
    ArdFSVolume v; assert(v.format() && v.begin()); assert(save(v, "/keep", 1, 12));
    auto file = v.open("/late", "w", 8200); assert(file);
    uint8_t bytes[256];
    for (size_t at = 0; at < 8200;) {
      size_t n = std::min<size_t>(sizeof(bytes), 8200 - at);
      for (size_t i = 0; i < n; ++i) bytes[i] = uint8_t(at + i);
      assert(file.write(bytes, n) == n); at += n;
    }
    int status; do { status = file.writeStep(); } while (!status); assert(status == 1);
    ardfsTestFlash[3 * 4096 + 32 + 5 + 1] ^= 1;
    assert(!file.flush()); file.close();
    v.end(); assert(v.begin() && !v.exists("/late")); check(v, "/keep", 1, 12);
  }
  // Two gaps in one file, persisted and used by later reads/appends/compaction.
  reset();
  {
    ArdFSVolume v; assert(v.format() && v.begin());
    ESP.badProgramSectorsForTest = {3, 5};
    const size_t capacity = v.totalBytes();
    assert(save(v, "/payload", 10000, 7));
    assert(v.totalBytes() == capacity - 8192);
    check(v, "/payload", 10000, 7);
    v.end(); assert(v.begin()); check(v, "/payload", 10000, 7);
    assert(save(v, "/keep", 1, 99));
    for (unsigned i = 0; i < 20; ++i) {
      assert(save(v, "/payload", 10000, i));
      check(v, "/keep", 1, 99);
      v.end(); assert(v.begin()); check(v, "/payload", 10000, i);
    }
  }
  // Either metadata copy can later become unreadable after an acknowledged save.
  for (unsigned failedHeader : {0U, 1U}) {
    reset(); ArdFSVolume v; assert(v.format() && v.begin());
    ESP.badProgramSectorsForTest = {3}; assert(save(v, "/payload", 10000, 8));
    ESP.badReadSectorsForTest = {failedHeader};
    v.end(); assert(v.begin()); check(v, "/payload", 10000, 8);
  }
  // If neither metadata sector accepts the new map, retain the previous file.
  reset();
  {
    ArdFSVolume v; assert(v.format() && v.begin()); assert(save(v, "/keep", 1, 5));
    ESP.badProgramSectorsForTest = {0, 1, 3};
    assert(!save(v, "/payload", 10000, 8));
    v.end(); assert(v.begin()); check(v, "/keep", 1, 5); assert(!v.exists("/payload"));
  }
  // No spare capacity: do not shift or overwrite an already committed file.
  reset();
  {
    ArdFSVolume v; assert(v.format() && v.begin()); assert(save(v, "/keep", 1, 5));
    ESP.badProgramSectorsForTest = {3};
    assert(!save(v, "/payload", v.totalBytes() - 4096 - 40, 8));
    v.end(); assert(v.begin()); check(v, "/keep", 1, 5);
  }
  // Erase/read failures in unused sectors are skipped too.
  for (unsigned mode = 0; mode < 2; ++mode) {
    reset(); ArdFSVolume v; assert(v.format() && v.begin());
    if (!mode) { ESP.badEraseSectorsForTest = {3}; std::fill_n(ardfsTestFlash.begin() + 3 * 4096, 4096, 0); }
    else ESP.badReadSectorsForTest = {3};
    assert(save(v, "/payload", 10000, 3));
    v.end(); assert(v.begin()); check(v, "/payload", 10000, 3);
  }
  // A failed metadata sector moves publication to the other reserved sector.
  reset();
  {
    ArdFSVolume v; assert(v.format() && v.begin());
    assert(save(v, "/keep", 1, 5));
    ESP.badProgramSectorsForTest = {0, 4};
    assert(save(v, "/payload", 10000, 9));
    ESP.badReadSectorsForTest = {0};
    v.end(); assert(v.begin()); check(v, "/keep", 1, 5); check(v, "/payload", 10000, 9);
  }
  // Every I/O cut, including torn programs, during data relocation + map publication.
  reset();
  { ArdFSVolume v; assert(v.format() && v.begin()); assert(save(v, "/keep", 1, 5)); }
  const auto stable = ardfsTestFlash;
  int operations = 0, totalCuts = 0;
  for (bool failedHeader : {false, true}) {
  ardfsTestFlash = stable;
  ESP.badProgramSectorsForTest = failedHeader ? std::vector<unsigned>{0, 4} : std::vector<unsigned>{4};
  ardfsPowerOff = false; ardfsFailAt = -1;
  { ArdFSVolume v; assert(v.begin()); int before = ardfsFlashCalls; assert(save(v, "/payload", 10000, 9)); operations = ardfsFlashCalls - before; }
  for (bool torn : {false, true}) for (int cut = 1; cut <= operations; ++cut) {
    ardfsTestFlash = stable; ardfsPowerOff = false; ardfsFailAt = -1;
    {
      ArdFSVolume v; assert(v.begin());
      ardfsTornOperation = torn; ardfsFailAt = ardfsFlashCalls + cut;
      save(v, "/payload", 10000, 9);
    }
    ardfsPowerOff = false; ardfsFailAt = -1;
    ArdFSVolume v; assert(v.begin()); check(v, "/keep", 1, 5);
    if (v.exists("/payload")) check(v, "/payload", 10000, 9);
    assert(save(v, "/after", 1, 12)); check(v, "/after", 1, 12);
    ++totalCuts;
  }
  }
  printf("PASS: generation-scoped sector maps, gaps, metadata failover, erase/read retirement, remount/compaction and %d power-cut points with torn operations\n", totalCuts);
}
