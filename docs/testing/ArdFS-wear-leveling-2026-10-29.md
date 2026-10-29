# ArdFS wear distribution — 2026-10-29

The native backend now allocates free physical data sectors in circular order.
Its per-bank cursor survives ordinary catalog publication and restart. Occupied,
retired and reader-pinned sectors are skipped, with at most 256 inspected slots
per allocation step and one complete traversal before reporting no space.
Collection children share the destination cursor rather than restarting at the
beginning for every file.

Transaction erase attempts, including metadata and failed attempts, accumulate
in a per-bank budget. Formatting is excluded. At `8 * (bankSectors - 2)` attempts,
a later payload write may use the existing atomic bank collection even if the
active bank has room. This periodically moves cold files and exercises both
banks. The destination budget resets when collection starts. Optional collection
is deferred if the destination is pinned, has no known usable metadata sector,
or its known healthy capacity cannot hold the resulting files. Mandatory
collection for space/recovery keeps its existing failure behavior. Deletion alone
does not trigger maintenance.

## Persistence and limits

The V3 catalog's unused bytes 164–175 contain an optional extension: marker
`0x31524c57` at 164, next-sector uint16 at 168, reserved bytes at 170–171, and
uint32 erase budget at 172. Existing catalog CRC, mirrored publication and bank
revision protect these fields. Older V3 records supply defaults of cursor 2 and
zero budget. New fields do not require another flash write or a format-version
change. V1/V2 migration remains unchanged. Older V3 writers can ignore/drop the
extension, which loses its wear history but leaves file layout compatible.

This is approximate distribution, without persistent per-sector erase counters.
Unpublished progress and some retries can be lost on power failure. Fixed
metadata sectors continue to use their catalog logs; relocation of metadata
through a wider pool is not implemented. The policy does not promise equal
physical wear or recovery of arbitrary unreadable data. Maintenance adds copy
traffic and total latency while retaining cooperative step bounds.

## Measured distribution

Same ESP8266 host workload, 128 KiB partition: create a 9000-byte cold file,
reset test-only erase counters, then replace a one-byte hot file 360 times.
Checksums/readback verify both final files. Metadata erases are excluded below.

| Measurement | Before | After |
| --- | ---: | ---: |
| Data sectors receiving an erase | 2 | 28 |
| Minimum erases per data sector | 0 | 1 |
| Maximum erases per data sector | 180 | 18 |
| Total data erases | 360 | 369 |

The additional nine data erases are cold-file movement. This is a specific
simulated workload, not a flash-lifetime estimate. Scripts and raw results are
`distribution.cpp`, `distribution-before.log` and `distribution-after.log` in
the artifact directory. Both reference builds use the same flash fixture and
ASan/UBSan; only the volume implementation differs.

## Firmware and RAM

Clean Arduino CLI builds of `DynamicPages`, default board options, same local
cores and configuration for isolated `before` and formatted `final` snapshots.
The baseline already contains the 256-byte cache + 256-byte relocation scratch
optimization; historical 4 KiB-buffer reports are not this baseline.

| Target | Core | Before binary | Final binary | Increase |
| --- | --- | ---: | ---: | ---: |
| ESP8266 Generic | 3.1.2 | 592624 B | 593168 B | 544 B |
| ESP32 Dev Module | 3.3.12 | 1282608 B | 1283088 B | 480 B |

Static RAM increases by 16 B: 43740 → 43756 B on ESP8266 and 57724 → 57740 B
on ESP32. DWARF confirms volume fixed sizes 380 → 396 B / 384 → 400 B.
Reader, writer and index-entry fixed sizes are unchanged (24 / 384 / 32 B on
both targets). Dynamic cache/scratch remains a single 512-byte allocation, with
no new dynamic allocation. Per-file tables and existing reference maps remain.
Image sizes include padding; ESP32 reports 1282834 program bytes after change.

Artifacts: `/tmp/ardfs-wear-20261029`. Adjacent JSON records production source
hashes, binary hashes, compiler output and measurements. Dependency files
verify isolated sources. The intermediate unformatted `after` builds give the
same image sizes; final numbers come from the formatted `final` snapshot.

## Validation

Both final commands passed with exit code 0:

- `bash tests/run-config-memory.sh` (`final-suite.log`), ASan/UBSan: native
  storage regression, fragmentation, bad sectors, small-cache relocation,
  legacy migration, corruption, unknown lengths, allocation failures and torn
  publication; plus wear cursor persistence, cold-data bank cycling, pinned and
  capacity deferral, old V3 catalogs without extension, 616 ESP8266 / 484 ESP32
  maintenance interruption points, and bounded scanning across 300 occupied
  sectors in a 3 MiB partition on both platforms.
- `bash tests/run-littlefs-storage.sh` (`littlefs.log`): optional backend
  compatibility and Portal configuration persistence.

Formatting checks passed for changed production files and the new fixture;
`git diff --check` passed. Final production hashes still match measured sources.
An initial scan fixture exceeded the ESP8266 stub's physical flash bounds; it
was corrected to a valid 3 MiB partition before the final full run. Build
intermediate directories were removed; source snapshots, binaries, ELFs, logs
and dependency files remain.
