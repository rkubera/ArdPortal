#!/usr/bin/env python3
"""Exercise the actual portal startup scheduler with mock and native storage."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/ArdPortalImpl.h').read_text()
start = source.index('void ArdPortal::serviceStorage(uint32_t now) {')
end = source.index('  _storageMounted = _storage.mounted();', start)
scheduler = source[start:end] + '  (void)now;\n}\n'
mock = r'''
#include <cassert>
#include <cstdint>
uint32_t tick = 0;
uint32_t millis() { return tick; }
struct Storage {
  unsigned calls = 0, remaining = 10000, cost = 0;
  bool* completed = nullptr;
  bool initialized = true, requeue = false;
  void loop() {
    ++calls; tick += cost;
    if (remaining && !--remaining) {
      initialized = true;
      if (completed) *completed = true;
      if (requeue) remaining = 100;
    }
  }
  bool busy() const { return remaining != 0; }
  bool ready() const { return initialized; }
};
struct ArdPortal {
  Storage _storage;
  bool _configurationReady = false;
  void serviceStorage(uint32_t now);
};
'''
mock_main = r'''
int main() {
  ArdPortal fast;
  fast.serviceStorage(tick);
  assert(fast._storage.calls == 64); // Hard bound even when millis does not advance.
  ArdPortal slow;
  slow._storage.cost = 1;
  slow.serviceStorage(tick);
  assert(slow._storage.calls == 4 && tick == 4);
  tick = UINT32_MAX - 1;
  ArdPortal wrap;
  wrap._storage.cost = 1;
  wrap.serviceStorage(tick);
  assert(wrap._storage.calls == 4 && tick == 2);
  ArdPortal done;
  done._storage.remaining = 3;
  done._storage.completed = &done._configurationReady;
  done.serviceStorage(tick);
  assert(done._configurationReady && done._storage.calls == 3);
  done._storage.remaining = 100;
  done.serviceStorage(tick);
  assert(done._storage.calls == 4); // Ordinary transactions stay single-step.
  ArdPortal formatting;
  formatting._configurationReady = true;
  formatting._storage.initialized = false;
  formatting.serviceStorage(tick);
  assert(formatting._storage.calls == 64);
  ArdPortal formatDone;
  formatDone._configurationReady = true;
  formatDone._storage.initialized = false;
  formatDone._storage.remaining = 3;
  formatDone._storage.requeue = true;
  formatDone.serviceStorage(tick);
  assert(formatDone._storage.calls == 3 && formatDone._storage.remaining == 100);
  formatDone.serviceStorage(tick);
  assert(formatDone._storage.calls == 4 && formatDone._storage.remaining == 99);
  ArdPortal idle;
  idle._storage.remaining = 0;
  idle.serviceStorage(tick);
  assert(idle._storage.calls == 1);
}
'''
native = r'''
#include "Arduino.h"
#include "ArdFS.h"
#include <cassert>
uint32_t fakeMillis = 0;
SerialStub Serial;
EspStub ESP;
std::vector<uint8_t> ardfsTestFlash(FS_PHYS_SIZE, 255);
int ardfsFlashCalls = 0, ardfsFailAt = -1, ardfsPrograms = 0, ardfsErases = 0;
bool ardfsPowerOff = false, ardfsTornOperation = false;
struct ArdPortal {
  ArdFS _storage;
  bool _configurationReady = false;
  void serviceStorage(uint32_t now);
};
'''
native_main = r'''
int main() {
  { ArdFSVolume volume; assert(volume.format()); }
  unsigned single = 0;
  {
    ArdFS fs;
    fs.begin();
    while (!fs.ready() && single < 10000) { fs.loop(); ++single; }
    assert(fs.mounted());
  }
  unsigned formatSteps = 0;
  {
    ArdFS fs;
    fs.begin();
    while (!fs.ready()) fs.loop();
    bool complete = false;
    assert(fs.format([&](const ArdFS::Result& result) { assert(result.ok); complete = true; }));
    while (!complete && formatSteps < 30000) { fs.loop(); ++formatSteps; }
    assert(complete);
  }
  ArdPortal portal;
  portal._storage.begin();
  unsigned batched = 0;
  while (!portal._storage.ready() && batched < 1000) {
    fakeMillis += 30; // Model application/UI work between portal calls.
    portal.serviceStorage(fakeMillis);
    ++batched;
  }
  assert(portal._storage.mounted());
  assert(single > 4000 && batched <= (single + 63) / 64);
  assert(portal._storage.readJsonConfig("/config.json",
    [](size_t, const uint8_t*, size_t) { assert(false); return true; },
    [&](const ArdFS::Result& result) {
      assert(result.ok && !result.found);
      portal._configurationReady = true;
    }, true));
  unsigned loads = 0;
  while (!portal._configurationReady && loads++ < 100) portal.serviceStorage(fakeMillis);
  assert(portal._configurationReady);
  bool formatted = false;
  assert(portal._storage.format([&](const ArdFS::Result& result) { assert(result.ok); formatted = true; }));
  unsigned formatCalls = 0;
  while (!formatted && formatCalls < 1000) {
    fakeMillis += 30;
    portal.serviceStorage(fakeMillis);
    ++formatCalls;
  }
  assert(formatted && formatCalls <= (formatSteps + 63) / 64);
  printf("PASS: explicit format/remount: %u single steps -> %u portal calls\n", formatSteps, formatCalls);
  printf("PASS: 1 MiB native mount: %u single steps -> %u portal calls; 30ms loop model %u -> %u ms\n",
         single, batched, single * 30, batched * 30);
}
'''
portal_fixture = native.replace('#include "ArdFS.h"', '#define ARDPORTAL_ENABLE_WIFI 0\n#define ARDPORTAL_ENABLE_DYNAMIC_PAGES 0\n#define ARDPORTAL_ENABLE_OTA 0\n#define ARDPORTAL_ENABLE_POWER_MANAGEMENT 0\n#include "ArdPortal.h"').split('struct ArdPortal {')[0] + '\ntime_t fakeUnixTime = 1800000000;\n'
portal_main = r'''
static unsigned boot(ArdPortal& portal, bool expectedRestored) {
  unsigned callbacks = 0;
  portal.onPortalAndAppConfigReady([&](bool restored) {
    assert(restored == expectedRestored);
    ++callbacks;
  });
  assert(portal.begin());
  unsigned calls = 0;
  while (!portal.portalAndAppConfigReady() && calls < 300) {
    fakeMillis += 30;
    portal.loop();
    ++calls;
  }
  assert(portal.portalAndAppConfigReady() && callbacks == 1);
  portal.onPortalAndAppConfigReady(nullptr);
  return calls;
}
int main() {
  // Real Portal + native ArdFS, including first-use format and JSON reload.
  {
    ArdPortal portal;
    unsigned calls = boot(portal, false);
    assert(calls < 300);
    assert(portal.setAppConfigValue("test", 42));
    portal.flushPortalAndAppConfig();
    unsigned count = 0;
    while (portal.portalAndAppConfigBusy() && count++ < 1000) {
      int before = ardfsFlashCalls;
      portal.loop();
      assert(ardfsFlashCalls - before <= 2); // Normal writes retain per-loop bound.
    }
    assert(!portal.portalAndAppConfigBusy());
    printf("PASS: complete Portal first-use startup in %u calls (30ms loop model)\n", calls);
  }
  {
    ArdPortal portal;
    unsigned calls = boot(portal, true);
    assert(calls < 100 && portal.getAppConfigValue("test").asDouble() == 42);
    printf("PASS: complete Portal reload/repair startup in %u calls\n", calls);
  }
  const auto saved = ardfsTestFlash;
  const int programs = ardfsPrograms, erases = ardfsErases;
  ardfsFailAt = ardfsFlashCalls + 1;
  {
    ArdPortal portal;
    boot(portal, false);
    assert(portal.portalAndAppConfigError().length());
  }
  assert(ardfsTestFlash == saved && ardfsPrograms == programs && ardfsErases == erases);
  ardfsFailAt = -1;
  ardfsPowerOff = false;
  puts("PASS: startup I/O failure preserves saved flash without automatic formatting");
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-startup-') as directory:
    path = Path(directory)
    for name, fixture, main in [('mock', mock, mock_main), ('native', native, native_main), ('portal', portal_fixture, portal_main)]:
        cpp = path / (name + '.cpp')
        cpp.write_text(fixture + (scheduler if name != 'portal' else '') + main)
        args = [os.environ.get('CXX', 'c++'), '-std=c++17', '-O1', '-Wall', '-Wextra', '-Werror',
                '-fsanitize=address,undefined', '-I' + str(root/'tests/memory-host'), '-I' + str(root/'src'), str(cpp)]
        if name != 'mock':
            args += ['-DESP8266', '-DFS_PHYS_SIZE=1048576U', *[str(root/'src'/file) for file in
                      ('ArdStorage.cpp', 'JsonStream.cpp', 'JournalReader.cpp', 'JsonDocumentStream.cpp')]]
        executable = path/name
        subprocess.run([*args, '-o', str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
print('PASS: startup time/step budgets, clock rollover, completion, idle and ordinary-operation guards')
