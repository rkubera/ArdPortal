# ArdFS file operations: firmware size, 2026-10-28

Clean Arduino CLI builds of unchanged `DynamicPages`, using the same cores and
settings as the preceding ArdFS measurements (ESP8266 3.1.2, ESP32 3.3.12).
The baseline is the saved `optimized25` snapshot; only the four ArdFS source
files differ. Sizes are uploadable application `.bin` files.

| Target | Before file operations | Initial operations | Optimized operations | Saved | Remaining growth |
| --- | ---: | ---: | ---: | ---: | ---: |
| ESP8266 | 584,592 B | 585,040 B | 584,912 B | 128 B | 320 B |
| ESP32 | 1,275,088 B | 1,275,504 B | 1,275,344 B | 160 B | 256 B |

The retained change borrows the mutation source path from the live index instead
of copying a String. The single-writer guard keeps that index stable until bank
publication. No source-path access occurs after the old index is released.
One copy-path selector now serves capacity planning, file skipping and renamed
path selection. The selector preserves normal replacement compaction as well as
rename/delete compaction. No new synchronous wrappers or flash-format changes.

Pointer-only trial: 584,960 / 1,275,392 B. Adding the shared selector saves another
48 B on each target. Both trials used fresh snapshots and clean build directories.

These numbers measure the existing DynamicPages application. Uncalled API methods
are eligible for linker garbage collection; calling every new method adds code.
They are not a measurement of peak heap or of an application exercising all APIs.

Logs, source snapshots and binaries are under
`/tmp/ardfs-firmware-size-qtyn83vc/file-opt1`, `file-opt2` and matching board output
and log paths. Numeric results and binary hashes are in the adjacent JSON file.

Validation: the full `tests/run-config-memory.sh` suite passed with address and
undefined-behavior sanitizers, including bounded work per loop, 2,988 raw-file
and 4,208 large-JSON power-cut points, startup repair, 32-entry compaction,
allocation failures and rename/delete power cuts. Host allocation payloads are
32 B for a reader and 440 B for a writer (previous writer: 456 B). Host sizes
are not target-device heap measurements.
