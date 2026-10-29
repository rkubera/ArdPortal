#include "Arduino.h"
#include "../../src/ArdFS.h"
#include <cassert>
uint32_t fakeMillis = 0;
SerialStub Serial;
EspStub ESP;
std::vector<uint8_t> ardfsTestFlash(FS_PHYS_SIZE, 0xff);
int ardfsFlashCalls = 0, ardfsFailAt = -1, ardfsPrograms = 0, ardfsErases = 0;
bool ardfsPowerOff = false, ardfsTornOperation = false;
static void advance(ArdFS& fs) {
  int calls = ardfsFlashCalls, programs = ardfsPrograms, erases = ardfsErases;
  fakeMillis += 10;
  fs.loop();
  assert(ardfsFlashCalls - calls <= 2);
  if (programs != ardfsPrograms || erases != ardfsErases) assert(ardfsFlashCalls - calls <= 1);
}
static void drain(ArdFS& fs) {
  unsigned budget = 20000;
  while (fs.busy() && budget--) advance(fs);
  assert(!fs.busy());
}
static uint32_t revision() {
  uint32_t newest = 0;
  for (unsigned at = 0; at < 8192; at += 256) {
    const auto* h = ardfsTestFlash.data() + at;
    if (ArdFSDetail::get(h) == ArdFSDetail::BankMagic && ArdFSDetail::get(h + 24) == ArdFSDetail::Commit)
      newest = std::max(newest, ArdFSDetail::get(h + 20));
  }
  return newest;
}
int main() {
  ArdFS fs;
  fs.begin();
  drain(fs);
  assert(fs.mounted());
  assert(fs.writeFile(
    "/keep", 1, [](size_t, size_t) {
      return String("K");
    },
    [](const auto& r) {
      assert(r.ok);
    }));
  drain(fs);
  uint32_t before = revision();
  bool finished = false;
  assert(fs.writeFile(
    "/next", 1, [](size_t, size_t) {
      return String("N");
    },
    [&](const auto& r) {
      assert(!r.ok);
      finished = true;
    }));
  unsigned budget = 10000;
  while (revision() == before && budget--) advance(fs);
  assert(budget && fs.busy());
  // The first catalog commit reached flash, but no read-back can establish it.
  for (unsigned id = 0; id < FS_PHYS_SIZE / 4096; ++id) ESP.badReadSectorsForTest.push_back(id);
  drain(fs);
  int erases = ardfsErases;
  assert(finished && !fs.mounted());
  int calls = ardfsFlashCalls;
  assert(!fs.writeFile(
    "/blocked", 1, [](size_t, size_t) {
      return String("B");
    },
    [](const auto&) {
      assert(false);
    }));
  assert(ardfsFlashCalls == calls);
  ESP.badReadSectorsForTest.clear();
  fs.begin();
  assert(fs.busy());
  drain(fs);
  assert(fs.mounted() && ardfsErases == erases);  // Recovery never formats existing data.
  bool read = false;
  assert(fs.readFile(
    "/keep", [](size_t at, const uint8_t* p, size_t n) {
      assert(at == 0 && n == 1 && p[0] == 'K');
      return true;
    },
    [&](const auto& r) {
      assert(r.ok && r.found);
      read = true;
    }));
  drain(fs);
  assert(read);
  assert(fs.writeFile(
    "/after", 1, [](size_t, size_t) {
      return String("A");
    },
    [](const auto& r) {
      assert(r.ok);
    }));
  drain(fs);
  puts("PASS: uncertain catalog commit invalidates storage, begin remounts without formatting, and later saves succeed");
}
