#pragma once
#include "ArdFS.h"
#include <memory>
// Test fixtures explicitly own/collect strings; production ArdFS exposes streams only.
struct ConfigTestResult : ArdFS::Result {
  String data;
};
inline bool writeConfigText(ArdFS& fs, const String& path, String json, ArdFS::Callback callback) {
  const size_t length = json.length();
  return fs.writeJsonConfig(
    path, length, [json](size_t offset, size_t maximum) {
      String part;
      if (!part.reserve(maximum) || !part.concat(json.c_str() + offset, maximum)) return String();
      return part;
    },
    std::move(callback));
}
inline bool readConfigText(ArdFS& fs, const String& path, std::function<void(const ConfigTestResult&)> callback, bool repair = false) {
  struct State {
    String text;
    bool failed = false;
  };
  auto state = std::make_shared<State>();
  return fs.readJsonConfig(
    path, [state](size_t offset, const uint8_t* bytes, size_t n) {
      if (offset != state->text.length() || !state->text.reserve(offset + n) || !state->text.concat(reinterpret_cast<const char*>(bytes), n)) {
        state->failed = true;
        return false;
      }
      return true;
    },
    [state, callback](const ArdFS::Result& result) {
      ConfigTestResult collected;
      static_cast<ArdFS::Result&>(collected) = result;
      if (!state->failed && (result.ok || result.repairAttempts == 3)) collected.data = std::move(state->text);
      callback(collected);
    },
    repair);
}
