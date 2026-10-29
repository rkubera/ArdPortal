# ArdFS firmware size comparison — 2026-10-27

Baseline: `e5fbd54586fad652253b782ce4e7ffb5e633a8d2`. Initial after: streamed ArdFS implementation, renamed configuration API and ArdPortal integration, before the size optimization described below.

The baseline, initial streaming and optimized variants were cross-compiled with Arduino CLI 1.5.1. `examples/DynamicPages` is identical in the snapshots. All six builds succeeded. Separate library snapshots and an isolated sketchbook ensure the baseline does not pick up the modified library.

## Firmware-size optimization (final update: 2026-10-28)

Final optimized builds used `--clean` and fresh build directories. Sketch dependency files were checked to reference the optimized snapshot. Intermediate trials that reused build directories retained stale sketch objects and are discarded.

| Target | Original baseline | Initial streaming | Optimized | Saved vs initial streaming | Remaining vs baseline |
| --- | ---: | ---: | ---: | ---: | ---: |
| ESP8266 | 584 368 | 594 880 | 584 592 | 10 288 (97.9% of growth) | +224 (+0.038%) |
| ESP32 | 1 276 848 | 1 287 712 | 1 275 024 | 12 688 (116.8% of growth) | −1 824 (−0.143%) |

The slot-opening pass saves another 256 B on ESP8266 and 384 B on ESP32 compared with 584 848 / 1 275 408 B. One checked helper opens slots for scan, payload delivery, repair, write and verification. Scanning still distinguishes a missing path from an operational error. A common chunk-read helper was also tested but increased ESP8266 size, so the previous read paths were retained. The measured candidates were 584 704 B (open helper), 584 768 B (open + read helpers), 584 640 B (scan/open + read helpers), and 584 592 B (retained scan/open helper).

The frame/number pass saves a further 192 B on ESP8266 and 96 B on ESP32 compared with 585 040 / 1 275 504 B. One cleanup routine releases fixed and overflow-frame key metadata; frame arrays have trivial destruction, avoiding duplicate array teardown machinery. Numeric transitions and checked uint32 decimal accumulation use smaller equivalent expressions. Tests cover reset/destruction with unfinished or duplicate-key objects beyond 16 levels and parity with the previous JSON parser.

Two alternate UTF-8 range validators passed Unicode tests but produced 584 912 / 584 896 B ESP8266 images, versus 584 848 B with the retained validator. Both were reverted. The exhaustive Unicode/invalid-sequence regression test remains in the suite.

The header/initialization pass saves another 336 B on ESP8266 and 576 B on ESP32 compared with 585 376 / 1 276 080 B. Bank and record headers share construction/publication code. The DOM parser accepts its limits at construction, avoiding a second initialization and removing unused reset code from this example. Syntax-engine construction is shared out of line. The flash images remain byte-identical to the preceding implementation after 40 variable-length writes, compaction and repeated remounts. ESP8266 reads now use the aligned uint32_t core overload; the generic uint8_t overload remains linked by other code, so no binary saving is attributed to that change.

The ESP8266-focused pass saves a further 768 B on ESP8266 and 688 B on ESP32 compared with 586 144 / 1 276 768 B. Flags and frequently addressed Strings precede large buffers; syntax validation shares the CRC/escaped-length pass; output reuses the existing byte buffer; and CRC combination uses scalar polynomial multiplication instead of two 32-word matrices. An independent test compares the new combiner with the old matrix algorithm for 1 024 inputs, every length bit and SIZE_MAX. Legacy deduplication still works when an unchanged payload would exceed escaped-output capacity.

ESP8266 analysis: its raw flash-read function is 149 B both before the ArdFS rewrite and after the preceding optimization. The old loop was 4 830 B on ESP8266 versus 7 125 B on ESP32; the preceding optimized loops were 3 270 / 3 979 B. Thus the smaller pre-existing ESP8266 loop offered about 1.6 kB less savings. This explains much of the different net growth, rather than a larger flash driver. DWARF confirms the ArdFS state shrank from 960 to 944 B after layout changes. The CRC change removes 256 B of local scratch arrays; this is not a measurement of whole-program stack peak.

The preceding index pass saved another 1 456 B on ESP8266 and 1 728 B on ESP32 compared with the previous shared-parser version (587 600 / 1 278 496 B). The file index now uses bounded linked entries instead of vector relocation; pending index memory is allocated before flash commit, write/verify transfers share a state, and callbacks are moved instead of copied. A dedicated test covers all 32 paths, repeated compaction, remount and allocation failures.

The optimization shares one non-template syntax engine between DOM parsing and streaming validation, factors journal callback dispatch and error handling, and reuses the journal parser state for input validation. It preserves streamed transfers, JSON/UTF-8/duplicate-key validation, the flash format, CRC, two copies and repair.

Optimized static RAM is 43,504 B on ESP8266 and 57,508 B on ESP32. These figures do not measure peak heap use.

Optimized core-reported flash code is 543,204 B on ESP8266 and 1,274,774 B on ESP32. ESP32 app partition capacity is 1 310 720 B.

Validation includes old/new JSON grammar and tree parity, the sanitizer-enabled host configuration suite, large binary/JSON files and power-cut recovery. The wider host suite requires CMake for four unrelated Matter checks; CMake was unavailable in this environment.

The tables below preserve the original baseline-to-initial-streaming comparison.

## Further allocation audit — 2026-10-28

