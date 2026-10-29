#include "Arduino.h"
#include "ArdFSWriterAccess.h"
#include <cassert>
#include <set>
uint32_t fakeMillis = 0;
SerialStub Serial;
EspStub ESP;
std::vector<uint8_t> ardfsTestFlash(FS_PHYS_SIZE, 255);
int ardfsFlashCalls = 0, ardfsFailAt = -1, ardfsPrograms = 0, ardfsErases = 0;
bool ardfsPowerOff = false, ardfsTornOperation = false;
static uint32_t crc(const uint8_t* h, size_t n, uint32_t c = UINT32_MAX) {
  while (n--) c = ArdCRC::updateByte(c, *h++);
  return c;
}
static void checksum(uint8_t* h, size_t n = 256) {
  ArdFSDetail::put(h + 16, UINT32_MAX);
  ArdFSDetail::put(h + 24, UINT32_MAX);
  ArdFSDetail::put(h + 16, ~crc(h, n));
  ArdFSDetail::put(h + 24, ArdFSDetail::Commit);
}
struct Catalog {
  uint32_t address = 0, generation = 0, revision = 0;
  uint8_t bytes[256] = {};
};
static Catalog newest() {
  Catalog best;
  for (unsigned sector = 0; sector < FS_PHYS_SIZE / 4096; ++sector) {
    const auto* base = ardfsTestFlash.data() + sector * 4096;
    if (ArdFSDetail::get(base) == ArdFSDetail::RecordMagic || ArdFSDetail::get(base) == ArdFSDetail::DataMagic) continue;
    for (unsigned offset = 0; offset < 4096; offset += 256) {
      auto* h = base + offset;
      if (ArdFSDetail::get(h) != ArdFSDetail::BankMagic || ArdFSDetail::get(h + 4) != 4 || ArdFSDetail::get(h + 24) != ArdFSDetail::Commit || ArdFSDetail::get(h + 180) != sector * 4096 + offset) continue;
      uint8_t copy[256];
      memcpy(copy, h, 256);
      checksum(copy);
      if (memcmp(copy, h, 256)) continue;
      uint32_t g = ArdFSDetail::get(h + 8), r = ArdFSDetail::get(h + 20);
      if (int32_t(g - best.generation) > 0 || (g == best.generation && int32_t(r - best.revision) > 0)) {
        best.address = sector * 4096 + offset;
        best.generation = g;
        best.revision = r;
        memcpy(best.bytes, h, 256);
      }
    }
  }
  assert(best.generation);
  return best;
}
static std::set<unsigned> meta(const Catalog& c) {
  std::set<unsigned> ids;
  unsigned bank = c.address / (FS_PHYS_SIZE / 2) * (FS_PHYS_SIZE / 8192);
  for (unsigned i = 0; i < 2; ++i) {
    unsigned id = ArdFSDetail::get16(c.bytes + 176 + i * 2);
    if (id != UINT16_MAX) ids.insert(bank + id);
  }
  return ids;
}
static int step(ArdFSFile& f, unsigned phase) {
  int calls = ardfsFlashCalls, w = ardfsPrograms, e = ardfsErases;
  int status = phase == 0 ? f.prepareStep() : phase == 1 ? f.writeStep()
                                                         : f.flushStep();
  assert(ardfsFlashCalls - calls <= 2);
  if (w != ardfsPrograms || e != ardfsErases) assert(ardfsFlashCalls - calls <= 1);
  return status;
}
static bool save(ArdFSVolume& v, const char* path, const std::vector<uint8_t>& data) {
  auto f = v.open(path, "w", data.size(), true);
  if (!f) return false;
  int status;
  unsigned budget = 0;
  do {
    status = step(f, 0);
    assert(++budget < 10000);
  } while (!status);
  if (status < 0) return false;
  for (size_t at = 0; at < data.size();) {
    do {
      status = step(f, 1);
      assert(++budget < 10000);
    } while (!status);
    if (status < 0) return false;
    size_t n = std::min<size_t>(256, data.size() - at);
    if (ArdFSWriterAccess::queueWrite(f, data.data() + at, n) != n) return false;
    at += n;
  }
  do {
    status = step(f, 2);
    assert(++budget < 10000);
  } while (!status);
  return status == 1;
}
static void check(ArdFSVolume& v, const char* path, const std::vector<uint8_t>& data) {
  auto f = v.open(path, "r");
  assert(f && f.size() == data.size());
  uint8_t bytes[256];
  for (size_t at = 0; at < data.size();) {
    size_t n = std::min<size_t>(256, data.size() - at);
    assert(f.read(bytes, n) == int(n) && !memcmp(bytes, data.data() + at, n));
    at += n;
  }
}
static void legacy() {
  std::fill(ardfsTestFlash.begin(), ardfsTestFlash.end(), 255);
  std::vector<uint8_t> data(6000, 77);
  uint8_t* h = ardfsTestFlash.data() + 8192;
  ArdFSDetail::put(h, ArdFSDetail::RecordMagic);
  ArdFSDetail::put(h + 4, 8192 | 1);
  ArdFSDetail::put(h + 8, 4);
  ArdFSDetail::put(h + 12, 6000);
  ArdFSDetail::put(h + 20, 40);
  ArdFSDetail::put(h + 24, ArdFSDetail::Commit);
  memcpy(h + 32, "/old", 4);
  ArdFSDetail::put16(h + 36, 2);
  ArdFSDetail::put16(h + 38, 3);
  uint8_t generation[4];
  ArdFSDetail::put(generation, 1);
  uint32_t c = crc(h, 16);
  c = crc(generation, 4, c);
  c = crc(h + 20, 4, c);
  c = crc(h + 32, 8, c);
  ArdFSDetail::put(h + 28, ~c);
  ArdFSDetail::put(h + 16, ArdCRC::combine(~c, ~crc(data.data(), data.size()), 6000));
  memcpy(h + 40, data.data(), 4056);
  memcpy(ardfsTestFlash.data() + 3 * 4096, data.data() + 4056, 1944);
  for (unsigned copy = 0; copy < 2; ++copy) {
    h = ardfsTestFlash.data() + copy * 4096;
    ArdFSDetail::put(h, ArdFSDetail::BankMagic);
    ArdFSDetail::put(h + 4, 3);
    ArdFSDetail::put(h + 8, 1);
    ArdFSDetail::put(h + 12, FS_PHYS_SIZE / 2);
    ArdFSDetail::put(h + 20, copy + 1);
    h[32] = h[33] = 0;
    h[34] = 1;
    ArdFSDetail::put16(h + 100, 2);
    checksum(h);
  }
}
int main() {
  ArdFSVolume v;
  assert(v.format() && v.begin());
  const std::vector<uint8_t> cold(9000, 77);
  assert(save(v, "/cold", cold));
  std::set<unsigned> used = meta(newest());
  bool dataInOriginal = false;
  for (unsigned i = 0; i < 360; ++i) {
    assert(save(v, "/hot", { uint8_t(i) }));
    auto ids = meta(newest());
    used.insert(ids.begin(), ids.end());
    auto f = v.open("/hot", "r");
    auto sectors = ArdFSWriterAccess::sectors(f);
    for (auto id : sectors)
      if (id < 2) dataInOriginal = true;
    f.close();
    if (!(i % 7)) {
      v.end();
      assert(v.begin());
      check(v, "/cold", cold);
    }
  }
  assert(used.size() > 8 && dataInOriginal);
  check(v, "/hot", { uint8_t(359) });
  printf("INFO: catalog sectors used across 360 updates: %zu\n", used.size());
  // Damage in the current, relocated catalogs must not expose an older catalog.
  const auto intact = ardfsTestFlash;
  auto selected = newest();
  for (auto id : meta(selected)) ardfsTestFlash[id * 4096 + selected.address % 4096 + 16] ^= 1;
  const auto damaged = ardfsTestFlash;
  v.end();
  assert(!v.begin() && v.lastError() == -5 && ardfsTestFlash == damaged);
  ardfsTestFlash = intact;
  v.end();
  assert(v.begin());
  auto selectedIds = meta(selected);
  ESP.badReadSectorsForTest.assign(selectedIds.begin(), selectedIds.end());
  v.end();
  assert(!v.begin() && v.lastError() == -5 && ardfsTestFlash == intact);
  ESP.badReadSectorsForTest.clear();
  v.end();
  assert(v.begin());

  // Arbitrary payload can contain an address-correct, CRC-valid fake catalog.
  // Sector type framing must prevent it from becoming a mount candidate.
  auto latest = newest();
  std::vector<uint8_t> payload(1000, 19);
  auto file = v.open("/raw", "w", payload.size());
  assert(file);
  unsigned root = ArdFSWriterAccess::dataAddress(file) / 4096;
  uint8_t fake[256];
  memcpy(fake, latest.bytes, 256);
  ArdFSDetail::put(fake + 8, latest.generation + 100);
  unsigned bank = root / (FS_PHYS_SIZE / 8192) * (FS_PHYS_SIZE / 8192), local = root - bank;
  ArdFSDetail::put16(fake + 176, local);
  ArdFSDetail::put16(fake + 178, local ? 0 : 1);
  ArdFSDetail::put(fake + 180, root * 4096 + 256);
  fake[34] = 0;
  checksum(fake);
  unsigned offset = 256 - ArdFSWriterAccess::dataAddress(file) % 4096;
  memcpy(payload.data() + offset, fake, 256);
  assert(file.write(payload.data(), payload.size()) == payload.size() && file.flush());
  file.close();
  v.end();
  assert(v.begin());
  check(v, "/raw", payload);
  check(v, "/cold", cold);
  // A one-bit root-type error must not promote the fake payload catalog.
  const auto goodRaw = ardfsTestFlash;
  v.end();
  ardfsTestFlash[root * 4096] ^= 0x80;
  const auto corruptRaw = ardfsTestFlash;
  assert(v.begin());
  check(v, "/cold", cold);
  assert(!v.exists("/raw") && ardfsTestFlash == corruptRaw);
  v.end();
  ardfsTestFlash = goodRaw;
  assert(v.begin());
  check(v, "/raw", payload);
  // A continuation may use original header sector 0/1. Its payload includes a
  // valid legacy checkpoint at offset 128; even a tail-only torn page is safe.
  std::vector<uint8_t> blob(10000, 17);
  uint8_t oldHeader[256];
  memset(oldHeader, 255, 256);
  ArdFSDetail::put(oldHeader, ArdFSDetail::BankMagic);
  ArdFSDetail::put(oldHeader + 4, 2);
  ArdFSDetail::put(oldHeader + 8, 0x40000000);
  ArdFSDetail::put(oldHeader + 12, FS_PHYS_SIZE / 2);
  ArdFSDetail::put(oldHeader + 20, 1);
  oldHeader[32] = oldHeader[33] = 0;
  checksum(oldHeader, 128);
  size_t firstPayload = 4096 - 32 - 5 - 6;
  ArdFSDetail::put(blob.data() + firstPayload, 2);
  ArdFSDetail::put(blob.data() + firstPayload + 4092, 2);
  memcpy(blob.data() + firstPayload + 124, oldHeader, 128);
  memcpy(blob.data() + firstPayload + 4092 + 124, oldHeader, 128);
  std::vector<uint8_t> framingBefore;
  int framingOps = 0;
  for (unsigned attempt = 0; attempt < 100 && !framingOps; ++attempt) {
    framingBefore = ardfsTestFlash;
    int count = ardfsFlashCalls;
    assert(save(v, "/blob", blob));
    int usedOps = ardfsFlashCalls - count;
    auto f = v.open("/blob", "r");
    auto ids = ArdFSWriterAccess::sectors(f);
    for (unsigned i = 1; i < ids.size(); ++i)
      if (ids[i] < 2) framingOps = usedOps;
  }
  assert(framingOps);
  // The same single-bit damage on a continuation is an error, never legacy mount.
  const auto goodBlob = ardfsTestFlash;
  auto blobFile = v.open("/blob", "r");
  auto blobIds = ArdFSWriterAccess::sectors(blobFile);
  uint32_t blobBank = ArdFSWriterAccess::dataAddress(blobFile) / (FS_PHYS_SIZE / 2) * (FS_PHYS_SIZE / 2);
  blobFile.close();
  v.end();
  unsigned damagedNode = blobIds[1] < 2 ? 1 : 2;
  assert(blobIds[damagedNode] < 2);
  ardfsTestFlash[blobBank + blobIds[damagedNode] * 4096] ^= 0x80;
  const auto corruptBlob = ardfsTestFlash;
  assert(!v.begin() && v.lastError() == -5 && ardfsTestFlash == corruptBlob);
  v.end();
  ardfsTestFlash = goodBlob;
  assert(v.begin());

  for (int cut = 1; cut <= framingOps; ++cut) {
    v.end();
    ardfsTestFlash = framingBefore;
    ardfsPowerOff = false;
    ardfsFailAt = -1;
    assert(v.begin());
    ardfsTornOperation = true;
    ESP.tornTailForTest = true;
    ardfsFailAt = ardfsFlashCalls + cut;
    save(v, "/blob", blob);
    v.end();
    ardfsPowerOff = false;
    ardfsFailAt = -1;
    ESP.tornTailForTest = false;
    assert(v.begin());
    check(v, "/cold", cold);
    if (v.exists("/blob")) check(v, "/blob", blob);
  }
  // Rotation with every interruption, including partial erases/programs.
  v.end();
  assert(v.format() && v.begin());
  assert(save(v, "/cold", cold));
  for (unsigned i = 0; i < 14; ++i) assert(save(v, "/hot", { uint8_t(i) }));
  const auto before = ardfsTestFlash;
  auto oldMeta = meta(newest());
  int calls = ardfsFlashCalls;
  assert(save(v, "/hot", { 14 }));
  int operations = ardfsFlashCalls - calls;
  auto nextMeta = meta(newest());
  assert(oldMeta != nextMeta);
  for (bool torn : { false, true })
    for (int cut = 1; cut <= operations; ++cut) {
      v.end();
      ardfsTestFlash = before;
      ardfsFailAt = -1;
      ardfsPowerOff = false;
      assert(v.begin());
      ardfsTornOperation = torn;
      ardfsFailAt = ardfsFlashCalls + cut;
      save(v, "/hot", { 14 });
      v.end();
      ardfsPowerOff = false;
      ardfsFailAt = -1;
      assert(v.begin());
      check(v, "/cold", cold);
      auto f = v.open("/hot", "r");
      uint8_t value = 0;
      assert(f && f.read(&value, 1) == 1 && (value == 13 || value == 14));
      f.close();
      assert(save(v, "/after", { 99 }));
      v.end();
      assert(v.begin());
      check(v, "/after", { 99 });
    }
  // Program, read, erase and silent erase failures all retire movable metadata.
  for (unsigned mode = 0; mode < 4; ++mode)
    for (unsigned id : nextMeta)
      if (!oldMeta.count(id)) {
        v.end();
        ardfsTestFlash = before;
        assert(v.begin());
        auto& faults = mode == 0 ? ESP.badProgramSectorsForTest : mode == 1 ? ESP.badReadSectorsForTest
                                                                : mode == 2 ? ESP.badEraseSectorsForTest
                                                                            : ESP.silentEraseSectorsForTest;
        faults = { id };
        assert(save(v, "/hot", { 14 }));
        v.end();
        assert(v.begin());
        check(v, "/cold", cold);
        check(v, "/hot", { 14 });
        auto c = newest();
        bool found = false;
        for (unsigned i = 0; i < c.bytes[32]; ++i) found |= ArdFSDetail::get16(c.bytes + 36 + i * 2) == id;
        assert(found && !meta(c).count(id));
        faults.clear();
      }
  // Abandoned entries can fill a peer log while its last committed slot is low.
  // One failed publication must make the next attempt collect instead of looping.
  v.end();
  assert(v.format() && v.begin());
  const std::vector<uint8_t> large(12 * 4090 + 4 - 32 - 6, 55);
  assert(save(v, "/large", large) && save(v, "/hot", { 7 }));
  for (unsigned attempt = 0; attempt < 13; ++attempt) {
    uint32_t seen = newest().revision;
    for (auto id : meta(newest()))
      for (unsigned at = 0; at < 4096; at += 256) {
        const auto* h = ardfsTestFlash.data() + id * 4096 + at;
        if (ArdFSDetail::get(h) == ArdFSDetail::BankMagic) seen = std::max(seen, ArdFSDetail::get(h + 20));
      }
    auto f = v.open("/scratch", "w", 1);
    assert(f);
    uint8_t value = 1;
    assert(f.write(&value, 1) == 1);
    bool written = false;
    unsigned budget = 0;
    while (!written) {
      assert(step(f, 2) == 0 && ++budget < 10000);
      auto ids = meta(newest());
      for (auto id : ids)
        for (unsigned at = 0; at < 4096; at += 256) {
          const auto* h = ardfsTestFlash.data() + id * 4096 + at;
          if (ArdFSDetail::get(h) == ArdFSDetail::BankMagic && ArdFSDetail::get(h + 24) == UINT32_MAX && ArdFSDetail::get(h + 20) > seen) written = true;
        }
    }
    assert(f.close());
  }
  assert(!save(v, "/hot", { 8 }));
  check(v, "/hot", { 7 });
  assert(save(v, "/hot", { 8 }));
  check(v, "/large", large);
  v.end();
  assert(v.begin());
  check(v, "/hot", { 8 });
  check(v, "/large", large);
  // A mirror interrupted before its first entry can later receive append slots.
  // Neither sector needs a committed record (or even a type) at offset zero.
  v.end();
  assert(v.format());
  ArdFSDetail::put16(ardfsTestFlash.data() + 178, 1);
  checksum(ardfsTestFlash.data());
  std::fill_n(ardfsTestFlash.begin() + 4096, 4096, 255);
  assert(v.begin());
  assert(save(v, "/offset", { 22 }));
  v.end();
  std::fill_n(ardfsTestFlash.begin(), 4096, 255);
  assert(v.begin());
  check(v, "/offset", { 22 });
  assert(save(v, "/again", { 23 }));
  v.end();
  assert(v.begin());
  check(v, "/again", { 23 });
  // Unsupported V3 must fail without auto-format or migration writes.
  v.end();
  legacy();
  const auto old = ardfsTestFlash;
  assert(!v.begin() && v.lastError() == -5);
  assert(ardfsTestFlash == old);
  printf("PASS: movable dual catalogs, reclaimed headers, metadata retirement, old-format rejection and %d rotation / %d tail-torn framing cuts\n", operations * 2, framingOps);
}
