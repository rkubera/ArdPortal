# ArdFS small-cache firmware optimization — 2026-10-29

The retained implementation shares the normal-write and relocation program,
read-back, erase and erased-page verification paths. A stage flag distinguishes
relocation from normal writes instead of separate state machines. Root commit
failures reuse the same retry/retirement path.

The drained 256-byte input queue is reused for generated root headers. The
separate 32-byte header and redundant root-address state are removed. Final
verification compares the payload CRC accumulator directly with the producer
accumulator; root metadata still receives page-by-page read-back verification
and the on-flash checksums are unchanged. Sector table CRCs consume native
little-endian bytes on supported targets, with a portable encoding fallback.
Root metadata uses the smaller measured copy implementation for each target.

The shared dynamic buffers remain 256-byte cache + 256-byte relocation scratch.
V3 compatibility, V1/V2 migration, bad-sector handling, reader pins, bounded
cooperative I/O, CRC validation and publication guarantees are preserved.

## Firmware measurements

Clean Arduino CLI builds of `DynamicPages`, ESP8266 Generic core 3.1.2 and
ESP32 Dev Module core 3.3.12, default board options and unchanged optimization
flags. The before build uses an isolated snapshot captured before this work;
the final build uses the workspace library. Dependency files verify both source
locations. Reused-object builds after switching library paths were excluded;
the figures below come from builds with `--clean` and separate baseline paths.

| Target | Before optimization | Final | Saved | Versus historical 4 KiB image |
| --- | ---: | ---: | ---: | ---: |
| esp8266 | 593056 B | 592624 B | 432 B | +192 B |
| esp32 | 1283040 B | 1282608 B | 432 B | +304 B |

The historical 4 KiB images are the `release2` binaries cited in the earlier
comparison (592432 / 1282304 B). The smaller cache still has a positive code
cost relative to that reference; this pass reduces it, not eliminates it.
Application `.bin` sizes include image alignment; ESP32 compiler program-byte
counts may differ slightly. Static RAM is unchanged; buffers and writer state
are allocated dynamically.

An unrolled byte encoder increased ESP8266 by 48 B and was rejected. Range
copying for root metadata grew ESP8266 by 32 B, so that target keeps the smaller
byte loop. Forcing the prefix helper out of line did not reduce image size and
was not retained. A temporary disk-exhaustion failure was excluded.

## Validation and provenance

`bash tests/run-config-memory.sh` passed with ASan/UBSan, including small-cache
allocation rejection, late and repeated relocation, unreadable-prefix rollback,
multi-page root metadata, unknown lengths, collection, migration, torn writes,
power cuts, source corruption and uncertain-publication remount recovery.
Formatting checks for the three changed production files and `git diff --check`
passed. The JSON records full source manifests, clean-build logs, binary hashes
and byte deltas. Artifacts and test logs are in `/tmp/ardfs-cache-opt-20261029`.
