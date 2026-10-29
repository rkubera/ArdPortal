#include "Arduino.h"
#define ARDPORTAL_USE_LITTLEFS 1
#include "ArdPortalStorage.h"
#include <cassert>
uint32_t fakeMillis = 0;
SerialStub Serial;
EspStub ESP;
using Storage = ArdPortalStorage;
using namespace littlefs_test;
void drain(Storage& fs) {
  for (unsigned i = 0; fs.busy() && i < 10000; ++i) {
    fakeMillis += 1000;
    fs.loop();
  }
  assert(!fs.busy());
}
void mount(Storage& fs) {
  fs.begin();
  drain(fs);
  assert(fs.mounted());
}
String collect(Storage& fs, bool repair = false) {
  String result;
  bool done = false;
  assert(fs.readJsonConfig(
    "/config", [&](size_t offset, const uint8_t* p, size_t n) {
      assert(offset == result.length() && n <= 256);
      return result.concat(reinterpret_cast<const char*>(p), n);
    },
    [&](const Storage::Result& r) {
      assert(r.ok && r.found);
      done = true;
    },
    repair));
  drain(fs);
  assert(done);
  return result;
}
void save(Storage& fs, const String& text) {
  bool done = false;
  assert(fs.writeJsonConfig(
    "/config", text.length(), [text](size_t at, size_t n) {
      String part;
      part.concat(text.c_str() + at, n);
      return part;
    },
    [&](const Storage::Result& r) {
      assert(r.ok);
      done = true;
    }));
  assert(!done);
  drain(fs);
  assert(done);
}
int main() {
  const String old = "{\"value\":1}", updated = "{\"value\":2}";
  Storage fs;
  mount(fs);
  assert(!autoFormat);
  bool done = false;
  assert(fs.readJsonConfig(
    "/missing", [](size_t, const uint8_t*, size_t) {
      assert(false);
      return false;
    },
    [&](const Storage::Result& r) {
      assert(r.ok && !r.found);
      done = true;
    },
    true));
  drain(fs);
  assert(done);
  save(fs, old);
  assert(collect(fs, true) == old);
  // Startup repair must produce two valid copies and permit Portal writes.
  assert(fs.readJsonConfig(
    "/config", [](size_t, const uint8_t*, size_t) {
      return true;
    },
    [](const Storage::Result& r) {
      assert(r.ok && r.redundant);
    },
    true));
  drain(fs);
  const auto stable = files;
  int before = calls;
  save(fs, updated);
  int operations = calls - before;
  assert(collect(fs) == updated);
  auto commits = fs.commits();
  save(fs, updated);
  assert(fs.commits() == commits && fs.skipped() == 1);
  // A failed core call before/after each staged replacement preserves one JSON copy.
  for (int cut = 0; cut < operations; ++cut) {
    files = stable;
    off = false;
    failAt = -1;
    {
      Storage s;
      mount(s);
      failAt = calls + cut + 1;
      assert(s.writeJsonConfig(
        "/config", updated.length(), [updated](size_t at, size_t n) {
          String part;
          part.concat(updated.c_str() + at, n);
          return part;
        },
        [](const Storage::Result&) {}));
      drain(s);
    }
    off = false;
    failAt = -1;
    Storage s;
    mount(s);
    String result = collect(s, true);
    assert(result == old || result == updated);
  }
  files = stable;
  // Corrupt one copy; the journal reader falls back and repairs through LittleFS.
  files["/config"][0] = '!';
  {
    Storage s;
    mount(s);
    assert(collect(s, true) == old);
  }
  {
    Storage s;
    mount(s);
    assert(s.readJsonConfig(
      "/config", [](size_t, const uint8_t*, size_t) {
        return true;
      },
      [](const Storage::Result& r) {
        assert(r.ok && r.redundant);
      }));
    drain(s);
  }
  // Legacy plain JSON, raw binary, mutations, metadata and seek.
  files["/config"] = std::vector<uint8_t>(old.c_str(), old.c_str() + old.length());
  files.erase("/config.journal");
  {
    Storage s;
    mount(s);
    assert(collect(s, true) == old);
  }
  assert(fs.writeFile(
    "/raw", 3, [](size_t, size_t n) {
      const char b[] = { 'a', 0, 'z' };
      String out;
      out.concat(b, n);
      return out;
    },
    [](const Storage::Result& r) {
      assert(r.ok);
    }));
  drain(fs);
  {
    ArdLittleFSVolume v;
    assert(v.beginStep() == 1);
    auto f = v.open("/raw", "r");
    uint8_t b;
    assert(f.seek(2) && f.read(&b, 1) == 1 && b == 'z' && !f.seek(4));
  }
  assert(fs.rename("/raw", "/renamed", [](const Storage::Result& r) {
    assert(r.ok);
  }));
  drain(fs);
  assert(fs.remove("/renamed", [](const Storage::Result& r) {
    assert(r.ok);
  }));
  drain(fs);
  assert(!files.count("/raw") && !files.count("/renamed"));
  assert(fs.freeBytes() + fs.usedBytes() == fs.totalBytes());
  assert(fs.fileCount() == 2);
  for (size_t i = 0; i < fs.fileCount(); ++i) {
    String name;
    size_t length;
    assert(fs.fileInfo(i, name, length));
    assert(name == "/config" || name == "/config.journal");
    assert(length > 0);
  }
  assert(maxTransfer <= 256);
  auto saved = files;
  unsigned formatted = formats;
  failMount = true;
  {
    Storage s;
    s.begin();
    drain(s);
    assert(s.ready() && !s.mounted());
  }
  assert(formats == formatted && files == saved);
  failMount = false;
  failFormat = true;
  assert(fs.format([](const Storage::Result& r) {
    assert(!r.ok);
  }));
  drain(fs);
  assert(files == saved);
  failFormat = false;
  assert(fs.format([](const Storage::Result& r) {
    assert(r.ok && r.changed);
  }));
  drain(fs);
  assert(files.empty() && fs.mounted());
  printf("PASS: LittleFS bridge, JSON journal/repair/dedup, legacy/raw/seek/mutations, format/mount errors and %d core-I/O failure points\n", operations);
}
