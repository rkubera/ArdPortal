#include "Arduino.h"
#include "../../src/ArdFS.h"
#include "ConfigTestIO.h"
#include <cassert>

uint32_t fakeMillis = 0;
SerialStub Serial;
EspStub ESP;
std::vector<uint8_t> ardfsTestFlash(FS_PHYS_SIZE, 0xff);
int ardfsFlashCalls = 0, ardfsFailAt = -1, ardfsPrograms = 0, ardfsErases = 0;
bool ardfsPowerOff = false, ardfsTornOperation = false;

void drain(ArdFS& fs) {
  for (unsigned i = 0; fs.busy() && i < 20000; ++i) {
    fakeMillis += 10;
    const int calls = ardfsFlashCalls, erases = ardfsErases, programs = ardfsPrograms;
    fs.loop();
    {
      assert(ardfsFlashCalls - calls <= 2);
      assert(ardfsErases - erases <= 1 && ardfsPrograms - programs <= 1);
      if (ardfsErases != erases || ardfsPrograms != programs) assert(ardfsFlashCalls - calls <= 1);
    }
  }
  assert(!fs.busy());
}
void mount(ArdFS& fs) { fs.begin(); drain(fs); assert(fs.mounted()); }
constexpr size_t Length = 32 * 1024 + 27;
// No complete JSON string is needed by this source; its length exceeds the old limits.
String document(size_t at, size_t n, uint8_t revision) {
  assert(n <= 256);
  String part;
  assert(part.reserve(n));
  for (size_t i = 0; i < n; ++i) {
    size_t offset = at + i;
    part += offset == 0 || offset == Length - 1 ? '"' : char('a' + (offset + revision) % 26);
  }
  return part;
}
String fullDocument(uint8_t revision) {
  String result;
  assert(result.reserve(Length));
  for (size_t at = 0; at < Length; at += 256) result += document(at, std::min<size_t>(256, Length - at), revision);
  return result;
}
void save(ArdFS& fs, uint8_t revision, bool changed = true) {
  const size_t reserveLimit = String::reserveLimitForTest();
  String::reserveLimitForTest() = 256;
  bool completed = false;
  assert(fs.writeJsonConfig("/large.json", Length, [revision](size_t at, size_t n) { return document(at, n, revision); },
    [&](const auto& r) { assert(r.ok && r.changed == changed); completed = true; }));
  drain(fs);
  assert(completed);
  String::reserveLimitForTest() = reserveLimit;
}
ConfigTestResult load(bool repair = false) {
  ArdFS fs;
  mount(fs);
  ConfigTestResult result;
  bool completed = false;
  assert(readConfigText(fs, "/large.json", [&](const auto& r) { result = r; completed = true; }, repair));
  drain(fs);
  assert(completed);
  return result;
}
void streamLoad(uint8_t revision, bool repair = false, bool expectRepair = false) {
  String::reserveLimitForTest() = 256;
  ArdFS fs;
  mount(fs);
  size_t received = 0;
  bool completed = false;
  assert(fs.readJsonConfig("/large.json", [&](size_t at, const uint8_t* bytes, size_t n) {
    assert(at == received && n <= 256);
    String expected = document(at, n, revision);
    assert(!memcmp(bytes, expected.c_str(), n));
    received += n;
    return true;
  }, [&](const auto& r) {
    assert(r.ok && r.found && r.repaired == expectRepair);
    completed = true;
  }, repair));
  drain(fs);
  assert(completed && received == Length);
  String::reserveLimitForTest() = SIZE_MAX;
}
int main() {
  ArdFS fs;
  mount(fs);
  save(fs, 0);
  save(fs, 1);
  const auto twoCopies = ardfsTestFlash;
  assert(load().data == fullDocument(1));
  assert(load().redundant);
  streamLoad(1);
  // Generated and text-backed producers deduplicate against each other.
  save(fs, 1, false);
  String text = fullDocument(1);
  assert(writeConfigText(fs, "/large.json", text, [](const auto& r) { assert(r.ok && !r.changed); }));
  drain(fs);
  text = fullDocument(2);
  assert(writeConfigText(fs, "/large.json", text, [](const auto& r) { assert(r.ok && r.changed); }));
  drain(fs);
  assert(load().data == text);
  // Sink cancellation reports failure without changing either physical copy.
  {
    const auto before = ardfsTestFlash;
    size_t received = 0;
    assert(fs.readJsonConfig("/large.json", [&](size_t, const uint8_t*, size_t n) { received += n; return false; },
      [](const auto& r) { assert(!r.ok); }));
    drain(fs);
    assert(received > 0 && received <= 256 && ardfsTestFlash == before);
  }
  // Invalid JSON is still refused before changing either journal slot.
  const auto valid = ardfsTestFlash;
  text = "{\"duplicate\":1,\"duplicate\":2}";
  assert(writeConfigText(fs, "/large.json", text, [](const auto& r) { assert(!r.ok); }));
  drain(fs);
  assert(ardfsTestFlash == valid);
  String::reserveLimitForTest() = 256;
  auto outOfMemory = load();
  assert(!outOfMemory.ok && !outOfMemory.data.length() && ardfsTestFlash == valid);
  String::reserveLimitForTest() = SIZE_MAX;

  // A document may fit alone but fail once the surviving journal copy is included.
  // The refusal must happen before erasing either committed record.
  {
    ArdFS writer;
    mount(writer);
    const auto before = ardfsTestFlash;
    constexpr size_t tooLarge = 104 * 1024;
    assert(writer.writeJsonConfig("/large.json", tooLarge, [](size_t at, size_t n) {
      String part;
      assert(part.reserve(n));
      for (size_t i = 0; i < n; ++i)
        part += at + i == 0 || at + i == tooLarge - 1 ? '"' : 'x';
      return part;
    }, [](const auto& r) { assert(!r.ok); }));
    drain(writer);
    assert(ardfsTestFlash == before);
    // Leading JSON whitespace expands sixfold in the journal string; account
    // for that expansion before starting a physical write.
    assert(writer.writeJsonConfig("/large.json", Length, [](size_t at, size_t n) {
      String part;
      assert(part.reserve(n));
      for (size_t i = 0; i < n; ++i) part += at + i == Length - 1 ? '0' : '\n';
      return part;
    }, [](const auto& r) { assert(!r.ok); }));
    drain(writer);
    assert(ardfsTestFlash == before);
  }

  // Damage only the inner journal CRC while recomputing the outer record CRC.
  // This proves selection/fallback independently of the physical volume checksum.
  ardfsTestFlash = twoCopies;
  {
    ArdFSVolume volume;
    assert(volume.begin());
    auto input = volume.open("/large.json.journal", "r");
    assert(input);
    String wrapper;
    assert(wrapper.reserve(input.size()));
    uint8_t chunk[256];
    while (wrapper.length() < input.size()) {
      int n = input.read(chunk, std::min<size_t>(256, input.size() - wrapper.length()));
      assert(n > 0);
      assert(wrapper.concat(reinterpret_cast<const char*>(chunk), n));
    }
    input.close();
    wrapper.replace("generation\":2", "generation\":3");
    auto output = volume.open("/large.json.journal", "w", wrapper.length());
    assert(output);
    for (size_t at = 0; at < wrapper.length(); at += 256) {
      size_t n = std::min<size_t>(256, wrapper.length() - at);
      assert(output.write(reinterpret_cast<const uint8_t*>(wrapper.c_str()) + at, n) == n);
    }
    assert(output.flush());
  }
  streamLoad(0, true, true);
  auto recovered = load();
  assert(recovered.ok && recovered.redundant && recovered.data == fullDocument(0));
  assert(load().data == fullDocument(0));

  // The next save compacts while retaining the other journal slot.
  ardfsTestFlash = twoCopies;
  {
    ArdFS writer;
    mount(writer);
    save(writer, 2);
  }
  const auto fullBank = ardfsTestFlash;
  int calls = 0;
  {
    ArdFS writer;
    mount(writer);
    ardfsFlashCalls = 0;
    save(writer, 3);
    calls = ardfsFlashCalls;
  }
  const String older = fullDocument(2), newer = fullDocument(3);
  for (bool torn : {false, true}) {
    for (int cut = 1; cut <= calls; ++cut) {
      ardfsTestFlash = fullBank;
      {
        ArdFS writer;
        mount(writer);
        ardfsFlashCalls = 0;
        ardfsFailAt = cut;
        ardfsTornOperation = torn;
        assert(writer.writeJsonConfig("/large.json", Length, [](size_t at, size_t n) { return document(at, n, 3); },
          [](const auto&) {}));
        drain(writer);
      }
      ardfsFailAt = -1;
      ardfsPowerOff = false;
      auto result = load();
      assert(result.ok && result.found && (result.data == older || result.data == newer));
    }
  }
  ardfsTornOperation = false;
  printf("PASS: journal JSON above 14/28 KiB, writeJsonConfig/readJsonConfig, deduplication, CRC fallback/repair, OOM safety and %d power-cut points with compaction\n", calls * 2);
}
