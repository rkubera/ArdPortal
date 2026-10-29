#include "Arduino.h"
#include "ArdFSWriterAccess.h"
#include <cassert>
uint32_t fakeMillis = 0;
SerialStub Serial;
EspStub ESP;
std::vector<uint8_t> ardfsTestFlash(FS_PHYS_SIZE, 0xff);
int ardfsFlashCalls = 0, ardfsFailAt = -1, ardfsPrograms = 0, ardfsErases = 0;
bool ardfsPowerOff = false, ardfsTornOperation = false;
static uint8_t byte(size_t at, unsigned seed) {
  return uint8_t(at * 17 + at / 101 + seed);
}
static size_t length(unsigned sectors) {
  return sectors * 4092 + 4 - 32 - 2 - sectors * 2;
}
static int step(ArdFSFile& file, unsigned phase) {
  int calls = ardfsFlashCalls, writes = ardfsPrograms, erases = ardfsErases;
  int status = phase == 0 ? file.prepareStep() : phase == 1 ? file.writeStep()
                                                            : file.flushStep();
  assert(ardfsFlashCalls - calls <= 2);
  if (writes != ardfsPrograms || erases != ardfsErases) assert(ardfsFlashCalls - calls <= 1);
  return status;
}
static bool save(ArdFSVolume& v, const char* path, size_t size, unsigned seed) {
  auto f = v.open(path, "w", size, true);
  if (!f) return false;
  int status;
  do { status = step(f, 0); } while (!status);
  if (status < 0) return false;
  uint8_t bytes[256];
  for (size_t at = 0; at < size;) {
    do { status = step(f, 1); } while (!status);
    if (status < 0) return false;
    size_t n = std::min<size_t>(256, size - at);
    for (size_t i = 0; i < n; ++i) bytes[i] = byte(at + i, seed);
    if (ArdFSWriterAccess::queueWrite(f, bytes, n) != n) return false;
    memset(bytes, 0, sizeof(bytes));
    at += n;
  }
  do { status = step(f, 2); } while (!status);
  return status == 1;
}
static bool remove(ArdFSVolume& v, const char* path) {
  auto f = v.mutate(path);
  if (!f) return false;
  int status;
  do { status = step(f, 0); } while (!status);
  if (status < 0) return false;
  do { status = step(f, 2); } while (!status);
  return status == 1;
}
static void check(ArdFSVolume& v, const char* path, size_t size, unsigned seed) {
  auto f = v.open(path, "r");
  assert(f && f.size() == size);
  uint8_t bytes[256];
  for (size_t at = 0; at < size;) {
    size_t n = std::min<size_t>(256, size - at);
    assert(f.read(bytes, n) == int(n));
    for (size_t i = 0; i < n; ++i) assert(bytes[i] == byte(at + i, seed));
    at += n;
  }
  for (size_t at : { size_t(0), size_t(4013), size_t(8091), size - 1 })
    if (at < size) {
      assert(f.seek(at) && f.read(bytes, 1) == 1 && bytes[0] == byte(at, seed));
    }
}
static void setup(ArdFSVolume& v) {
  assert(v.format() && v.begin());
  for (auto name : { "/a", "/b", "/d", "/e" }) assert(save(v, name, length(3), name[1]));
  const auto data = ardfsTestFlash;
  assert(remove(v, "/a") && remove(v, "/d"));
  // Unlink changes only catalog sectors, never B/E or the freed payload sectors.
  assert(std::equal(data.begin() + 8192, data.begin() + FS_PHYS_SIZE / 2, ardfsTestFlash.begin() + 8192));
  assert(v.freeBytes() == 8 * 4096 && v.fileCount() == 2);
}
int main() {
  ArdFSVolume v;
  setup(v);
  const auto stable = ardfsTestFlash;
  int operations;
  {
    int before = ardfsFlashCalls;
    assert(save(v, "/c", length(7), 7));
    operations = ardfsFlashCalls - before;
    auto reader = v.open("/c", "r");
    auto ids = ArdFSWriterAccess::sectors(reader);
    assert(ids.size() == 7 && ids[0] == 14 && ids[1] == 15 && ids[2] == 2 && ids[3] == 3 && ids[6] == 9);
    for (unsigned id : { 5U, 6U, 7U, 11U, 12U, 13U }) assert(std::equal(stable.begin() + id * 4096, stable.begin() + (id + 1) * 4096, ardfsTestFlash.begin() + id * 4096));
  }
  check(v, "/b", length(3), 'b');
  check(v, "/e", length(3), 'e');
  check(v, "/c", length(7), 7);
  v.end();
  assert(v.begin());
  check(v, "/c", length(7), 7);
  assert(save(v, "/after", 1, 11) && save(v, "/after", 1, 12));  // Full bank: atomically collect into the other bank.
  v.end();
  assert(v.begin());
  check(v, "/b", length(3), 'b');
  check(v, "/e", length(3), 'e');
  check(v, "/c", length(7), 7);
  // A bad sector inside a hole must not shift any neighboring live file.
  for (unsigned bad : { 2U, 3U, 9U }) {
    v.end();
    ardfsTestFlash = stable;
    assert(v.begin());
    ESP.badProgramSectorsForTest = { bad };
    assert(save(v, "/c", length(7), 7));
    check(v, "/b", length(3), 'b');
    check(v, "/e", length(3), 'e');
    check(v, "/c", length(7), 7);
    v.end();
    ESP.badProgramSectorsForTest.clear();
    assert(v.begin());
    check(v, "/c", length(7), 7);
  }
  // Root commit failures relocate the retained first sector and update the catalog.
  v.end();
  ardfsTestFlash = stable;
  assert(v.begin());
  {
    auto f = v.open("/root", "w", 1);
    assert(f);
    uint8_t value = 99;
    assert(f.write(&value, 1) == 1);
    uint32_t root = ArdFSWriterAccess::dataAddress(f) / 4096;
    while (ArdFSDetail::get(ardfsTestFlash.data() + root * 4096) != ArdFSDetail::RecordMagic) assert(step(f, 2) == 0);
    ESP.programFailuresForTest = 2;
    int status;
    do { status = step(f, 2); } while (!status);
    assert(status == 1);
    auto r = v.open("/root", "r");
    assert(r && ArdFSWriterAccess::sectors(r)[0] != root);
    uint8_t actual = 0;
    assert(r.read(&actual, 1) == 1 && actual == value);
  }
  v.end();
  assert(v.begin());
  {
    auto f = v.open("/root", "r");
    uint8_t actual = 0;
    assert(f && f.read(&actual, 1) == 1 && actual == 99);
  }
  // A rename must not silently legitimize source corruption introduced after mount.
  v.end();
  ardfsTestFlash = stable;
  assert(v.begin());
  {
    auto source = v.open("/b", "r");
    assert(source);
    uint32_t address = ArdFSWriterAccess::dataAddress(source);
    source.close();
    ardfsTestFlash[address + 1] ^= 1;
    auto operation = v.mutate("/b", "/renamed-b");
    assert(operation);
    int status;
    do { status = step(operation, 0); } while (!status);
    assert(status == -1 && !v.exists("/renamed-b"));
    operation.close();
    check(v, "/e", length(3), 'e');
  }
  // Reader pins retain an unlinked file until its handle is closed.
  v.end();
  ardfsTestFlash = stable;
  assert(v.begin());
  {
    auto pinned = v.open("/b", "r");
    assert(pinned && remove(v, "/b"));
    assert(save(v, "/c", length(7), 7));
    uint8_t bytes[256];
    assert(pinned.read(bytes, 256) == 256);
    for (unsigned i = 0; i < 256; ++i) assert(bytes[i] == byte(i, 'b'));
    v.end();
    assert(v.begin());
    assert(!pinned && pinned.read(bytes, 1) == -1);
  }
  // Publication may be interrupted before or after either catalog commit.
  for (bool torn : { false, true })
    for (int cut = 1; cut <= operations; ++cut) {
      v.end();
      ardfsTestFlash = stable;
      ardfsPowerOff = false;
      ardfsFailAt = -1;
      assert(v.begin());
      ardfsTornOperation = torn;
      ardfsFailAt = ardfsFlashCalls + cut;
      save(v, "/c", length(7), 7);
      v.end();
      ardfsPowerOff = false;
      ardfsFailAt = -1;
      assert(v.begin());
      check(v, "/b", length(3), 'b');
      check(v, "/e", length(3), 'e');
      assert(!v.exists("/a") && !v.exists("/d"));
      if (v.exists("/c")) check(v, "/c", length(7), 7);
      assert(save(v, "/after", 1, 11));
    }
  // Old catalog entries cannot reinterpret a reused root as a different file.
  v.end();
  assert(v.format() && v.begin());
  for (auto name : { "/a", "/b", "/d", "/e" }) assert(save(v, name, length(3), name[1]));
  assert(save(v, "/tail", length(2) - 3, 0));  // Occupy the tail so C reuses A's root.
  assert(remove(v, "/a") && save(v, "/c", length(3), 'c'));
  v.end();
  for (unsigned at = 0; at < 8192; at += 256) {
    auto* h = ardfsTestFlash.data() + at;
    if (ArdFSDetail::get(h) == ArdFSDetail::BankMagic && ArdFSDetail::get(h + 20) > 12)
      ArdFSDetail::put(h + 24, UINT32_MAX);
  }
  const auto damaged = ardfsTestFlash;
  assert(!v.begin() && v.lastError() == -5 && ardfsTestFlash == damaged);
  // All bad data sectors must not underflow the capacity; a healthy other bank can recover.
  v.end();
  assert(v.format() && v.begin());
  for (unsigned id = 2; id < 16; ++id) ESP.badProgramSectorsForTest.push_back(id);
  assert(!save(v, "/dead", 1, 1));
  assert(save(v, "/recovered", 1, 2));
  v.end();
  assert(v.begin());
  check(v, "/recovered", 1, 2);
  v.end();
  ESP.badProgramSectorsForTest.clear();
  assert(v.format() && v.begin());
  for (unsigned id = 0; id < 32; ++id) ESP.badProgramSectorsForTest.push_back(id);
  assert(!save(v, "/dead", 1, 1) && !save(v, "/dead", 1, 1));
  assert(v.totalBytes() == 0 && v.freeBytes() == 0 && v.maxFileBytes(5) == 0);
  assert(!v.open("/dead", "w", size_t(UINT32_MAX)));
  printf("PASS: noncontiguous holes, no B/E rewrite, bad cells in holes, seek, pins, collection and %d power cuts\n", operations * 2);
}
