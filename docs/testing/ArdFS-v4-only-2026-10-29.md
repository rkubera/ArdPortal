# ArdFS V4 only — 2026-10-29

The native filesystem now accepts V4 only. V1/V2/V3 mounting, migration,
legacy logical-to-physical address translation, sequential record scanning and
unframed continuation reads have been removed. Old images fail mounting without
writes or automatic formatting. Existing old-format installations require an
explicit format and re-upload of files; this change provides no migration.

## Firmware

Clean DynamicPages builds with the same board defaults and compiler flags as the
preceding moving-metadata measurement (`opt8`). Sizes are application `.bin`
bytes, including padding.

| Board/core | Previous V4 with compatibility | V4 only | Reduction |
| --- | ---: | ---: | ---: |
| ESP8266 Generic / 3.1.2 | 593616 B | 592240 B | 1376 B |
| ESP32 Dev Module / 3.3.12 | 1283888 B | 1282336 B | 1552 B |

The result is also 688 B / 528 B smaller than the prior V3 measurement
(592928 B / 1282864 B), while retaining movable metadata and fragmentation.
Those V3 figures are historical reference points, not additional fresh builds.

Static RAM is 43756 B / 57732 B, down 16 B on each platform. DWARF gives a
396-byte volume on both platforms (408 B previously). Writer/reader/index entry
sizes remain 388/24/32 B; dynamic cache and scratch remain 256+256 B.
ESP32 reports 1282090 program bytes before image padding.

## Additional simplifications

- Removed format flags and separate legacy metadata-fault flags from bank state.
  One retirement list and one physical sector allocator serve data and metadata.
- Discovery reads only 256-byte catalog records; the 128-byte legacy half-slot
  parser is gone. Existing type framing and damaged-metadata checks remain.
- Payload reads always follow framed sector tables. Removed legacy address
  mapping, alternate source-checksum calculation and redundant read branches.
- Collection handles V4 space recovery and wear distribution only. It no longer
  converts layouts or translates retired legacy metadata sectors.
- V4 wear fields are required, removing old default initialization paths.
  Damaged or missing wear state fails safely instead of resetting counters.
- Capacity and used-space accounting use the common V4 sector reference maps.

## Verification and artifacts

`bash tests/run-config-memory.sh` passed with ASan/UBSan for both native adapters,
including fragmentation, retirement, bounded scans, reader pins, recovery and
power-cut sweeps. V1/V2/V3 fixtures now assert rejection with byte-for-byte
unchanged flash. Moving-metadata fixtures retain 174 rotation cuts per adapter
and 231/194 tail-torn framing cuts. The 360-update fixture still uses 24 distinct
catalog sectors. Tests are simulated flash tests, not physical device tests.

Artifacts are in `/tmp/ardfs-v4-only-20261029`; the frozen production snapshot is
`v4only`. ESP8266 compiled the snapshot. ESP32 resolved the installed ArdPortal
source path to the workspace; every production source byte was checked against
the frozen snapshot. Logs, binaries, ELF files, dependency files and the adjacent
JSON preserve the measured source identity and compiler output. Build
intermediates were removed after verification. The native-suite log is
`/tmp/ardfs-v4-only-suite.log`.

`bash tests/run-littlefs-storage.sh` and `git diff --check` also passed.
