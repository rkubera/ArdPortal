#include "Arduino.h"
#include <cassert>
uint32_t fakeMillis = 0;
SerialStub Serial;
EspStub ESP;
std::vector<uint8_t> ardfsTestFlash(65536, 0xff);
int ardfsFlashCalls = 0, ardfsFailAt = -1, ardfsPrograms = 0, ardfsErases = 0;
bool ardfsPowerOff = false, ardfsTornOperation = false;
#include "../../examples/AsyncStorage/AsyncStorage.ino"
int main() {
  setup();
  for (unsigned turn = 0; turn < 10000 && (!started || storage.busy()); ++turn) loop();
  assert(started && storage.mounted() && !storage.busy() && storage.commits() == 2);
  bool completed = false;
  String payload;
  assert(storage.readJsonConfig(
    "/example.json", [&](size_t offset, const uint8_t* bytes, size_t n) {
      assert(offset == payload.length() && n <= 256);
      return payload.concat(reinterpret_cast<const char*>(bytes), n);
    },
    [&](const ArdFS::Result& r) {
      assert(r.ok && r.found && r.redundant);
      completed = true;
    }));
  for (unsigned turn = 0; turn < 10000 && storage.busy(); ++turn) loop();
  assert(completed && payload == configJson);
  puts("PASS: AsyncStorage sketch compiles and runs with the streamed config API, repair and readback");
}
