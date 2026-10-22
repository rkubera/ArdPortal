#include "ZigbeeConsolePayload.h"
#include <cassert>
#include <string>
#include <vector>
/** @brief Test decoded values and bounded parsing of malformed frames. @return Zero on success. */
int main() {
  std::vector<std::string> lines;
  unsigned sequence = 0;
  auto emit = [&](const char* json, uint8_t seq) {
    lines.emplace_back(json);
    sequence = seq;
    assert(lines.back().size() < 96 && lines.back().front() == '{' && lines.back().back() == '}');
  };
  uint8_t response[] = { 0x18, 0x86, 0x0b, 0x0a, 0 };
  ArdZigbeeConsole::decode(response, sizeof(response), 6, emit);
  assert(sequence == 134 && lines.back() == "{\"cmd\":\"default_response\",\"to\":\"report_attributes\",\"status\":\"SUCCESS\"}");
  uint8_t vendor[] = { 0x1c, 0x34, 0x12, 0x87, 0x0b, 0x0a, 0x86 };
  ArdZigbeeConsole::decode(vendor, sizeof(vendor), 6, emit);
  assert(sequence == 135 && lines.back().find("\"status\":134") != std::string::npos);
  uint8_t report[] = { 0x18, 1, 10, 0x12, 0, 0x29, 0x83, 0xff };
  ArdZigbeeConsole::decode(report, sizeof(report), 0x201, emit);
  assert(lines.back().find("\"value\":-125") != std::string::npos);
  uint8_t color[] = { 0x11, 2, 7, 0x34, 0x12, 0x78, 0x56, 0, 0 };
  ArdZigbeeConsole::decode(color, sizeof(color), 0x300, emit);
  assert(lines.back().find("\"x\":4660,\"y\":22136") != std::string::npos);
  uint8_t on[] = { 0x11, 3, 1 };
  ArdZigbeeConsole::decode(on, sizeof(on), 6, emit);
  assert(lines.back() == "{\"cmd\":\"on\"}");
  uint8_t warning[] = { 0x11, 4, 0, 0xff, 0xff, 0xff, 0xff, 0xff };
  ArdZigbeeConsole::decode(warning, sizeof(warning), 0x502, emit);
  assert(lines.back() == "{\"cmd\":\"warning_flash\",\"duty\":255,\"level\":255}");
  for (unsigned length = 0; length < sizeof(color); ++length) ArdZigbeeConsole::decode(color, length, 0x300, emit);
  uint32_t random = 1;
  uint8_t fuzz[32];
  for (unsigned test = 0; test < 2000; ++test) {
    for (auto& byte : fuzz) {
      random = random * 1664525 + 1013904223;
      byte = random >> 24;
    }
    ArdZigbeeConsole::decode(fuzz, test % 33, test % 2 ? 0x300 : 6, emit);
  }
  puts("PASS: decoded ZCL responses, signed attributes, color commands and truncated/random frames");
}
