# ArdPortal storage backend selection — 2026-10-28

All four requested values are uploadable `DynamicPages.ino.bin` sizes, in bytes.
The change column uses the default ArdFS backend after adding the LittleFS option.

| Target | Before adding the option | Change with default ArdFS | Firmware with ArdFS | Firmware with LittleFS |
| --- | ---: | ---: | ---: | ---: |
| ESP8266 | 584,912 | −400 | 584,512 | 607,520 |
| ESP32 | 1,275,344 | −320 | 1,275,024 | 1,310,032 |

The default remains ArdFS. Defining `ARDPORTAL_USE_LITTLEFS 1` before
`#include <ArdPortal.h>` selects LittleFS in the sketch translation unit; no
build-wide flags are needed. Compared with the pre-option firmware, selecting
LittleFS adds 22,608 B on ESP8266 and 34,688 B on ESP32. The LittleFS core library
has a real flash cost; the option does not make that cost disappear.

## Method

All new builds used Arduino CLI `compile --clean`, separate snapshots/build
folders, and unchanged DynamicPages sketches except for the LittleFS sketch-local
define. ESP8266 uses Generic/core 3.1.2, ESP32 uses Dev Module/core 3.3.12.
CPU, flash, optimization and partition flags were checked equal before/after.
The baseline source snapshot is byte-identical to the preceding `file-opt2`
measurement. Binaries, hashes, logs and settings are in the adjacent JSON file.

The common transaction engine is now a backend-parameterized implementation.
The native driver is included only when explicitly selected; there is no native
filesystem `.cpp` compiled unconditionally. The linker symbol tables for both
LittleFS images contain no ArdFSVolume, ArdFSFile or ArdFSDetail symbols. None
of their 133 ESP8266 / 122 ESP32 compiler dependency files includes ArdFS.h,
ArdFSVolume.h or ArdFSVolumeImpl.h. This verifies compile-time exclusion rather
than relying only on linker garbage collection.

On ESP32 the core reports 1,274,782 B program storage with ArdFS and 1,309,786 B
with LittleFS. The default app partition is 1,310,720 B, leaving only 934 B program
storage headroom in this example with LittleFS. The `.bin` measurements include
image headers/alignment and therefore differ from core program-storage figures.
Native static RAM is 43,516 B / 57,508 B; LittleFS reports 44,028 B / 57,636 B.
These values do not measure peak heap or filesystem runtime allocations.

## Validation and limits

The full sanitizer-enabled native configuration suite passed, including 2,988
raw-file and 4,208 large-JSON power-cut points, OOM, repair, deduplication and
32-entry compaction. The LittleFS bridge tests pass against both ESP8266/ESP32
API variants and interrupt each of 20 core-I/O points during staged JSON writes.
They cover CRC fallback/repair, legacy JSON, binary files, seek, mutations,
metadata and mount/format failures. Portal integration tests persist/reload
PortalConfig and AppConfig, perform factory reset, identify the backend through
the information API and format through the HTTP route. Focused flash-text,
network-api, factory-reset-callback, page-stream and runtime-state tests passed.
The LittleFS fault tests use a filesystem mock; they are not physical-board
power-cut validation of the core filesystem implementation.

Both backends share journal/CRC/repair/validation behavior. Payload processing
is cooperative; LittleFS mount, format, flush, rename and internal garbage
collection are synchronous core calls. The bridge disables implicit formatting
on mount failure. Switching backends requires a compatible/preformatted volume
or explicit formatting, not automatic conversion of the native ArdFS format.
