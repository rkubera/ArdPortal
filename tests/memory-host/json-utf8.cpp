#include "Arduino.h"
#include "../../src/ArdJSON.h"
#include "../../src/JsonStream.h"
#include <cassert>
uint32_t fakeMillis = 0;
int main() {
  ArdJsonStream parser;
  auto ignore = [](ArdJsonStream::Event, uint8_t, unsigned) {
    return true;
  };
  auto accepts = [&](const char* bytes, size_t length) {
    parser.reset();
    parser.input('"', ignore);
    for (size_t i = 0; i < length; ++i) parser.input(uint8_t(bytes[i]), ignore);
    parser.input('"', ignore);
    return parser.finish(ignore);
  };
  // Every Unicode scalar, plus the complete surrogate range and values above
  // U+10FFFF. Quotes/backslashes use the existing JSON escape parity tests.
  for (uint32_t cp = 32; cp <= 0x110010; ++cp) {
    if (cp == '"' || cp == '\\') continue;
    char bytes[4];
    size_t n;
    if (cp < 0x80) {
      n = 1;
      bytes[0] = char(cp);
    } else if (cp < 0x800) {
      n = 2;
      bytes[0] = char(0xc0 | (cp >> 6));
      bytes[1] = char(0x80 | (cp & 63));
    } else if (cp < 0x10000) {
      n = 3;
      bytes[0] = char(0xe0 | (cp >> 12));
      bytes[1] = char(0x80 | ((cp >> 6) & 63));
      bytes[2] = char(0x80 | (cp & 63));
    } else {
      n = 4;
      bytes[0] = char(0xf0 | (cp >> 18));
      bytes[1] = char(0x80 | ((cp >> 12) & 63));
      bytes[2] = char(0x80 | ((cp >> 6) & 63));
      bytes[3] = char(0x80 | (cp & 63));
    }
    const bool expected = cp <= 0x10ffff && !(cp >= 0xd800 && cp <= 0xdfff);
    assert(ArdJSON::validUtf8(String(std::string(bytes, n))) == expected);
    assert(accepts(bytes, n) == expected);
  }
  // All lead/second-byte pairs at each input length exercise truncation,
  // continuation bounds, overlong forms, surrogates and the Unicode ceiling.
  for (unsigned lead = 128; lead < 256; ++lead)
    for (unsigned second = 0; second < 256; ++second) {
      const char bytes[] = { char(lead), char(second), char(0x80), char(0x80) };
      for (size_t n = 1; n <= 4; ++n)
        assert(accepts(bytes, n) == ArdJSON::validUtf8(String(std::string(bytes, n))));
    }
  puts("PASS: all Unicode scalars/surrogates, overlong and truncated UTF-8, every lead/second-byte pair, and U+10FFFF bounds");
}
