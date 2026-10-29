#include "ArdJSON.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>

uint32_t fakeMillis = 0;

int main() {
  using ArdJSON::JSON;
  using ArdJSON::JSONVar;
  int64_t signedValue = 0;
  uint64_t unsignedValue = 0;
  assert(JSONVar(INT64_MIN).toInteger(signedValue) && signedValue == INT64_MIN);
  assert(JSONVar(INT64_MAX).toInteger(signedValue) && signedValue == INT64_MAX);
  assert(JSONVar(UINT64_MAX).toUnsignedInteger(unsignedValue) && unsignedValue == UINT64_MAX);
  assert(!JSONVar(UINT64_MAX).toInteger(signedValue));
  assert(!JSONVar(-1).toUnsignedInteger(unsignedValue));
  assert(!JSON.parse("18446744073709551616").toUnsignedInteger(unsignedValue));
  assert(!JSON.parse("-9223372036854775809").toInteger(signedValue));
  assert(!JSONVar(1.5).toInteger(signedValue));
  for (const char* text : { "0.0", "1e0", "1E+2", "-0.1", "-1e2" }) {
    signedValue = 42;
    unsignedValue = 42;
    const auto number = JSON.parse(text);
    assert(!number.toInteger(signedValue) && signedValue == 42);
    assert(!number.toUnsignedInteger(unsignedValue) && unsignedValue == 42);
  }
  assert(!JSONVar(std::numeric_limits<double>::infinity()).isValid());
  assert(!JSONVar(std::numeric_limits<double>::quiet_NaN()).isValid());

  // Preserve the complete 17-digit floating-point wire representation and
  // checked conversion behavior across exponent, sign and mantissa patterns.
  uint64_t bits = 1;
  for (unsigned i = 0; i < 10000; ++i) {
    bits = bits * UINT64_C(6364136223846793005) + 1;
    JSONVar integer(bits);
    assert(integer.toUnsignedInteger(unsignedValue) && unsignedValue == bits);
    signedValue = 42;
    assert(integer.toInteger(signedValue) == (bits <= uint64_t(INT64_MAX)));
    if (bits <= uint64_t(INT64_MAX)) assert(signedValue == int64_t(bits));
    else assert(signedValue == 42);
    double input;
    memcpy(&input, &bits, sizeof(input));
    if (!std::isfinite(input)) continue;
    char expected[32];
    snprintf(expected, sizeof(expected), "%.17g", input);
    JSONVar number(input);
    assert(number.isValid() && JSON.stringify(number) == expected);
    errno = 0;
    char* end = nullptr;
    double reference = strtod(expected, &end);
    const bool valid = errno != ERANGE && end && !*end && std::isfinite(reference);
    double actual = 0;
    assert(number.toDouble(actual) == valid);
    if (valid) assert(!memcmp(&actual, &reference, sizeof(actual)));
  }

  // Exercise every ASCII byte, including NUL, escaped controls and HTML
  // delimiters, through full output, sizing and arbitrary slice boundaries.
  for (bool html : { false, true }) {
    ArdJSON::Limits limits;
    limits.escapeHtml = html;
    for (unsigned c = 0; c < 128; ++c) {
      String input;
      input += char(c);
      char expected[9];
      if (c < 32 || (html && (c == '<' || c == '>' || c == '&')))
        snprintf(expected, sizeof(expected), "\"\\u%04x\"", c);
      else if (c == '"' || c == '\\')
        snprintf(expected, sizeof(expected), "\"\\%c\"", c);
      else snprintf(expected, sizeof(expected), "\"%c\"", c);
      JSONVar value(input);
      String error;
      assert(JSON.stringify(value, false, &error, limits) == expected && !error.length());
      size_t length = strlen(expected);
      assert(JSON.measure(value, &error, limits) == length && !error.length());
      for (size_t at = 0; at <= length; ++at) {
        String slice = JSON.stringifySlice(value, at, 2, &error, limits);
        size_t count = std::min(size_t(2), length - at);
        assert(!error.length() && slice.length() == count);
        assert(!memcmp(slice.c_str(), expected + at, count));
      }
    }
  }
  puts("PASS: exact numeric boundaries, 10000 double patterns and all ASCII/HTML escapes with output slicing");
}
