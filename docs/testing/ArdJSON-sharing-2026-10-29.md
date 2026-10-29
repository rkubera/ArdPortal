# ArdJSON code sharing measurements — 2026-10-29

The retained implementation prevents inlining of `JSONVar::swap()` and shares
member unlinking between both `remove()` overloads and object patch deletion.
The helper is a header-defined template so Arduino sketches can continue using
the header from multiple translation units. Public API and node layout are unchanged.

Clean Arduino CLI builds of `examples/DynamicPages` used ESP8266 Generic
(`esp8266:esp8266:generic`, core 3.1.2) and ESP32 Dev Module
(`esp32:esp32:esp32`, core 3.3.12). Values below are binary file sizes in bytes.

| Variant | ESP8266 | ESP32 |
| --- | ---: | ---: |
| Baseline | 591840 | 1281984 |
| Shared swap | 591840 | 1281824 |
| Shared member deletion | 591824 | 1281984 |
| Cold, non-inlined failure helper (rejected) | 591840 | 1282000 |
| Retained swap + deletion | 591824 | 1281808 |
| Swap + deletion + non-inlined clear/appendKey (rejected) | 591968 | 1281888 |

The retained combination saves **16 bytes on ESP8266** and **176 bytes on ESP32**.
Compiler-reported static RAM stays at 43716 and 57732 bytes respectively.
Results apply to this example and toolchain; other sketches can link different
subsets of JSON operations.

Validation includes configuration memory tests, LittleFS storage tests, page
stream and runtime state tests. JSON patch, numeric and stream tests also ran
with AddressSanitizer and UndefinedBehaviorSanitizer. Added deletion assertions
cover first/middle/last members, nested values, array indices, missing entries
and allocation failure. Formatting and `git diff --check` pass.

The accompanying JSON records binary hashes, source hashes and local build log
paths under `/tmp/ardjson-sharing2-20261029`. The formatted working header was
checked against the measured snapshot with whitespace ignored.
