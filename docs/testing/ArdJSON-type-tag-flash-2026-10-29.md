# ArdJSON value tag — 2026-10-29

## Retained implementation

`JSONVar::Type` uses a `uint8_t` underlying type on ESP8266. ESP32 and other
builds retain the default `int` underlying type. Enumerator names and values,
JSON output, numeric ranges and public method signatures are preserved.

Clean embedded builds assert `sizeof(JSONVar) == 24` on ESP8266 core 3.1.2 and
`sizeof(JSONVar) == 32` on ESP32 core 3.3.12. The ESP8266 value occupies four fewer
bytes, independently of allocator rounding and separately allocated strings,
keys and linked members. Its enum storage/DOM layout changes ABI: compile all
translation units against matching headers and platform definitions.

## Firmware measurements

Clean `DynamicPages` builds with the board defaults. Baseline includes the
preceding shared UTF-8 decoder. Application image sizes include padding.

| Target | Before .bin | Final .bin | Saved | Static RAM before → final |
| --- | ---: | ---: | ---: | ---: |
| ESP8266 | 592112 B | 591840 B | 272 B | 43756 → 43716 B |
| ESP32 | 1281984 B | 1281984 B | 0 B | 57732 → 57732 B |

The byte tag is limited to ESP8266 because using it on ESP32 increased the image
by 32 B. This is source compatibility, not a guarantee of stable binary layout.
These measurements describe this example; other feature profiles may differ.

## Compared variants

| Variant | ESP8266 .bin | ESP32 .bin |
| --- | ---: | ---: |
| Byte tag on both platforms | 591840 B | 1282016 B |
| Explicit no-inline frame cleanup | 592112 B | 1281984 B |
| Shared allocation/copy path for owned keys | 592192 B | 1281968 B |
| Shared numeric error selector | 592112 B | 1282016 B |
| Move key strings directly into `keys()` output | 592112 B | 1281984 B |
| Retained platform-specific tag | 591840 B | 1281984 B |

The other variants are not retained: they either increase one target or do not
reduce firmware size. Source snapshots, image hashes and compiler output are
recorded in the JSON report.

## Validation

Clean embedded builds include compile-time node-size/tag-size assertions.
`bash tests/run-config-memory.sh` and `bash tests/run-littlefs-storage.sh`
passed with ASan/UBSan. Focused numeric, stream and patch checks also exercise
the ESP8266 tag branch. Formatting and `git diff --check` are checked.

Artifacts: `/tmp/ardjson-next-20261029`; baseline artifacts:
`/tmp/ardjson-more-20261029`.