Read handles no longer copy the path: reads use only the resolved address, length
and cursor. Writer paths remain owned and checked before preparation. This avoids
one String allocation for paths exceeding the core's small-string capacity,
including `/ardportal.json` and its journal path. Short paths already held inline
do not save a heap allocation. No whole-program peak-heap reduction is claimed.

Fresh clean DynamicPages builds for this change are 584,608 B on ESP8266
(+16 B against the preceding flash minimum) and 1,275,024 B on ESP32 (unchanged).
Static RAM is unchanged. This retains a small flash cost in exchange for avoiding
read-path allocation and its allocation-failure point.

Rejected experiments: a byte-sized phase enum combined with a common CRC byte
helper produced 584,704 / 1,275,120 B; the CRC helper alone produced
584,592 / 1,275,024 B, identical to the preceding implementation. Neither is retained.
Snapshots, logs and images are in the same temporary artifact directory under
`optimized22`–`optimized24` and the matching board `pass22`–`pass24` directories.

The sanitizer-enabled configuration suite passed. The retained read-path change
also passed separately rebuilt sanitizer tests for all 32 index entries,
compaction/remount, allocation failure and 164 raw-I/O power-cut points plus
54 startup-repair cuts.

## Separate read/write handles — 2026-10-28

Read handles now contain only the volume pointer, resolved address, file length,
cursor and writer tag. A derived writer owns the path, record/commit/compaction
state, 32-byte header and 256-byte work buffer. A tagged deleter destroys the
correct allocation without virtual dispatch or an extra allocation. Compaction
copies remain writer allocations. Public API and on-flash format are unchanged.

Target DWARF in `ArdFSVolume.cpp.o` confirms these allocation payload sizes
(excluding allocator metadata/rounding):

| Target | Previous read | New read | Saved per read handle | Writer before/after |
| --- | ---: | ---: | ---: | ---: |
| ESP8266 | 380 B | 20 B | 360 B | 380 B |
| ESP32 | 384 B | 20 B | 364 B | 384 B |

Clean DynamicPages images are 584,592 B on ESP8266 (−16 B against the preceding
read-path change) and 1,275,088 B on ESP32 (+64 B). Static RAM remains
43,504 / 57,508 B. These are per-handle heap savings, not a measurement of
whole-program peak heap. Snapshots and clean build logs are under `optimized25`
and the board `pass25` directories in the same temporary artifact directory.

The allocation regression test verifies that readers request at most 64 B on the
host and that each reader/writer still uses one allocation. It also exercises
read/write rejection, reader flush/prepare, move construction/assignment,
replacement of an open reader, repeated close and release of the writer guard.
The full sanitizer-enabled `tests/run-config-memory.sh` suite passed, including
2,988 raw-file and 4,208 large-JSON power-cut points, repair, OOM and compaction.

## Application firmware (`DynamicPages.ino.bin`)

| Target | Before (bytes) | After (bytes) | Difference | Change |
| --- | ---: | ---: | ---: | ---: |
| ESP8266 Generic, core 3.1.2 | 584 368 | 594 880 | +10 512 | +1.80% |
| ESP32 Dev Module, core 3.3.12 | 1 276 848 | 1 287 712 | +10 864 | +0.85% |

These are the uploadable application images, excluding separate bootloaders, partition tables and merged images. The differences include the parser and Portal changes required by the new ArdFS API.

## Static/global RAM

| Target | Before (bytes) | After (bytes) | Difference |
| --- | ---: | ---: | ---: |
| esp8266 | 43 008 | 43 616 | +608 |
| esp32 | 57 012 | 57 700 | +688 |

RAM figures come from the board core size report and do not measure peak heap use while saving/loading JSON. Firmware and static RAM increased; this experiment does not quantify the dynamic-memory benefit of streaming.

## Linker/core size report

| Target | Flash code before | Flash code after | Difference |
| --- | ---: | ---: | ---: |
| esp8266 | 542 924 | 553 512 | +10 588 |
| esp32 | 1 276 598 | 1 287 470 | +10 872 |

The core-reported code size differs from the `.bin` size because the image also contains headers, loadable sections and alignment. ESP8266 reports IROM here; ESP32 reports Arduino program-storage usage.

## Build settings and artifacts

- ESP8266: `esp8266:esp8266:generic`, 80 MHz CPU, 1 MB flash, 40 MHz DOUT, core 3.1.2, default flags (`-Os`).
- ESP32: `esp32:esp32:esp32`, 240 MHz CPU, 4 MB flash, 80 MHz DIO, default partition scheme, core 3.3.12, default flags (`-Os`).
- CPU/flash/partition/compiler settings were checked for equality before/after.
- ESP32 program-storage usage after the change is 1 287 470 / 1 310 720 bytes (98%).
- Full JSON compiler logs, code snapshots and binaries: `/tmp/ardfs-firmware-size-qtyn83vc`.
- Source fingerprints, binary SHA-256 hashes and numeric results: [JSON results](ArdFS-firmware-size-2026-10-27.json).

Each build used this command pattern (the JSON CLI configuration selects the installed Arduino cores and an isolated temporary sketchbook):

```sh
arduino-cli --config-file <temporary-config> --json compile --clean \
  --fqbn <target> --jobs 4 --library <before-or-after-snapshot> \
  --build-path <separate-build-dir> --output-dir <separate-output-dir> \
  <snapshot>/examples/DynamicPages
```
