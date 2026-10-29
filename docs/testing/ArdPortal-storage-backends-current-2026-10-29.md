# ArdFS V4 versus LittleFS — current comparison, 2026-10-29

Four fresh clean DynamicPages builds from identical production source bytes.
The LittleFS sketch differs only by `#define ARDPORTAL_USE_LITTLEFS 1` before
includes. Board defaults and optimization flags are unchanged. Native V4 includes
fragmentation, bad-sector handling, wear distribution, movable metadata,
removal of V1–V3 compatibility and the Device format button.

## Measured firmware and static RAM

| Target/core | ArdFS image | LittleFS image | ArdFS firmware saving | ArdFS static RAM | LittleFS static RAM |
| --- | ---: | ---: | ---: | ---: | ---: |
| ESP8266 Generic / 3.1.2 | 592400 B | 607552 B | 15152 B (14.80 KiB) | 43756 B | 44028 B |
| ESP32 Dev Module / 3.3.12 | 1282480 B | 1310096 B | 27616 B (26.97 KiB) | 57732 B | 57636 B |

“ArdFS uses about 15 KB less firmware” is an observed result for ESP8266, not a
required threshold or a universal standalone library size. It is 15152 decimal
bytes, slightly below 15 KiB (15360 bytes). ESP32 saves about 27 KB. Image sizes
include padding and the whole application, shared transaction engine and core.
Static RAM saves 272 B on ESP8266; ArdFS uses 96 B more on ESP32.
No runtime heap peak or throughput/latency benchmark was performed.

ESP32 compiler program usage is 1282234 B / 1309846 B (ArdFS / LittleFS).
The default 1310720-byte application partition leaves 28486 B / 874 B of
compiler-reported program capacity. This is firmware space, not file capacity.

## Functional comparison

| Criterion | ArdFS V4 in this project | LittleFS |
| --- | --- | --- |
| Allocation / fragmentation | Sector tables reuse separate holes; files need not be contiguous | Block allocation supports fragmented files |
| File storage capacity | Live files must fit one bank; the other bank is reserved, plus metadata/bad-sector overhead | No mandatory spare half-partition; metadata and COW updates still require free space |
| Bad flash | Read-back verification, relocation and persistent per-bank retirement list, max 32 retired sectors per bank | Bad-block detection and avoidance through core/block callbacks and verification |
| Wear | Circular allocation, cold-data bank cycling, catalog rotation through shared free sectors; approximate wear accounting | Dynamic wear leveling via block allocator |
| Power loss | Verified data/root/catalog commits and mirrored movable catalogs; fault-injection coverage; conservative mount errors for ambiguity | COW and atomic filesystem operations, committed through sync/close |
| Metadata | Two movable catalog sectors per bank, sixteen 256-byte slots each | Metadata pairs and logs |
| File/namespace limits | 32 live files, paths up to 72 bytes; flat namespace | Directory hierarchy and POSIX-like API; limits depend on core/configuration |
| Updating files | Current writer appends to a newly staged replacement; reads can seek; rename copies payload | Core supports seek/read/write and directory operations; ArdPortal bridge exposes its shared transaction API |
| Runtime RAM | Shared dynamic 256-byte cache + 256-byte relocation scratch; index, sector-reference maps and per-file tables allocate additional memory | Configurable bounded caches/buffers and open-file state; not measured here |
| Cooperative service | Bounded native I/O/scan steps through loop; physical erase/program can still block | ArdPortal queues requests, but core mount/format/flush/rename calls are synchronous |
| JSON redundancy | Shared ArdStorage journals maintain two physical config files; raw files do not receive that extra journal pair | Same shared ArdStorage journal policy |
| Formatting in Portal | Device button and POST /api/storage/format; success restarts | Same UI and endpoint use selected backend |
| Mount / old formats | V4 only; erased region can initialize automatically; V1–V3/corruption fail without auto-format | Bridge disables auto-format on mount failure |

LittleFS descriptions follow its [upstream documentation](https://github.com/littlefs-project/littlefs).
Neither backend reconstructs already unreadable user data without another valid
copy. Fault tolerance does not imply an ECC implementation or equal erase counts.
ArdFS simulator tests do not establish hardware lifetime or relative speed.

ArdFS now offers the needed fragmented allocation and metadata wear protection
with smaller firmware for this application. LittleFS retains a broader filesystem
API and avoids the mandatory second-bank capacity cost. A runtime benchmark on
the intended flash hardware is required to compare speed and heap peaks.

## Reproducibility

Artifacts: `/tmp/ardfs-compare-current-20261029`. Snapshots `ardfs` and `littlefs`;
logs `<board>-<backend>.log`, binaries/ELFs/dependencies in matching `-out` folders.
Every production source hash matches both snapshots and the workspace. All four
builds succeeded; compiler dependency files confirm backend separation. The
adjacent JSON records exact source/image hashes, compiler output and sizes.
Previous comparison reports remain historical and are not overwritten.
