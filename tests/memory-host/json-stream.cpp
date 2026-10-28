#include "Arduino.h"
#include "../../src/ArdJSON.h"
#include "../../src/JournalReader.h"
#include "../../src/JsonDocumentStream.h"
#include <cassert>
#include "JsonSyntaxOracle.h"
uint32_t fakeMillis = 0;
SerialStub Serial;
EspStub ESP;
bool stream(const String& text) {
  ArdJsonStream parser;
  auto ignore = [](ArdJsonStream::Event, uint8_t, unsigned) { return true; };
  for (size_t i = 0; i < text.length(); ++i) parser.input(uint8_t(text[i]), ignore);
  return parser.finish(ignore);
}
bool dom(const String& text) {
  ArdJSON::Limits limits;
  limits.maxInputBytes = limits.maxStringBytes = 100000;
  limits.maxNodes = 10000;
  return ArdJSON::JSON.parse(text, nullptr, limits).isValid();
}
uint32_t referenceTimes(const uint32_t* matrix, uint32_t value) {
    uint32_t sum = 0;
    for (unsigned i = 0; value; ++i, value >>= 1) if (value & 1) sum ^= matrix[i];
    return sum;
  }

uint32_t referenceCombine(uint32_t left, uint32_t right, size_t bytes) {
    if (!bytes) return left;
    uint32_t odd[32], even[32];
    odd[0] = 0xedb88320U;
    for (unsigned i = 1; i < 32; ++i) odd[i] = uint32_t(1) << (i - 1);
    for (unsigned i = 0; i < 32; ++i) even[i] = referenceTimes(odd, odd[i]);
    for (unsigned i = 0; i < 32; ++i) odd[i] = referenceTimes(even, even[i]);
    do {
      for (unsigned i = 0; i < 32; ++i) even[i] = referenceTimes(odd, odd[i]);
      if (bytes & 1) left = referenceTimes(even, left);
      bytes >>= 1;
      if (!bytes) break;
      for (unsigned i = 0; i < 32; ++i) odd[i] = referenceTimes(even, even[i]);
      if (bytes & 1) left = referenceTimes(odd, left);
      bytes >>= 1;
    } while (bytes);
    return left ^ right;
  }

