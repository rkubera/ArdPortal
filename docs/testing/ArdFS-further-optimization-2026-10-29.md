# ArdFS further firmware optimization — 2026-10-29

The baseline for this pass already includes the small write cache, the previous
size optimizations, circular sector allocation, persisted wear history and
optional cold-file bank collection. It is the current wear-distribution version,
not the earlier pre-wear images. These existing features remain enabled.

## Retained changes

- Share CRC polynomial multiplication and combination between native file
  metadata and gzip asset trailers. Keep the asset helper's existing XOR
  behavior for arbitrary CRC arguments at zero length.
- Share the reflected CRC-32 byte update between native file scanning, journal
  streaming and the storage checksum path. This is an out-of-line byte helper;
  the change targets code size, without an on-device throughput measurement.
- Compute required/optional bank collection capacity once. Preserve deferral
  for pinned destinations, unusable metadata and insufficient healthy capacity.
- Skip the erased reserved root prefix during relocation. Begin replay at the
  first page that can contain payload; preserve the original flash source and
  dirty cache page for subsequent destination failures.
- Remove state assignments overwritten before their next read. An explicit
  no-inline collection helper increased the ESP32 image and was not retained.

Shared dynamic buffers remain 256-byte cache + 256-byte relocation scratch.
No checksum format, file limit, allocation policy or publication guarantee was
removed. The V3 wear extension and V1/V2 migration remain intact.

## Clean firmware builds

`DynamicPages`, ESP8266 Generic core 3.1.2 and ESP32 Dev Module core 3.3.12,
unchanged board defaults and optimization flags. Both builds use `--clean`.
The baseline uses an isolated source snapshot; final builds use the workspace.
Dependency files verify the intended headers rather than reused sketch objects.

| Target | Before this pass | Final | Saved | Static RAM, unchanged |
| --- | ---: | ---: | ---: | ---: |
| esp8266 | 593168 B | 592928 B | 240 B | 43756 B |
| esp32 | 1283088 B | 1282864 B | 224 B | 57740 B |

These are application `.bin` sizes including padding. ESP32 compiler program
bytes decreased from 1282834 to 1282622 (212 B); its image decreases by 224 B.
The JSON contains source manifests, binary hashes and build log paths.

## Validation

`bash tests/run-config-memory.sh` passed with ASan/UBSan, including physical
fragmentation, source corruption, repeated relocation, unreadable-prefix
rollback, small-cache allocation rejection, migration, reader pins, wear
rotation/restart, optional-collection deferral, cold-data cycling, torn writes,
power cuts and uncertain-publication remount recovery.

`bash tests/run-littlefs-storage.sh` passed on both core API variants, including
Portal config/AppConfig persistence. `python3 tests/asset-crc-test.py` checks
173 zlib/reference and boundary cases with ASan/UBSan, including incremental
byte updates and the zero-length compatibility rule. It is included in
`tests/run-host.sh`. Generated/compiled gzip profile tests also passed, verifying
actual FULL/BASIC/CUSTOM, Matter and Zigbee resource output.

Formatting of the changed native headers and CRC utility headers was checked;
the touched checksum lines in the existing storage implementation were formatted
without rewriting unrelated code. `git diff --check` passed. Artifacts and logs:
`/tmp/ardfs-cache-opt2-20261029`.
