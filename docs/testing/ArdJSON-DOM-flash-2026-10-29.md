# ArdJSON Writer and DOM code sharing — 2026-10-29

## Implementation

- Share exact `String` key lookup between `hasOwnProperty()` and mutable/const
  object access. Keep the C-string lookup independent so callers do not need
  extra length arguments or temporary strings.
- Share stored `ObjectKey` lookup between patch size validation and the Writer.
  The lookup supports flash dictionary keys, RAM-owned keys, empty names and
  embedded NUL bytes.
- In `Writer::writeObjectPatch()`, find replacements once, using stored keys
  directly. Membership checks do not create temporary `String` copies. Output
  still converts keys for escaping/serialization.
- Keep the common lookups as non-inlined template definitions, following the
  existing C-string lookup pattern. Linker COMDAT folding retains one copy of
  each specialization across translation units.

The public API, header-only value/serialization use, insertion/output order,
integer ranges and floating-point representation are preserved. No additional
implementation translation unit is required.

## Firmware measurements

`DynamicPages`, clean Arduino CLI builds in isolated source snapshots, board
settings unchanged: ESP8266 Generic core 3.1.2 and ESP32 Dev Module core 3.3.12.
The baseline includes the preceding integer-conversion optimizations.

| Target | Baseline .bin | Final .bin | Saved | Static RAM |
| --- | ---: | ---: | ---: | ---: |
| ESP8266 | 592384 B | 592256 B | 128 B | 43756 B, unchanged |
| ESP32 | 1282480 B | 1282128 B | 352 B | 57732 B, unchanged |

Application image sizes include padding. ESP32 compiler program storage drops
from 1282230 B to 1281878 B. These figures apply to this example, not every
possible set of JSON operations. The JSON report records section sizes, source
hashes, image hashes and build log locations. Final source matches the measured
snapshot apart from whitespace.

## Compared implementations

| Variant | ESP8266 .bin | ESP32 .bin |
| --- | ---: | ---: |
| Bulk out-of-line DOM, ObjectKey and Writer | 592544 B | 1282976 B |
| Out-of-line Writer only | 592544 B | 1282736 B |
| Out-of-line non-template DOM functions | 592512 B | 1283008 B |
| Shared pointer/length lookup | 592464 B | 1282400 B |
| Pointer/length lookup and direct Writer patch lookup | 592400 B | 1282176 B |
| Separate String lookup and direct Writer patch lookup | 592352 B | 1282176 B |
| Direct Writer patch lookup alone | 592336 B | 1282272 B |
| Direct stored-key lookup shared by Writer and patch validation | 592272 B | 1282272 B |
| Final combined lookups | 592256 B | 1282128 B |

Moving large portions of the header to an implementation file prevents useful
inlining and increases these firmware images. Those variants and their build
integration are not retained.

## Validation

The patch regression exercises exact String/flash keys, empty keys, long keys,
embedded NULs, read-only/mutable lookup under allocation failure, deletion,
replacement, insertion, one-byte output slices and source preservation. The
stream and numeric regressions cover syntax parity, Unicode and exact numbers.

Validation uses ASan/UBSan, the native configuration/recovery suite, the LittleFS
bridge suite, page streaming, runtime state, flash text, control models and build
matrix dispatch. Formatting and `git diff --check` are checked.

Artifacts and logs: `/tmp/ardjson-dom-20261029`.
