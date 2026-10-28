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
  if (bytes == 4096) ++sectorAllocations;
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
static uint8_t value(size_t at, unsigned seed) { return uint8_t(at * 17 + seed); }
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
  assert(file && sectorAllocations == allocations + 1); // Includes any compaction copies.
  if (compact) assert(volume.fileCount() >= 2);
  if (fault == 1) ESP.programFailuresForTest = 1;
  if (fault == 2) ESP.silentProgramFailuresForTest = 1;
  if (fault == 3) { ESP.programFailuresForTest = 1; ESP.eraseFailuresForTest = 1; }
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
    memset(bytes, 0, sizeof(bytes)); // The caller can discard every accepted slice.
    supplied += n;
  }
  int status;
  do { status = step(file, true); } while (!status);
  assert(status == 1 && sectorAllocations == allocations + 1);
  assert(ESP.programFailuresForTest == 0 && ESP.silentProgramFailuresForTest == 0 && ESP.eraseFailuresForTest == 0);
  assert(file.flushStep() == 1 && file.close());
  verify(volume, length, seed);
}
int main() {
  ArdFSVolume volume;
  assert(volume.format() && volume.begin());
  // Allocation failure must not modify flash or acquire the writer guard.
  for (int cut : {0, 1}) {
    const auto before = ardfsTestFlash;
    allocationCountdown = cut;
    assert(!volume.open("/payload", "w", 1, true) && volume.lastError() == -12);
    allocationCountdown = -1;
    assert(before == ardfsTestFlash);
  }
  for (size_t length : {size_t(0), size_t(1), size_t(4056), size_t(4057), size_t(4096), size_t(8193)}) {
    for (int fault = 0; fault <= 3; ++fault) save(volume, length, unsigned(length + fault), fault);
  }
  // Retain another file while replacements force compaction; only one 4 KiB allocation.
  { auto other = volume.open("/other", "w", 1); uint8_t b = 99; assert(other.write(&b, 1) == 1 && other.flush()); }
  for (unsigned i = 0; i < 8; ++i) save(volume, 8193, i, 2, true);
  { auto other = volume.open("/other", "r"); uint8_t b = 0; assert(other.read(&b, 1) == 1 && b == 99); }
  // Permanent failure is bounded and must leave the previous committed version readable.
  {
    auto file = volume.open("/payload", "w", 1);
    uint8_t b = 42; assert(file.write(&b, 1) == 1);
    ESP.programFailuresForTest = -1;
    int status, steps = 0;
    do { status = step(file, true); assert(++steps < 100); } while (!status);
    assert(status == -1);
    ESP.programFailuresForTest = 0;
  }
  verify(volume, 8193, 7);
  volume.end(); assert(volume.begin()); verify(volume, 8193, 7);
  puts("PASS: dynamic shared 4 KiB sector buffer, single-pass input, boundaries, cached retries, bounded I/O, compaction, OOM and permanent failure");
}
