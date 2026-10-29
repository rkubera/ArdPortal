#include "Arduino.h"
#include "../../src/ArdFS.h"
#include <cassert>

uint32_t fakeMillis = 0;
SerialStub Serial;
EspStub ESP;
std::vector<uint8_t> ardfsTestFlash(FS_PHYS_SIZE, 0xff);
int ardfsFlashCalls = 0, ardfsFailAt = -1, ardfsPrograms = 0, ardfsErases = 0;
bool ardfsPowerOff = false, ardfsTornOperation = false;

void drain(ArdFS& fs) {
  for (unsigned i = 0; fs.busy() && i < 10000; ++i) {
    const int calls = ardfsFlashCalls, erases = ardfsErases, programs = ardfsPrograms;
    fs.loop();
    {
      // ESP8266 may need two aligned reads for one unaligned 256-byte slice.
      // No loop may scan/verify/copy an entire file or erase multiple sectors.
      assert(ardfsFlashCalls - calls <= 2);
      assert(ardfsErases - erases <= 1);
      assert(ardfsPrograms - programs <= 1);
      if (ardfsErases != erases || ardfsPrograms != programs)
        assert(ardfsFlashCalls - calls <= 1); // No transfer combined with an erase/write.
    }
  }
  assert(!fs.busy());
}
void mount(ArdFS& fs) { fs.begin(); drain(fs); assert(fs.mounted()); }
String bytes(size_t offset, size_t length, uint8_t seed) {
  assert(length <= 256);
  String result;
  assert(result.reserve(length));
  for (size_t i = 0; i < length; ++i) result += char((offset + i + seed) % 256);
  return result;
}
constexpr size_t Length = 40 * 1024 + 3;
constexpr size_t OtherLength = 20 * 1024 + 1;
void verify(uint8_t seed, size_t length = Length) {
  ArdFS fs;
  mount(fs);
  size_t received = 0;
  assert(fs.readFile("/binary", [&](size_t offset, const uint8_t* data, size_t n) {
    assert(offset == received && n <= 256);
    for (size_t i = 0; i < n; ++i) assert(data[i] == uint8_t((offset + i + seed) % 256));
    received += n;
    return true;
  }, [&](const ArdFS::Result& r) { assert(r.ok && r.found); }));
  drain(fs);
  assert(received == length);
}
int main() {
  String::reserveLimitForTest() = 256;
  // A short write stays buffered; close abandons it and preserves the old file.
  {
    ArdFSVolume volume;
    assert(volume.format() && volume.begin());
    const int openCalls = ardfsFlashCalls;
    auto file = volume.open("/direct", "w", 513, true);
    assert(ardfsFlashCalls == openCalls);
    int status = 0;
    unsigned steps = 0;
    while (status == 0) {
      int calls = ardfsFlashCalls, erases = ardfsErases;
      status = file.prepareStep();
      assert(ardfsFlashCalls - calls <= 2 && ardfsErases - erases <= 1);
      ++steps;
    }
    assert(status == 1 && steps > 2);
    assert(file);
    String part = bytes(0, 256, 0);
    int programs = ardfsPrograms;
    assert(file.write(reinterpret_cast<const uint8_t*>(part.c_str()), 256) == 256);
    assert(ardfsPrograms == programs + 1); // Only the sector type is durable; payload remains buffered.
    assert(!file.flush()); // Short producer cannot commit.
    file.close();
    assert(!volume.exists("/direct"));
  }
  {
    ArdFS fs;
    mount(fs);
    const int calls = ardfsFlashCalls;
    assert(fs.writeFile("/binary", Length, [](size_t offset, size_t n) { return bytes(offset, n, 7); },
      [](const ArdFS::Result& r) { assert(r.ok && r.changed); }));
    assert(ardfsFlashCalls == calls); // Queuing performs no flash I/O.
    drain(fs);
  }
  verify(7);
  {
    ArdFS fs;
    mount(fs);
    const auto before = ardfsTestFlash;
    assert(fs.writeFile("/oversized", FS_PHYS_SIZE, [](size_t o, size_t n) { return bytes(o, n, 1); },
      [](const ArdFS::Result& r) { assert(!r.ok); }));
    drain(fs);
    assert(ardfsTestFlash == before);
    // A producer checksum mismatch must prevent the commit marker.
    ArdFSVolume volume;
    assert(volume.begin());
    auto file = volume.open("/corrupt", "w", 256);
    assert(file);
    String part;
    assert(part.reserve(256));
    for (size_t i = 0; i < 256; ++i) part += char(0x5a);
    assert(file.write(reinterpret_cast<const uint8_t*>(part.c_str()), 256) == 256);
    // A permanent program failure exhausts the bounded cached-sector retries.
    ESP.programFailuresForTest = -1;
    assert(!file.flush());
    ESP.programFailuresForTest = 0;
    file.close();
    assert(!volume.exists("/corrupt"));
  }
  const auto stable = ardfsTestFlash;
  // Repeated replacements force bank compaction while retaining another file.
  {
    ArdFS fs;
    mount(fs);
    assert(fs.writeFile("/other", OtherLength, [](size_t o, size_t n) { return bytes(o, n, 5); },
      [](const ArdFS::Result& r) { assert(r.ok); }));
    drain(fs);
    for (uint8_t seed = 8; seed < 12; ++seed) {
      assert(fs.writeFile("/binary", Length, [seed](size_t o, size_t n) { return bytes(o, n, seed); },
        [](const ArdFS::Result& r) { assert(r.ok); }));
      drain(fs);
      verify(seed);
    }
    size_t received = 0;
    assert(fs.readFile("/other", [&](size_t o, const uint8_t* data, size_t n) {
      for (size_t i = 0; i < n; ++i) assert(data[i] == uint8_t(o + i + 5));
      received += n;
      return true;
    }, [](const ArdFS::Result& r) { assert(r.ok); }));
    drain(fs);
    assert(received == OtherLength);
  }
  // Cut every raw I/O during a large replacement that must compact the bank.
  ardfsTestFlash = stable;
  {
    ArdFS fs;
    mount(fs);
    assert(fs.writeFile("/binary", Length, [](size_t o, size_t n) { return bytes(o, n, 8); },
      [](const ArdFS::Result& r) { assert(r.ok); }));
    drain(fs);
  }
  {
    ArdFS fs;
    mount(fs);
    assert(fs.writeFile("/other", OtherLength, [](size_t o, size_t n) { return bytes(o, n, 5); },
      [](const ArdFS::Result& r) { assert(r.ok); }));
    drain(fs);
  }
  const auto fullBank = ardfsTestFlash;
  int calls = 0;
  {
    ArdFS fs;
    mount(fs);
    ardfsFlashCalls = 0;
    assert(fs.writeFile("/binary", Length, [](size_t o, size_t n) { return bytes(o, n, 9); },
      [](const ArdFS::Result& r) { assert(r.ok); }));
    drain(fs);
    calls = ardfsFlashCalls;
  }
  for (bool torn : {false, true}) {
    for (int cut = 1; cut <= calls; ++cut) {
      ardfsTestFlash = fullBank;
      {
        ArdFS fs;
        mount(fs);
        ardfsFlashCalls = 0;
        ardfsFailAt = cut;
        ardfsTornOperation = torn;
        assert(fs.writeFile("/binary", Length, [](size_t o, size_t n) { return bytes(o, n, 9); },
          [](const ArdFS::Result&) {}));
        drain(fs);
      }
      ardfsFailAt = -1;
      ardfsPowerOff = false;
      uint8_t seed = 0;
      {
        ArdFSVolume volume;
        assert(volume.begin());
        auto file = volume.open("/binary", "r");
        assert(file && file.read(&seed, 1) == 1 && (seed == 8 || seed == 9));
      }
      verify(seed);
      {
        ArdFS fs;
        mount(fs);
        size_t received = 0;
        assert(fs.readFile("/other", [&](size_t o, const uint8_t* data, size_t n) {
          assert(o == received);
          for (size_t i = 0; i < n; ++i) assert(data[i] == uint8_t(o + i + 5));
          received += n;
          return true;
        }, [](const ArdFS::Result& r) { assert(r.ok); }));
        drain(fs);
        assert(received == OtherLength);
      }
    }
  }
  ardfsTornOperation = false;
  printf("PASS: large raw replacement/compaction across %d raw I/O power-cut points including torn operations\n", calls * 2);
  // Interrupted direct replacement never hides the previous committed version.
  ardfsTestFlash = stable;
  {
    ArdFS fs;
    mount(fs);
    assert(fs.writeFile("/binary", Length, [](size_t o, size_t n) {
      return o >= 512 ? String() : bytes(o, n, 9);
    }, [](const ArdFS::Result& r) { assert(!r.ok); }));
    drain(fs);
  }
  verify(7);
  {
    ArdFS fs;
    mount(fs);
    assert(fs.writeFile("/binary", 0, [](size_t, size_t) { assert(false); return String(); },
      [](const ArdFS::Result& r) { assert(r.ok); }));
    drain(fs);
  }
  verify(0, 0);
  {
    ArdFS fs; mount(fs);
    bool done = false;
    const size_t initialCount = fs.fileCount();
    const int before = ardfsFlashCalls;
    assert(fs.rename("/binary", "/renamed", [&](const ArdFS::Result& r) { assert(r.ok && r.found && r.changed); done = true; }));
    assert(!done && ardfsFlashCalls == before);
    assert(!fs.remove("/other", [](const ArdFS::Result&) {}));
    drain(fs); assert(done);
    assert(fs.fileCount() == initialCount && fs.freeBytes() + fs.usedBytes() == fs.totalBytes());
    done = false;
    assert(fs.remove("/renamed", [&](const ArdFS::Result& r) { assert(r.ok && r.changed); done = true; }));
    drain(fs); assert(done && fs.fileCount() == initialCount - 1);
    assert(fs.remove("/missing", [](const ArdFS::Result& r) { assert(!r.ok && !r.changed); }));
    drain(fs);
  }
  puts("PASS: raw binary files above JSON/journal limits, bounded I/O per loop, stepped erase/verify/compaction, aborted and empty files");
}
