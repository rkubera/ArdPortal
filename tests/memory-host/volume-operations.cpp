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
size_t lastAllocationBytes = 0;
unsigned allocationCalls = 0;
void* testAllocate(size_t bytes) noexcept {
  lastAllocationBytes = bytes;
  ++allocationCalls;
  if (allocationCountdown == 0) return nullptr;
  if (allocationCountdown > 0) --allocationCountdown;
  return std::malloc(bytes);
}
static bool mutate(ArdFSVolume& volume, const char* source, const char* destination = nullptr) {
  auto file = volume.mutate(source, destination);
  if (!file) return false;
  int status;
  do { status = file.prepareStep(); } while (!status);
  return status == 1 && file.flush();
}
int main() {
  ArdFSVolume v; assert(v.format() && v.begin());
  { auto f = v.open("/a", "w", 3); const uint8_t b[] = {1,2,3}; assert(f && f.write(b, 3) == 3 && f.flush()); }
  assert(v.fileCount() == 1 && v.usedBytes() == 4096);
  assert(v.freeBytes() + v.usedBytes() == v.totalBytes());
  { auto f = v.open("/a", "r"); uint8_t b; assert(f.seek(2) && f.position() == 2 && f.read(&b, 1) == 1 && b == 3); assert(f.seek(3) && !f.seek(4) && f.position() == 3); }
  auto stable = ardfsTestFlash;
  // Cut power at every flash operation in the atomic rename.
  int operations;
  { int before = ardfsFlashCalls; assert(mutate(v, "/a", "/longer-name")); operations = ardfsFlashCalls - before; }
  v.end(); assert(v.begin() && !v.exists("/a") && v.exists("/longer-name"));
  String name; size_t size = 0;
  assert(v.fileInfo(0, name, size) && name == "/longer-name" && size == 3);
  assert(!v.fileInfo(1, name, size));
  assert(mutate(v, "/longer-name")); v.end(); assert(v.begin() && v.fileCount() == 0 && v.usedBytes() == 0);
  for (int cut = 0; cut < operations; ++cut) {
    ardfsTestFlash = stable; ardfsPowerOff = false; ardfsFailAt = -1;
    { ArdFSVolume x; assert(x.begin()); ardfsFailAt = ardfsFlashCalls + cut + 1; mutate(x, "/a", "/longer-name"); }
    ardfsPowerOff = false; ardfsFailAt = -1;
    ArdFSVolume x; assert(x.begin());
    assert(x.exists("/a") != x.exists("/longer-name"));
    auto f = x.open(x.exists("/a") ? "/a" : "/longer-name", "r");
    uint8_t b[3]; assert(f && f.read(b, 3) == 3 && b[0] == 1 && b[2] == 3);
  }
  ardfsTestFlash = stable;
  { ArdFSVolume x; assert(x.begin()); int before = ardfsFlashCalls; assert(mutate(x, "/a")); operations = ardfsFlashCalls - before; }
  for (int cut = 0; cut < operations; ++cut) {
    ardfsTestFlash = stable; ardfsPowerOff = false; ardfsFailAt = -1;
    { ArdFSVolume x; assert(x.begin()); ardfsFailAt = ardfsFlashCalls + cut + 1; mutate(x, "/a"); }
    ardfsPowerOff = false; ardfsFailAt = -1;
    ArdFSVolume x; assert(x.begin());
    assert(x.fileCount() <= 1);
    if (x.exists("/a")) { auto f = x.open("/a", "r"); uint8_t b[3]; assert(f.read(b, 3) == 3 && b[2] == 3); }
  }
  // Names at the public limit must survive temporary caller buffers and remounts.
  {
    ArdFSVolume x; assert(x.format() && x.begin());
    char name[74]; memset(name, 'a', 72); name[0] = '/'; name[72] = 0;
    const String saved(name);
    auto f = x.open(name, "w", 1, true); assert(f);
    memset(name, 'b', 72);
    int status; do { status = f.prepareStep(); } while (!status);
    uint8_t byte = 91;
    assert(status == 1 && f.write(&byte, 1) == 1 && f.flush()); f.close();
    x.end(); assert(x.begin() && x.exists(saved.c_str()));
    String listed; size_t length = 0;
    assert(x.fileInfo(0, listed, length) && listed == saved && length == 1);
    name[0] = '/'; name[72] = 0;
    const String renamed(name);
    auto operation = x.mutate(saved.c_str(), name); assert(operation);
    memset(name, 'c', 72);
    do { status = operation.prepareStep(); } while (!status);
    assert(status == 1 && operation.flush()); operation.close();
    x.end(); assert(x.begin() && !x.exists(saved.c_str()) && x.exists(renamed.c_str()));
    name[0] = '/'; name[72] = 'c'; name[73] = 0;
    assert(!x.open(name, "w", 1) && !x.mutate(renamed.c_str(), name));
    assert(mutate(x, renamed.c_str())); x.end(); assert(x.begin() && x.fileCount() == 0);
  }
  puts("PASS: mutations, remount, seek, listing, capacity, owned 72-byte paths and rename power cuts");
}
