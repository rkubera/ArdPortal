#include "Arduino.h"
#include "ArdFSWriterAccess.h"
#include <cassert>
#include <set>
uint32_t fakeMillis = 0;
SerialStub Serial;
EspStub ESP;
std::vector<uint8_t> ardfsTestFlash(FS_PHYS_SIZE, 0xff);
int ardfsFlashCalls = 0, ardfsFailAt = -1, ardfsPrograms = 0, ardfsErases = 0;
bool ardfsPowerOff = false, ardfsTornOperation = false;
static int step(ArdFSFile& f, bool flush) {
  int calls = ardfsFlashCalls, writes = ardfsPrograms, erases = ardfsErases;
  int status = flush ? f.flushStep() : f.prepareStep();
  assert(ardfsFlashCalls - calls <= 2);
  if (writes != ardfsPrograms || erases != ardfsErases) assert(ardfsFlashCalls - calls <= 1);
  return status;
}
static bool save(ArdFSVolume& v, const char* path, size_t n, uint8_t value) {
  auto f = v.open(path, "w", n, true);
  if (!f) return false;
  int status;
  do { status = step(f, false); } while (!status);
  if (status < 0) return false;
  uint8_t bytes[256];
  memset(bytes, value, sizeof(bytes));
  while (n) {
    size_t take = std::min(n, sizeof(bytes));
    if (f.write(bytes, take) != take) return false;
    n -= take;
  }
  do { status = step(f, true); } while (!status);
  return status == 1;
}
static uint16_t root(ArdFSVolume& v, const char* path) {
  auto f = v.open(path, "r");
  assert(f);
  return ArdFSWriterAccess::dataAddress(f) / 4096;
}
static void check(ArdFSVolume& v, const char* path, size_t n, uint8_t value) {
  auto f = v.open(path, "r");
  assert(f && f.size() == n);
  uint8_t bytes[256];
  while (n) {
    size_t take = std::min(n, sizeof(bytes));
    assert(f.read(bytes, take) == int(take));
    for (size_t i = 0; i < take; ++i) assert(bytes[i] == value);
    n -= take;
  }
}
static void fresh(ArdFSVolume& v) {
  v.end();
  ESP.badEraseSectorsForTest.clear();
  ESP.badProgramSectorsForTest.clear();
  assert(v.format() && v.begin());
  assert(save(v, "/cold", 9000, 77));
}
int main() {
  ArdFSVolume v;
  if (FS_PHYS_SIZE == 3145728U) {
    assert(v.format() && v.begin());
    assert(save(v, "/cold", 300U * 4092 + 4 - 32 - 5 - 600, 77));
    bool wrapped=false; uint16_t previous=0;
    for (unsigned j=0;j<600;++j) {
      assert(save(v,"/hot",1,j)); uint16_t id=root(v,"/hot");
      assert(id<2 || (id>=302 && id<384));
      if (j && id<previous) wrapped=true;
      previous=id;
    }
    assert(wrapped);
    check(v, "/cold", 300U * 4092 + 4 - 32 - 5 - 600, 77);
    puts("PASS: circular allocation skips 300 occupied sectors across bounded scan steps");
    return 0;
  }
  fresh(v);
  const auto cold = ardfsTestFlash;
  ESP.eraseCountsForTest.assign(FS_PHYS_SIZE / 4096, 0);
  for (unsigned i = 0; i < 11; ++i) {
    assert(save(v, "/hot", 1, i));
    assert(root(v, "/hot") == 5 + i);
    v.end();
    assert(v.begin());  // Cursor must survive every acknowledged write/restart.
  }
  assert(std::equal(cold.begin() + 8192, cold.begin() + 5 * 4096, ardfsTestFlash.begin() + 8192));
  for (unsigned i = 11; i < 360; ++i) assert(save(v, "/hot", 1, i));
  for (unsigned i = 0; i < 32; ++i)
    if (i % 16 >= 2) assert(ESP.eraseCountsForTest[i] > 0);
  check(v, "/cold", 9000, 77);
  check(v, "/hot", 1, uint8_t(359));
  unsigned min = UINT32_MAX, max = 0;
  for (unsigned i = 0; i < 32; ++i)
    if (i % 16 >= 2) {
      min = std::min(min, ESP.eraseCountsForTest[i]);
      max = std::max(max, ESP.eraseCountsForTest[i]);
    }
  printf("INFO: 360 hot updates, data-sector erase counts %u..%u (cold sectors included)\n", min, max);
  // Old readers pin the destination bank: postpone optional collection, keep writing.
  fresh(v);
  auto pinned = v.open("/cold", "r");
  assert(pinned);
  unsigned i = 0;
  while (root(v, "/cold") < 16) {
    assert(++i < 200);
    assert(save(v, "/hot", 1, i));
  }
  for (unsigned j = 0; j < 160; ++j) assert(save(v, "/hot", 1, j));
  assert(root(v, "/cold") >= 16);
  uint8_t byte = 0;
  assert(pinned.read(&byte, 1) == 1 && byte == 77);
  pinned.close();
  assert(save(v, "/hot", 1, 33) && root(v, "/cold") < 16);
  // A worn active bank must remain usable if the other bank has too little capacity.
  fresh(v);
  // Persist an older empty bank with 11 known bad data sectors (3 usable).
  auto* header = ardfsTestFlash.data() + 16 * 4096;
  memcpy(header, ardfsTestFlash.data(), 256);
  ArdFSDetail::put(header + 8, 0);
  ArdFSDetail::put(header + 180,16*4096);
  header[32] = 11;
  for (unsigned j = 0; j < 11; ++j) ArdFSDetail::put16(header + 36 + j * 2, j + 2);
  auto checksum = [](const uint8_t* h) {
    uint32_t c = UINT32_MAX;
    for (unsigned j = 0; j < 256; ++j) {
      c ^= h[j];
      for (unsigned k = 0; k < 8; ++k) c = (c >> 1) ^ (0xedb88320U & (0U - (c & 1)));
    }
    return ~c;
  };
  ArdFSDetail::put(header + 16, UINT32_MAX);
  ArdFSDetail::put(header + 24, UINT32_MAX);
  ArdFSDetail::put(header + 16, checksum(header));
  ArdFSDetail::put(header + 24, ArdFSDetail::Commit);
  v.end();
  assert(v.begin());
  for (unsigned j = 0; j < 170; ++j) assert(save(v, "/hot", 1, j));
  assert(root(v, "/cold") < 16);
  check(v, "/cold", 9000, 77);
  // V4 requires the wear fields; a malformed catalog must not silently reset wear.
  fresh(v);
  for (unsigned at = 0; at < 8192; at += 256) {
    auto* h = ardfsTestFlash.data() + at;
    if (ArdFSDetail::get(h) != ArdFSDetail::BankMagic) continue;
    memset(h + 164, 255, 12);
    ArdFSDetail::put(h + 16, UINT32_MAX);
    ArdFSDetail::put(h + 24, UINT32_MAX);
    ArdFSDetail::put(h + 16, checksum(h));
    ArdFSDetail::put(h + 24, ArdFSDetail::Commit);
  }
  v.end();
  const auto malformed = ardfsTestFlash;
  assert(!v.begin() && v.lastError() == -5);
  assert(ardfsTestFlash == malformed);
  // Interrupted maintenance must retain the old snapshot or the complete new one.
  fresh(v);
  i = 0;
  while (root(v, "/cold") < 16) {
    assert(save(v, "/hot", 1, ++i));
    assert(i < 200);
  }
  // Replay the transaction immediately before the bank switch, injecting every I/O cut.
  fresh(v);
  std::vector<uint8_t> before;
  int operations = 0;
  for (i = 1; i < 200; ++i) {
    before = ardfsTestFlash;
    int calls = ardfsFlashCalls;
    assert(save(v, "/hot", 1, i));
    if (root(v, "/cold") >= 16) {
      operations = ardfsFlashCalls - calls;
      break;
    }
  }
  assert(operations > 0);
  for (bool torn : { false, true })
    for (int cut = 1; cut <= operations; ++cut) {
      v.end();
      ardfsTestFlash = before;
      ardfsPowerOff = false;
      ardfsFailAt = -1;
      assert(v.begin());
      ardfsTornOperation = torn;
      ardfsFailAt = ardfsFlashCalls + cut;
      save(v, "/hot", 1, i);
      v.end();
      ardfsPowerOff = false;
      ardfsFailAt = -1;
      assert(v.begin());
      check(v, "/cold", 9000, 77);
      auto f = v.open("/hot", "r");
      assert(f && f.read(&byte, 1) == 1 && (byte == uint8_t(i) || byte == uint8_t(i - 1)));
      f.close();
      assert(save(v, "/after", 1, 99));
    }
  printf("PASS: rotation/restart, cold-data bank cycling, pinned/capacity deferral, bounded steps and %d maintenance power cuts\n", operations * 2);
}
