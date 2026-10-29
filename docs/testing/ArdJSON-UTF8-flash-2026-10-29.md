# ArdJSON shared UTF-8 decoder — 2026-10-29

## Implementation

`ArdUtf8::consume()` in `JsonUtf8.h` provides one strict incremental decoder for
`ArdJsonStream::output()` and `ArdJSON::validUtf8()`. The parser retains its
existing state fields; the complete-string validator keeps equivalent local
state. No heap buffer, additional implementation source or public API is needed.
Template linkage and the no-inline attribute retain one shared decoder in the
linked program.

Validation rejects illegal lead/continuation bytes, overlong encodings,
surrogates, values above U+10FFFF and truncated sequences. Parser error and
callback handling, JSON byte output and numeric precision remain intact.

## Firmware measurements

Clean `DynamicPages` builds, ESP8266 Generic core 3.1.2 and ESP32 Dev Module core
3.3.12, unchanged board settings. Baseline is the preceding retained DOM/Writer
optimization; its source snapshot matches this pass's baseline except whitespace.

| Target | Before .bin | Final .bin | Saved | Static RAM |
| --- | ---: | ---: | ---: | ---: |
| ESP8266 | 592256 B | 592112 B | 144 B | 43756 B, unchanged |
| ESP32 | 1282128 B | 1281984 B | 144 B | 57732 B, unchanged |

Image sizes include padding. Compiler output, binary hashes, source manifests
and log locations are recorded in the JSON report. These measurements describe
this example, not every possible firmware profile. On-device throughput was not
measured; sharing replaces duplicated decoder code with a call per input byte.

## Rejected variants

| Variant | ESP8266 .bin | ESP32 .bin |
| --- | ---: | ---: |
| Direct array append in `push()` | 592304 B | 1282192 B |
| Block copy of RAM-owned keys | 592256 B | 1282128 B |
| Shared Writer key/colon output | 592272 B | 1281952 B |
| Shared UTF-8 plus shared Writer key/colon output | 592144 B | 1281824 B |
| Same, forcing Writer helper inline on ESP8266 | 592192 B | 1281824 B |

The retained decoder reduces both platforms without the ESP8266 size increase
introduced by adding the Writer helper on top. Direct append grew both images;
block copying did not change either image. Those changes are not retained.

## Validation

`tests/memory-host/json-utf8.cpp` checks every Unicode scalar, the surrogate
range, the Unicode ceiling and every non-ASCII lead/second-byte pair at lengths
1–4. Pair tests use independent explicit byte bounds for both parser and string
validation, rather than comparing two consumers of the same decoder.

`bash tests/run-config-memory.sh` passed with ASan/UBSan, including JSON numeric,
stream/patch, allocation-failure and filesystem power-loss/recovery cases.
`bash tests/run-littlefs-storage.sh`, page-stream and runtime-state regressions
passed. Formatting and `git diff --check` passed.

Artifacts: `/tmp/ardjson-more-20261029`; baseline artifacts:
`/tmp/ardjson-dom-20261029`.
