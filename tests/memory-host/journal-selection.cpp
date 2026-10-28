// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#include "Arduino.h"
#include "ArdFSHostVolume.h"
#include <cassert>
#include <functional>
#include <memory>
#define private public
#include "../../src/ArdFS.h"
#undef private
#include "ConfigTestIO.h"
long allocationBudget = -1;
void* testAllocate(size_t bytes) noexcept {
  if (allocationBudget == 0) return nullptr;
  if (allocationBudget > 0) --allocationBudget;
  return std::malloc(bytes);
}
uint32_t fakeMillis = 0;
SerialStub Serial;
EspStub ESP;
bool failMount = false, failFormat = false, failFileWrite = false, failRename = false;
std::map<std::string, std::vector<uint8_t>> fakeFiles;
/** @brief Advance an accepted operation to completion. @param fs Storage instance. @return No value. */
void drain(ArdFS& fs) {
  for (int i = 0; i < 10000 && fs.busy(); ++i) {
    fakeMillis += 10;
    fs.loop();
  }
  assert(!fs.busy());
}
/** @brief Build a valid journal envelope. @param generation Sequence. @param data JSON document. @return Complete record. */
String record(uint32_t generation, const String& data) {
  ArdJSON::JSONVar root = ArdJSON::JSONVar::object();
  root["journal"] = 1;
  root["generation"] = generation;
  root["crc32"] = ArdFS::checksum(generation, data);
  root["data"] = data;
  return ArdJSON::JSON.stringify(root);
}
/** @brief Install a raw journal copy. @param slot Copy index. @param generation Sequence. @param data JSON document. @return No value. */
void install(int slot, uint32_t generation, const String& data) {
  String text = record(generation, data);
  fakeFiles[slot ? "/test.json.journal" : "/test.json"] = { text.c_str(), text.c_str() + text.length() };
}
/** @brief Read fresh storage with an optional allocation failure during validation. @param failSlot Slot to fail, or -1. @return Read result. */
ConfigTestResult load(int failSlot = -1, bool repair = false) {
  ArdFS fs;
  fs.begin();
  drain(fs);
  ConfigTestResult result;
  bool done = false;
  assert(readConfigText(fs,
    "/test.json", [&](const auto& r) {
      result = r;
      done = true;
    },
    repair));
  for (int i = 0; i < 10000 && fs.busy(); ++i) {
    allocationBudget = fs._phase == ArdFS::Phase::Read && fs._slot == failSlot ? 0 : -1;
    fakeMillis += 10;
    fs.loop();
    allocationBudget = -1;
  }
  assert(done && !fs.busy());
  return result;
}
int main() {
  const String older = "{\"relay\":false}", newer = "{\"relay\":true}";
  for (int newest = 0; newest < 2; ++newest) {
    fakeFiles.clear();
    install(1 - newest, 10, older);
    install(newest, 11, newer);
    auto result = load();
    assert(result.ok && result.found && !result.recovered && result.data == newer);
    // Resource failure cannot be treated as corruption and silently restore the older copy.
    auto failed = load(newest);
    if (failed.ok) fprintf(stderr, "BUG: validation OOM returned success with %s\n", failed.data.c_str());
    assert(!failed.ok && !failed.data.length() && failed.error.length());
    assert(load().data == newer);  // Neither copy was overwritten.
    const auto before = fakeFiles;
    ArdFS writer;
    writer.begin();
    drain(writer);
    bool saved = true;
    assert(writeConfigText(writer, "/test.json", "{\"relay\":false,\"revision\":12}", [&](const auto& r) {
      saved = r.ok;
    }));
    for (int i = 0; i < 10000 && writer.busy(); ++i) {
      // The producer is validated in the CRC pass before opening any writer.
      allocationBudget = writer._phase == ArdFS::Phase::SourceCrc ? 0 : -1;
      writer.loop();
      allocationBudget = -1;
    }
    assert(!writer.busy() && !saved && fakeFiles == before);
    // Stream comparison also aborts on allocation failure in the newest slot;
    // it must not mistake an unreadable latest version for a corrupt record.
    saved = true;
    assert(writeConfigText(writer, "/test.json", "{\"relay\":false,\"revision\":12}", [&](const auto& r) { saved = r.ok; }));
    for (int i = 0; i < 10000 && writer.busy(); ++i) {
      String::reserveFailCountdownForTest() = writer._phase == ArdFS::Phase::Read && writer._slot == newest ? 0 : SIZE_MAX;
      writer.loop();
      String::reserveFailCountdownForTest() = SIZE_MAX;
    }
    assert(!writer.busy() && !saved && fakeFiles == before);
  }
  for (int newest = 0; newest < 2; ++newest) {
    fakeFiles.clear();
    install(1 - newest, UINT32_MAX, older);
    install(newest, 0, newer);
    assert(load().data == newer);
    auto& damaged = fakeFiles[newest ? "/test.json.journal" : "/test.json"];
    damaged[damaged.size() - 4] ^= 1;
    auto recovered = load();
    assert(recovered.ok && recovered.recovered && recovered.data == older);
  }
  for (int good = 0; good < 2; ++good)
    for (bool missing : { false, true }) {
      fakeFiles.clear();
      install(good, 42, newer);
      const std::string healthy = good ? "/test.json.journal" : "/test.json";
      if (!missing) fakeFiles[good ? "/test.json" : "/test.json.journal"] = { 'b', 'a', 'd' };
      const auto bytes = fakeFiles[healthy];
      auto repaired = load(-1, true);
      assert(repaired.ok && repaired.repaired && repaired.redundant && repaired.recovered && repaired.data == newer && repaired.repairAttempts == 1);
      assert(fakeFiles[healthy] == bytes);
      const auto stable = fakeFiles;
      const size_t writes = fakeOpenWrites;
      auto healthyRead = load(-1, true);
      assert(healthyRead.ok && healthyRead.redundant && !healthyRead.repaired && healthyRead.data == newer);
      assert(fakeFiles == stable && fakeOpenWrites == writes);
    }
  for (int good = 0; good < 2; ++good) {
    fakeFiles.clear();
    install(good, 42, newer);
    const std::string healthy = good ? "/test.json.journal" : "/test.json";
    const auto bytes = fakeFiles[healthy];
    failFileWrite = true;
    auto failed = load(-1, true);
    failFileWrite = false;
    assert(!failed.ok && failed.found && !failed.redundant && !failed.repaired && failed.repairAttempts == 3 && failed.data == newer && failed.error.length());
    assert(fakeFiles[healthy] == bytes && load().data == newer);
  }
  fakeFiles.clear();
  install(0, 10, older);
  install(1, 11, newer);
  const auto stable = fakeFiles;
  auto healthy = load(-1, true);
  assert(healthy.ok && healthy.redundant && !healthy.repaired && fakeFiles == stable);
  fakeFiles.clear();
  fakeFiles["/test.json"] = { 'b', 'a', 'd' };
  fakeFiles["/test.json.journal"] = { 'b', 'a', 'd' };
  const auto bad = fakeFiles;
  auto noSource = load(-1, true);
  assert(!noSource.ok && !noSource.repaired && fakeFiles == bad);
  // Legacy objects and noncanonical envelopes use the same bounded reader/repair.
  fakeFiles.clear();
  const String legacy = "{\"legacy\":\"payload\"}";
  fakeFiles["/test.json"] = {legacy.c_str(), legacy.c_str() + legacy.length()};
  auto legacyRepaired = load(-1, true);
  assert(legacyRepaired.ok && legacyRepaired.repaired && legacyRepaired.redundant && legacyRepaired.data == legacy);
  assert(fakeFiles["/test.json"] == fakeFiles["/test.json.journal"]);
  fakeFiles.clear();
  ArdJSON::JSONVar noncanonical = ArdJSON::JSONVar::object();
  noncanonical["data"] = newer;
  noncanonical["crc32"] = ArdFS::checksum(91, newer);
  noncanonical["generation"] = 91;
  noncanonical["journal"] = 1;
  String wrapper = ArdJSON::JSON.stringify(noncanonical, true);
  fakeFiles["/test.json.journal"] = {wrapper.c_str(), wrapper.c_str() + wrapper.length()};
  ArdFS streaming;
  streaming.begin(); drain(streaming);
  const auto verifiedSource = fakeFiles["/test.json.journal"];
  String delivered;
  assert(streaming.readJsonConfig("/test.json", [&](size_t at, const uint8_t* bytes, size_t n) {
    assert(at == delivered.length() && n <= 256);
    return delivered.concat(reinterpret_cast<const char*>(bytes), n);
  }, [](const auto& r) { assert(r.ok && r.repaired && r.redundant); }, true));
  drain(streaming);
  assert(delivered == newer && fakeFiles["/test.json.journal"] == verifiedSource);
  fakeFiles.erase("/test.json");
  failFileWrite = true;
  delivered = String();
  assert(streaming.readJsonConfig("/test.json", [&](size_t at, const uint8_t* bytes, size_t n) {
    assert(at == delivered.length());
    return delivered.concat(reinterpret_cast<const char*>(bytes), n);
  }, [](const auto& r) { assert(!r.ok && r.found && !r.repaired && r.repairAttempts == 3); }, true));
  drain(streaming);
  failFileWrite = false;
  assert(delivered == newer && fakeFiles["/test.json.journal"] == verifiedSource);
  // A legacy document can fit physically while its canonical escaped form
  // would not. An unchanged write must still validate and deduplicate it.
  fakeFiles.clear();
  String largeLegacy(std::string(200000, '\n') + "{}");
  fakeFiles["/test.json"] = {largeLegacy.c_str(), largeLegacy.c_str() + largeLegacy.length()};
  ArdFS deduplicating;
  deduplicating.begin(); drain(deduplicating);
  const size_t opens = fakeOpenWrites;
  bool completed = false;
  assert(writeConfigText(deduplicating, "/test.json", largeLegacy, [&](const auto& r) {
    assert(r.ok && !r.changed); completed = true;
  }));
  drain(deduplicating);
  assert(completed && deduplicating.skipped() == 1 && fakeOpenWrites == opens);
  // Bad syntax is rejected without opening or changing either destination.
  const auto unchanged = fakeFiles;
  completed = false;
  assert(writeConfigText(deduplicating, "/test.json", "{broken}", [&](const auto& r) {
    assert(!r.ok); completed = true;
  }));
  drain(deduplicating);
  assert(completed && fakeFiles == unchanged && fakeOpenWrites == opens);
  puts("PASS: newest journal in either slot, sequence rollover, real corruption recovery and allocation failure without stale rollback");
}
