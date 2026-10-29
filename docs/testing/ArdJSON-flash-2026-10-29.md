# ArdJSON flash measurements — 2026-10-29

## Retained implementation

- `toInteger()` checks the numeric type directly and uses its checked decimal
  scan to reject fractions/exponents, avoiding three preliminary string searches.
- `toUnsignedInteger()` uses constant quotient/remainder overflow thresholds
  instead of division involving the current digit.
- API, exact integer ranges, floating-point precision, numeric lexemes and
  failed-conversion output semantics are preserved.

## Firmware results

Clean builds of isolated source snapshots, `DynamicPages`, board defaults,
ESP8266 Generic core 3.1.2 and ESP32 Dev Module core 3.3.12.

| Target | Baseline application .bin | Retained .bin | Saved |
| --- | ---: | ---: | ---: |
| ESP8266 | 592400 B | 592384 B | 16 B |
| ESP32 | 1282480 B | 1282480 B | 0 B |

These are application image sizes including padding. The JSON report contains
compiler section sizes, hashes and build logs. These measurements describe this
example; results depend on which JSON operations an application links.

## Rejected experiments

- Replacing JSON control-character `snprintf` with direct hexadecimal encoding
  increased the ESP32 image to 1282592 B.
- A shared signed/unsigned magnitude scanner combined with direct escaping
  produced 592432 B / 1282608 B (ESP8266 / ESP32).
- Broad no-inline attributes on numeric conversions and the double constructor,
  combined with direct escaping, produced 592624 B / 1283056 B.
- Additional no-inline attributes on writer/string output or `toDouble()` and
  constant signed overflow thresholds did not improve the retained result.
- Removing all library `snprintf` calls with a bounded diagnostic formatter and
  direct decimal conversion produced 593664 B / 1284336 B: an increase of
  1264 B / 1856 B. That implementation is not retained.

`snprintf` is used by storage envelopes, identity, timestamps and Zigbee
messages; Zigbee includes `%.7g` conversions. Arduino/SDK dependencies retain
formatted I/O independently of ArdJSON. ESP8266's default `build.float` setting
explicitly links `_printf_float` and `_scanf_float`. Consequently removing the
library's format calls does not remove the corresponding libc machinery in
these builds. `%.17g` and `strtod` remain the JSON double conversions.

## Validation

The numeric regression covers signed/unsigned boundaries, overflow, decimal and
exponent rejection, output preservation on failure, 10000 integer/double bit
patterns, nonfinite values and every ASCII/HTML escape with sliced output.
It runs from `tests/run-config-memory.sh` with the JSON parser/patch tests.

Final focused numeric, patch and stream checks use ASan/UBSan. Formatting and
`git diff --check` are checked. The broad host run cannot pass the Matter CMake
checks or browser JavaScript tests in this environment because `cmake` and
`node` are absent from PATH; those failures are unrelated to the JSON
implementation.

Artifacts: `/tmp/ardjson-flash-20261029`.
