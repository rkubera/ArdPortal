# ArdFS fragmentation: firmware optimization, 2026-10-29

This pass optimizes the fragmentation implementation measured in
[the initial report](ArdFS-fragmentation-size-2026-10-29.md).
The V3 format, legacy migration, bad-sector handling, publication guarantees
and dynamic shared 4 KiB sector buffer are preserved.

Changes:

- Share the bounded read loop between legacy logical addresses and V3 sector
  lists. Address translation remains bank-specific.
- Share checked payload copying between rename and bank collection; keep source
  CRC validation and per-sector read-back verification.
- Remove the duplicate physical write address and compact the writer layout.
  The allocation cursor fits the configured maximum of 65535 sectors per bank;
  the collection index fits the volume's 32-file limit.
- Use byte copies for little-endian sector tables on ESP8266/ESP32, retaining
  explicit byte-order conversions for other targets.
- Keep fragment-count arithmetic in one function instead of repeating its
  constant-division code at each call site.
- Revalidate the selected catalog into its existing map instead of allocating
  another temporary bank-map object on the mount stack.

A trial forcing the byte-codec functions out of line increased the ESP8266
binary by 96 B and was rejected. Failed builds due to temporary disk exhaustion
were excluded from measurements.

The clean-build setup is unchanged: `DynamicPages`, ESP8266 Generic core 3.1.2,
ESP32 Dev Module core 3.3.12, same Arduino CLI configuration and board defaults.
Artifact root: `/tmp/ardfs-fragments-20261029`; final measured snapshot: `opt7`.

## Final measurements

| Target | Initial fragmentation | Optimized | Saved |
| --- | ---: | ---: | ---: |
| ESP8266 Generic | 592432 B | 592176 B | 256 B |
| ESP32 Dev Module | 1282304 B | 1282112 B | 192 B |

These are application binary sizes, including image alignment. ESP32 compiler
program-size output decreased from 1282062 to 1281858 B (204 B); image padding
accounts for the difference from the `.bin` delta. Static RAM is unchanged:
43740 B / 57724 B respectively. Target DWARF reports a writer fixed size of
384 B, down from 396 B; reader and entry fixed sizes remain 24 B / 32 B.
The dynamic sector buffer remains 4096 B. The initial fragmentation increase
relative to the pre-fragmentation firmware is now 4448 B on ESP8266 and 5024 B
on ESP32. This is a modest reduction, not elimination of the feature's cost.

The adjacent JSON records source hashes and binary hashes. Both dependency
files name the isolated `opt7` implementation. Generated build intermediates
are removed after measurement; logs, binaries, ELFs and dependency files remain.

## Validation

Both final runs passed with exit code 0:

- `bash tests/run-config-memory.sh` (`opt7-suite.log`), with ASan/UBSan,
  fragmentation, bad sectors, V1/V2 migration, torn operations, power cuts,
  uncertain catalog publication and remount recovery on ESP8266 and ESP32.
- `bash tests/run-littlefs-storage.sh` (`opt7-littlefs.log`), including Portal
  configuration persistence through the optional LittleFS backend.

`git diff --check` passed. Production source hashes still match the measured
snapshot after validation.
