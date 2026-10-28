#include "Arduino.h"
#include "../../src/ArdFSVolume.h"
#include <cassert>
#include <cstdlib>
uint32_t fakeMillis = 0;
SerialStub Serial;
EspStub ESP;
std::vector<uint8_t> ardfsTestFlash(FS_PHYS_SIZE, 0xff);
int ardfsFlashCalls = 0, ardfsFailAt = -1, ardfsPrograms = 0, ardfsErases = 0;
bool ardfsPowerOff = false, ardfsTornOperation = false;
int allocationCountdown = -1;
size_t lastAllocationBytes = 0, previousAllocationBytes = 0;
unsigned allocationCalls = 0;
void* testAllocate(size_t bytes) noexcept {
  previousAllocationBytes = lastAllocationBytes;
  lastAllocationBytes = bytes;
  ++allocationCalls;
  if (allocationCountdown == 0) return nullptr;
  if (allocationCountdown > 0) --allocationCountdown;
  return std::malloc(bytes);
}
static String path(unsigned i) { return String("/file-") + String(i); }
static void write(ArdFSVolume& volume, unsigned i, uint8_t byte) {
  auto file = volume.open(path(i).c_str(), "w", 1);
  assert(file && file.write(&byte, 1) == 1 && file.flush());
}
static void verify(ArdFSVolume& volume, unsigned updated) {
  for (unsigned i = 0; i < 32; ++i) {
    auto file = volume.open(path(i).c_str(), "r");
    uint8_t b = 0;
    assert(file && file.size() == 1 && file.read(&b, 1) == 1);
    assert(b == uint8_t(i == 7 ? updated : i));
  }
}
int main() {
  {
    ArdFSVolume volume; assert(volume.format() && volume.begin());
    for (unsigned i = 0; i < 32; ++i) write(volume, i, i);
    assert(!volume.open("/overflow", "w", 1));
    // Fill the bank and compact repeatedly while preserving all 32 entries.
    for (unsigned i = 0; i < 100; ++i) write(volume, 7, i);
    verify(volume, 99);
  }
  { ArdFSVolume volume; assert(volume.begin()); verify(volume, 99); }
  // A read must allocate only the small cursor, never a writer's work buffers.
  // Exercise both deletion paths through moves and replacement of an open handle.
  {
    ArdFSVolume volume; assert(volume.begin());
    unsigned calls = allocationCalls;
    auto reader = volume.open("/file-7", "r");
    const size_t readerBytes = lastAllocationBytes;
    assert(reader && allocationCalls == calls + 1 && readerBytes <= 64);
    assert(reader.prepareStep() == 1 && reader.flushStep() == 1);
    uint8_t b = 0;
    assert(reader.write(&b, 1) == 0);
    auto moved = std::move(reader);
    assert(!reader && moved.read(&b, 1) == 1 && b == 99);
    calls = allocationCalls;
    auto writer = volume.open("/file-7", "w", 1, true);
    assert(writer && allocationCalls == calls + 2);
    assert(lastAllocationBytes == 4096 && previousAllocationBytes >= readerBytes + 256);
    assert(writer.read(&b, 1) == -1);
    moved = std::move(writer);
    assert(!writer && moved.close() && moved.close());
    // Closing the moved writer must release the volume's single-writer guard.
    writer = volume.open("/file-7", "w", 1, true);
    assert(writer);
    printf("Handle allocations: reader=%zu B, writer=%zu B + sector buffer=%zu B\n", readerBytes, previousAllocationBytes, lastAllocationBytes);
  }
  const auto stable = ardfsTestFlash;
  // Index allocation failures must leave a mount failure distinguishable from
  // an invalid/empty volume, with no formatting or writes.
  for (int cut = 0; cut < 32; ++cut) {
    ArdFSVolume volume;
    allocationCountdown = cut;
    const int programs = ardfsPrograms, erases = ardfsErases;
    assert(!volume.begin() && volume.lastError() == -12);
    assert(programs == ardfsPrograms && erases == ardfsErases);
    allocationCountdown = -1;
  }
  // Exhaust allocation during writer preparation. No record may be published
  // unless its in-memory index entry was allocated before committing flash.
  for (int cut = 0; cut < 4; ++cut) {
    ardfsTestFlash = stable;
    {
      ArdFSVolume volume; assert(volume.begin());
      allocationCountdown = cut;
      auto file = volume.open(path(7).c_str(), "w", 1);
      if (file) { uint8_t b = 200; if (file.write(&b, 1) == 1) file.flush(); }
      allocationCountdown = -1;
    }
    ArdFSVolume volume; assert(volume.begin());
    auto file = volume.open(path(7).c_str(), "r"); uint8_t b = 0;
    assert(file && file.read(&b, 1) == 1 && (b == 99 || b == 200));
  }
  puts("PASS: 32-entry index, repeated compaction/remount, capacity bound and index allocation failures before flash commit");
}
