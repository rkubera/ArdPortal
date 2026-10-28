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
void* testAllocate(size_t bytes) noexcept {
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
