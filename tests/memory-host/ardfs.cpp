#include "Arduino.h"
#include "../../src/ArdFS.h"
#include <cassert>
#include <fstream>

uint32_t fakeMillis = 0;
SerialStub Serial;
EspStub ESP;
std::vector<uint8_t> ardfsTestFlash(65536, 0xff);
int ardfsFlashCalls = 0, ardfsFailAt = -1, ardfsPrograms = 0, ardfsErases = 0;
bool ardfsPowerOff = false, ardfsTornOperation = false;

void drain(ArdFS& fs) { for(int i=0;i<2000 && fs.busy();++i) fs.loop(); assert(!fs.busy()); }
void mount(ArdFS& fs) {
  fs.begin(); drain(fs); assert(fs.mounted()); }
String load() {
  ArdFS fs; mount(fs); String value;
  assert(fs.read("/config.json", [&](const ArdFS::Result& r) { assert(r.ok && r.found); value = r.data; }));
  drain(fs); return value;
}
int main() {
  // Formatting a bank must yield between individual sector erases.
  {ArdFSVolume volume;unsigned calls=fakeYieldCalls;assert(volume.format());assert(fakeYieldCalls-calls>=ardfsTestFlash.size()/4096);}

  const String oldValue = "{\"enabled\":true}", newValue = "{\"enabled\":false,\"name\":\"updated\"}";
  {
    ArdFS fs; mount(fs); assert(fs.write("/config.json", oldValue, [](const ArdFS::Result& r) { assert(r.ok && r.changed); })); drain(fs);
  }
  const auto stable = ardfsTestFlash;
  int operationCount;
  {
    ArdFS fs; mount(fs); ardfsFlashCalls = 0;
    assert(fs.writeJson("/config.json",newValue.length(),[&](size_t at,size_t maximum){return newValue.substring(at,at+maximum);}, [](const ArdFS::Result& r) { assert(r.ok && r.changed); })); drain(fs);
    operationCount = ardfsFlashCalls;
  }
  assert(load() == newValue);
  for(bool torn : {false, true}) {
    for(int cut=1;cut<=operationCount;++cut) {
      ardfsTestFlash = stable; ardfsPowerOff = false; ardfsFailAt = -1;
      {
        ArdFS fs; mount(fs); ardfsFlashCalls = 0; ardfsFailAt = cut; ardfsTornOperation = torn;
        assert(fs.writeJson("/config.json",newValue.length(),[&](size_t at,size_t maximum){return newValue.substring(at,at+maximum);}, [](const ArdFS::Result&) {})); drain(fs);
        // Reads/programs/erases remain failed during teardown: no extra commit after power loss.
      }
      ardfsPowerOff = false; ardfsFailAt = -1;
      String recovered = load(); assert(recovered == oldValue || recovered == newValue);
    }
  }
  // Fill the active bank so the next journal save triggers atomic compaction.
  ardfsTestFlash = stable;
  String prior;
  {
    ArdFS fs; mount(fs);
    for(int i=0;i<6;++i){prior="{\"count\":"+String(i)+"}";assert(fs.write("/config.json",prior,[](const ArdFS::Result&r){assert(r.ok);}));drain(fs);}
  }
  auto fullBank=ardfsTestFlash;
  int compactCalls;
  {
    ArdFS fs;mount(fs);ardfsFlashCalls=0;
    assert(fs.write("/config.json",newValue,[](const ArdFS::Result&r){assert(r.ok);}));drain(fs);compactCalls=ardfsFlashCalls;
  }
  assert(load()==newValue);
  for(bool torn:{false,true})for(int cut=1;cut<=compactCalls;++cut){
    ardfsTestFlash=fullBank;ardfsPowerOff=false;ardfsFailAt=-1;
    {ArdFS fs;mount(fs);ardfsFlashCalls=0;ardfsFailAt=cut;ardfsTornOperation=torn;assert(fs.write("/config.json",newValue,[](const ArdFS::Result&){}));drain(fs);}
    ardfsPowerOff=false;ardfsFailAt=-1;String recovered=load();assert(recovered==prior||recovered==newValue);
  }
  ardfsTestFlash = stable;
  {
    ArdFS fs; mount(fs); int writes = ardfsPrograms, erases = ardfsErases;
    assert(fs.write("/config.json", oldValue, [](const ArdFS::Result& r) { assert(r.ok && !r.changed); })); drain(fs);
    assert(ardfsPrograms == writes && ardfsErases == erases && fs.skipped() == 1);
    assert(fs.write("/second.json", "{\"value\":2}", [](const ArdFS::Result& r) { assert(r.ok); })); drain(fs);
    assert(fs.read("/second.json", [](const ArdFS::Result& r) { assert(r.ok && r.data == "{\"value\":2}"); })); drain(fs);
  }
  // Portal path length is not divisible by four; a reboot must preserve it.
  const String snapshot="{\"config\":{\"ssid\":\"saved-network\",\"password\":\"secret\"},\"app\":{}}";
  {ArdFS fs;mount(fs);assert(fs.write("/ardportal.json",snapshot,[](const ArdFS::Result&r){assert(r.ok);}));drain(fs);}
  int erasesBeforeReboot=ardfsErases;
  {ArdFS fs;mount(fs);assert(fs.read("/ardportal.json",[&](const ArdFS::Result&r){assert(r.ok&&r.found&&r.data==snapshot);}));drain(fs);}
  assert(ardfsErases==erasesBeforeReboot);
  printf("PASS: ArdFS custom format/flash adapter, append and compaction, %d raw I/O power-cut points with torn operations, remount/recovery, deduplication and independent files\n", (operationCount+compactCalls) * 2);
}
