#include "Arduino.h"
#include "ArdFSWriterAccess.h"
#include <cassert>
#include <cstdlib>

uint32_t fakeMillis = 0;
SerialStub Serial;
EspStub ESP;
std::vector<uint8_t> ardfsTestFlash(FS_PHYS_SIZE, 0xff);
int ardfsFlashCalls = 0, ardfsFailAt = -1, ardfsPrograms = 0, ardfsErases = 0;
bool ardfsPowerOff = false, ardfsTornOperation = false;
int allocationCountdown = -1;
unsigned sectorAllocations = 0;
void* testAllocate(size_t bytes) noexcept {
  if (allocationCountdown == 0) return nullptr;
  if (allocationCountdown > 0) --allocationCountdown;
  if (bytes == 4096) return nullptr;  // No full-sector allocation is available.
  if (bytes == 512) ++sectorAllocations;
  return std::malloc(bytes);
}
static int step(ArdFSFile& file, bool final) {
  int calls = ardfsFlashCalls, programs = ardfsPrograms, erases = ardfsErases;
  int status = final ? file.flushStep() : file.writeStep();
  assert(ardfsFlashCalls - calls <= 2);
  assert(ardfsPrograms - programs <= 1 && ardfsErases - erases <= 1);
  if (ardfsPrograms != programs || ardfsErases != erases) assert(ardfsFlashCalls - calls <= 1);
  return status;
}
static uint8_t value(size_t at, unsigned seed) {
  return uint8_t(at * 17 + seed);
}
static void verify(ArdFSVolume& volume, size_t length, unsigned seed) {
  auto file = volume.open("/payload", "r");
  assert(file && file.size() == length);
  uint8_t bytes[256];
  for (size_t at = 0; at < length;) {
    size_t n = std::min<size_t>(sizeof(bytes), length - at);
    assert(file.read(bytes, n) == int(n));
    for (size_t i = 0; i < n; ++i) assert(bytes[i] == value(at + i, seed));
    at += n;
  }
}
static void save(ArdFSVolume& volume, size_t length, unsigned seed, int fault, bool compact = false) {
  const unsigned allocations = sectorAllocations;
  auto file = volume.open("/payload", "w", length);
  assert(file && sectorAllocations == allocations + 1);  // Includes any compaction copies.
  if (compact) assert(volume.fileCount() >= 2);
  if (fault == 1) ESP.programFailuresForTest = 1;
  if (fault == 2) ESP.silentProgramFailuresForTest = 1;
  if (fault == 3) {
    ESP.programFailuresForTest = 1;
    ESP.eraseFailuresForTest = 1;
  }
  uint8_t bytes[256];
  assert(ArdFSWriterAccess::queueWrite(file, nullptr, 0) == 0 && file.position() == 0);
  size_t supplied = 0;
  while (supplied < length) {
    int status;
    do { status = step(file, false); } while (!status);
    assert(status == 1);
    size_t n = std::min<size_t>(sizeof(bytes), length - supplied);
    for (size_t i = 0; i < n; ++i) bytes[i] = value(supplied + i, seed);
    const int calls = ardfsFlashCalls;
    assert(ArdFSWriterAccess::queueWrite(file, bytes, n) == n && ardfsFlashCalls == calls);
    assert(ArdFSWriterAccess::queueWrite(file, bytes, n) == 0 && file.position() == supplied + n);
    assert(ArdFSWriterAccess::queueWrite(file, bytes, 257) == 0);
    memset(bytes, 0, sizeof(bytes));  // The caller can discard every accepted slice.
    supplied += n;
  }
  int status;
  do { status = step(file, true); } while (!status);
  assert(status == 1 && sectorAllocations == allocations + 1);
  assert(ESP.programFailuresForTest == 0 && ESP.silentProgramFailuresForTest == 0 && ESP.eraseFailuresForTest == 0);
  assert(file.flushStep() == 1 && file.close());
  verify(volume, length, seed);
}
// A late failure must replay the verified prefix from flash, without calling
// the producer again. A source read failure must preserve the old committed file.
static void lateRelocation(bool unreadable, bool secondDestinationBad) {
  ArdFSVolume volume;
  assert(volume.format() && volume.begin());
  save(volume, 1200, 17, 0);
  auto file = volume.open("/payload", "w", 1200);
  assert(file);
  unsigned source = ArdFSWriterAccess::dataAddress(file) / 4096;
  uint8_t bytes[256];
  for (size_t at = 0; at < 768; at += sizeof(bytes)) {
    for (size_t i = 0; i < sizeof(bytes); ++i) bytes[i] = value(at + i, 31);
    assert(file.write(bytes, sizeof(bytes)) == sizeof(bytes));
  }
  int status;
  do { status = step(file, false); } while (!status);
  assert(status == 1);
  ESP.badProgramSectorsForTest = { source };
  if (secondDestinationBad) ESP.badProgramSectorsForTest.push_back(source + 1);
  if (unreadable) ESP.badReadSectorsForTest = { source };
  for (size_t at = 768; at < 1200;) {
    size_t n = std::min<size_t>(sizeof(bytes), 1200 - at);
    for (size_t i = 0; i < n; ++i) bytes[i] = value(at + i, 31);
    assert(ArdFSWriterAccess::queueWrite(file, bytes, n) == n);
    memset(bytes, 0, sizeof(bytes));
    do { status = step(file, false); } while (!status);
    if (status < 0) break;
    at += n;
  }
  if (status >= 0) do {
      status = step(file, true);
    } while (!status);
  assert((status == -1) == unreadable);
  file.close();
  ESP.badProgramSectorsForTest.clear();
  ESP.badReadSectorsForTest.clear();
  verify(volume, 1200, unreadable ? 17 : 31);
  volume.end();
  assert(volume.begin());
  verify(volume, 1200, unreadable ? 17 : 31);
}
#if FS_PHYS_SIZE >= 2097152U
static void largeMetadata(bool unknownLength) {
  ArdFSVolume volume;
  assert(volume.format() && volume.begin());
  const size_t length = unknownLength ? 5000 : 130 * 4096;
  auto file = volume.open("/payload", "w", unknownLength ? SIZE_MAX : length);
  assert(file);
  const uint32_t root = ArdFSWriterAccess::dataAddress(file) & ~4095U;
  uint8_t bytes[256];
  for (size_t at = 0; at < length;) {
    size_t n = std::min<size_t>(sizeof(bytes), length - at);
    for (size_t i = 0; i < n; ++i) bytes[i] = value(at + i, 43);
    assert(file.write(bytes, n) == n);
    memset(bytes, 0, sizeof(bytes));
    at += n;
  }
  // The root metadata spans multiple cache pages for the known-length case.
  while (ArdFSDetail::get(ardfsTestFlash.data() + root) != ArdFSDetail::RecordMagic) assert(step(file, true) == 0);
  if (!unknownLength) ESP.programFailuresForTest = 2;
  int status;
  do { status = step(file, true); } while (!status);
  assert(status == 1 && ESP.programFailuresForTest == 0);
  file.close();
  verify(volume, length, 43);
  volume.end();
  assert(volume.begin());
  verify(volume, length, 43);
}
#endif
int main() {
  ArdFSVolume volume;
  assert(volume.format() && volume.begin());
  // Allocation failure must not modify flash or acquire the writer guard.
  for (int cut : { 0, 1 }) {
    const auto before = ardfsTestFlash;
    allocationCountdown = cut;
    assert(!volume.open("/payload", "w", 1, true) && volume.lastError() == -12);
    allocationCountdown = -1;
    assert(before == ardfsTestFlash);
  }
  for (size_t length : { size_t(0), size_t(1), size_t(4056), size_t(4057), size_t(4096), size_t(8193) }) {
    for (int fault = 0; fault <= 3; ++fault) save(volume, length, unsigned(length + fault), fault);
  }
  // Retain another file while replacements force compaction; only one shared 512-byte cache/scratch allocation.
  {
    auto other = volume.open("/other", "w", 1);
    uint8_t b = 99;
    assert(other.write(&b, 1) == 1 && other.flush());
  }
  for (unsigned i = 0; i < 8; ++i) save(volume, 8193, i, 2, true);
  {
    auto other = volume.open("/other", "r");
    uint8_t b = 0;
    assert(other.read(&b, 1) == 1 && b == 99);
  }
  // Permanent failure is bounded and must leave the previous committed version readable.
  {
    auto file = volume.open("/payload", "w", 1);
    uint8_t b = 42;
    assert(file.write(&b, 1) == 1);
    ESP.programFailuresForTest = -1;
    int status, steps = 0;
    do {
      status = step(file, true);
      assert(++steps < 2000);
    } while (!status);
    assert(status == -1);
    ESP.programFailuresForTest = 0;
  }
  verify(volume, 8193, 7);
  volume.end();
  assert(volume.begin());
  verify(volume, 8193, 7);
  volume.end();
  lateRelocation(false, false);
  lateRelocation(false, true);
  lateRelocation(true, false);
#if FS_PHYS_SIZE >= 2097152U
  largeMetadata(false);
  largeMetadata(true);
#endif
  puts("PASS: 256-byte cache, no 4 KiB allocation, bounded I/O, compaction, late/repeated relocation and unreadable-prefix rollback");
}