int main() {
  // Compare the scalar polynomial combiner with the previous independent
  // matrix algorithm, including every length bit and arbitrary CRC states.
  uint32_t random = 0x12345678;
  auto next = [&]() { random = random * 1664525U + 1013904223U; return random; };
  for (unsigned i = 0; i < 1024; ++i) {
    const uint32_t left = next(), right = next();
    size_t length = i < sizeof(size_t) * 8 ? size_t(1) << i : next();
    assert(ArdJournalReader::combine(left, right, length) == referenceCombine(left, right, length));
  }
  assert(ArdJournalReader::combine(123, 456, 0) == 123);
  assert(ArdJournalReader::combine(123, 456, SIZE_MAX) == referenceCombine(123, 456, SIZE_MAX));
  const char* cases[] = {"", " ", "null", "false", "true", "truefalse", "nul", "1", "-0", "1.0e+4", "01", "1.", "1e", "--2", "+1", "-", "1 2", "[]", "{}", "[1,]", "{\"x\":1,}", "{\"x\":1 \"y\":2}", "[1,true,null,{},[\"x\"]]", "{\"a\":1,\"a\":2}", "{\"a\":1,\"\\u0061\":2}", "{\"a\":{\"a\":2}}", "\"\\ud83d\\ude00\"", "\"\\ud83d\"", "\"\\udc00\"", "\"\\x00\"", "\"\\u0000\"", "\"zażółć 😀\"", "{\"a\\u0000b\":0}", "[[]]", "{\"\":0}", "\"\\/\\b\\f\\n\\r\\t\\\"\\\\\""};
  for (const char* text : cases) {
    assert(stream(text) == dom(text));
    ArdJSON::StreamParser builder;
    const String input(text);
    bool accepted = true;
    for (size_t at = 0; at < input.length(); ++at)
      if (!builder.input(at, reinterpret_cast<const uint8_t*>(input.c_str()) + at, 1)) { accepted = false; break; }
    auto value = builder.take();
    assert((accepted && value.isValid()) == dom(input));
    if (value.isValid()) assert(ArdJSON::JSON.stringify(value) == ArdJSON::JSON.stringify(ArdJSON::JSON.parse(input)));
  }
  for (unsigned depth = 0; depth < 22; ++depth) {
    String text;
    for (unsigned i = 0; i < depth; ++i) text += '[';
    text += '0';
    for (unsigned i = 0; i < depth; ++i) text += ']';
    assert(stream(text) == dom(text));
  }
  // Exhaustively mutate a nested document, including control and invalid UTF-8 bytes.
  size_t oracleAt = 0;
  auto oracle = [&]() {
    assert(oracleAt < JsonMutationCount);
    char c = JsonMutationOracle[oracleAt / 4];
    unsigned bits = c <= '9' ? unsigned(c - '0') : unsigned(c - 'a' + 10);
    return bool(bits & (1U << (oracleAt++ % 4)));
  };
  const std::string original = "{\"outer\":[-0.12e+3,true,false,null,{\"unicode\":\"\\ud83d\\ude00\"}]}";
  for (size_t at = 0; at < original.size(); ++at) {
    for (unsigned b = 0; b < 256; ++b) {
      std::string text = original;
      text[at] = char(b);
      assert(stream(String(text)) == oracle());
      assert(stream(String(text)) == dom(String(text)));
    }
    std::string text = original;
    text.erase(at, 1);
    assert(stream(String(text)) == oracle());
    assert(stream(String(text)) == dom(String(text)));
  }
  assert(oracleAt == JsonMutationCount);
  // Reset/destruction must release key metadata in both inline and overflow
  // frames, including unfinished input and duplicate-key failure at depth >16.
  {
    ArdJsonStream parser(32);
    auto ignore = [](ArdJsonStream::Event, uint8_t, unsigned) { return true; };
    std::string prefix;
    for (unsigned i = 0; i < 24; ++i) prefix += "{\"k\":";
    for (unsigned cycle = 0; cycle < 32; ++cycle) {
      for (uint8_t b : prefix) parser.input(b, ignore);
      assert(!parser.failed);
      if (cycle & 1) assert(!parser.finish(ignore));
      parser.reset(32);
      const std::string complete = prefix + "0" + std::string(24, '}');
      for (uint8_t b : complete) parser.input(b, ignore);
      assert(parser.finish(ignore));
      parser.reset(32);
      const std::string duplicate = prefix + "{\"same\":1,\"same\":2}" + std::string(24, '}');
      for (uint8_t b : duplicate) parser.input(b, ignore);
      assert(parser.failed && parser.error == ArdJsonStream::Error::Duplicate);
      parser.reset(32);
    }
    for (uint8_t b : prefix) parser.input(b, ignore); // Destructor owns open keys.
  }
  // The general DOM API preserves its original configurable depth up to 32.
  for (unsigned depth = 17; depth < 35; ++depth) {
    String text;
    for (unsigned i = 0; i < depth; ++i) text += '[';
    text += '0';
    for (unsigned i = 0; i < depth; ++i) text += ']';
    ArdJSON::Limits limits; limits.maxDepth = 32;
    assert(ArdJSON::JSON.parse(text, nullptr, limits).isValid() == (depth <= 32));
  }
  const String payload = "{\"a\":\"😀\\n\"}";
  uint32_t crc = UINT32_MAX;
  for (unsigned i = 0; i < 4; ++i) crc = ArdJournalStream::update(crc, uint8_t(42U >> (i * 8)));
  for (size_t i = 0; i < payload.length(); ++i) crc = ArdJournalStream::update(crc, uint8_t(payload[i]));
  ArdJSON::JSONVar envelope = ArdJSON::JSONVar::object();
  envelope["data"] = payload; envelope["crc32"] = ~crc; envelope["generation"] = 42; envelope["journal"] = 1;
  String wrapper = ArdJSON::JSON.stringify(envelope, true);
  ArdJournalReader reader;
  String decoded;
  auto emit = [&](uint8_t b, size_t at) { assert(at == decoded.length()); decoded += char(b); return true; };
  for (size_t at = 0; at < wrapper.length(); ++at) reader.input(uint8_t(wrapper[at]), emit);
  assert(reader.finish() && !reader.legacy && reader.generation == 42 && decoded == payload);
  // Exact upper uint32 boundaries and non-integer envelope fields. Invalid
  // generation/CRC numbers must fail schema validation rather than wrap.
  auto discard = [](uint8_t, size_t) { return true; };
  for (uint32_t delta = 0; delta < 6; ++delta) {
    const uint32_t generation = UINT32_MAX - delta;
    uint32_t sum = UINT32_MAX;
    for (unsigned i = 0; i < 4; ++i) sum = ArdJournalStream::update(sum, uint8_t(generation >> (8 * i)));
    sum = ArdJournalStream::update(ArdJournalStream::update(sum, '{'), '}');
    const String input = String("{\"journal\":1,\"generation\":") + String(generation) + ",\"crc32\":" + String(~sum) + ",\"data\":\"{}\"}";
    ArdJournalReader candidate;
    for (size_t i = 0; i < input.length(); ++i) candidate.input(uint8_t(input[i]), discard);
    assert(candidate.finish() && candidate.generation == generation);
  }
  for (const char* field : {"generation", "crc32"}) for (const char* number : {"-1", "4294967296", "42949672950", "1.0", "1e0"}) {
    const String input = String("{\"journal\":1,\"data\":\"{}\",\"") + field + "\":" + number + "}";
    ArdJournalReader candidate;
    for (size_t i = 0; i < input.length(); ++i) candidate.input(uint8_t(input[i]), discard);
    assert(!candidate.finish() && !candidate.schema);
  }
  // Application DOM budgets are still enforced while consuming storage slices.
  for (unsigned failure = 0; failure < 4; ++failure) {
    ArdJSON::Limits limits;
    if (failure == 0) limits.maxInputBytes = 3;
    if (failure == 1) limits.maxStringBytes = 1;
    if (failure == 2) limits.maxNodes = 1;
    if (failure == 3) limits.maxDepth = 0;
    ArdJSON::StreamParser builder;
    builder.reset(limits);
    const String input = "{\"nested\":{\"value\":\"hello\"}}";
    assert(!builder.input(0, reinterpret_cast<const uint8_t*>(input.c_str()), input.length()));
    assert(!builder.take().isValid());
  }
  puts("PASS: streamed JSON/DOM syntax parity, mutations, UTF-8, duplicate keys, depth and reordered journal envelopes");
}
