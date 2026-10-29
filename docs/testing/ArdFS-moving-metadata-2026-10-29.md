# ArdFS V4: moving metadata and firmware optimization — 2026-10-29

V4 catalog logs move through the same free-sector pool and circular allocator as
file data. There is no fixed bootstrap pointer or restricted metadata pool.
Normal publication maintains two catalog sectors; each has sixteen 256-byte
slots. A full log gets a fresh erased/verified sector, commits a replacement,
then releases its predecessor. Released metadata sectors, including 0/1, may
hold files. Readers keep their existing sector pins and epoch checks.

## Firmware measurements

Clean `DynamicPages` builds, unchanged board defaults, CLI configuration and
optimization flags. Reference `before` already includes fragmentation, the
512-byte cache/scratch, wear distribution and preceding CRC/code optimizations.
`final` is an intermediate first V4 implementation, not the finished result;
`opt8` is the retained optimized implementation.

| Target/core | V3 before | First V4 | Optimized V4 | Saved vs first V4 | V4 cost vs V3 |
| --- | ---: | ---: | ---: | ---: | ---: |
| ESP8266 Generic / 3.1.2 | 592928 B | 594432 B | 593616 B | 816 B | 688 B |
| ESP32 Dev Module / 3.3.12 | 1282864 B | 1284304 B | 1283888 B | 416 B | 1024 B |

These are application image sizes including padding. ESP32 reports 1283638
program bytes in the final build. Static RAM is 43772 B / 57748 B, increases of
16 B / 8 B from V3. Target DWARF reports a volume fixed size of 408 B on both
platforms (396 / 400 B before). Writer fixed size is 388 B, up 4 B from V3 and
down 4 B from the first V4; readers/index entries remain 24 / 32 B.
The single dynamic 256-byte cache + 256-byte relocation scratch remains 512 B.
Reference maps and per-file tables retain their existing allocation rules.

## Reuse and retained optimizations

- Common `eraseStep` handles bounded erase/read-back for data, metadata and
  formatting. Common `programStep` handles page/type writes and verification;
  commit markers reuse it while preserving I/O versus mismatch errors.
- Catalog read-back compares against the retained page using the existing
  `checkPhysical`. Its redundant expected-CRC state was removed.
- The existing sector allocator, reference maps, retirement list, bank collection,
  CRC implementation and commit machinery serve moving catalogs too.
- Bounded payload read/CRC progress is shared by mount, source copying and final
  read-back. Differing legacy/new index finalization remains specialized: a
  larger generic finalization helper increased ESP32 size and was rejected.
- V4 address arithmetic is shared between file reads and extent validation.
  Reference ownership already detects duplicate roots during mount, avoiding a
  second nested duplicate-root search in catalog decoding.
- Header words use alias-safe `memcpy` with explicit alignment; generic byte
  helpers remain available for arbitrary alignment. Header caches/scratch and
  stack word buffers are explicitly aligned. Unaligned paths/sector tables keep
  their existing byte operations. Little-endian word operations compile to
  native loads/stores; portable byte encoding remains the fallback.
- Hot state/map words are placed at short offsets. The ESP32-only partition
  pointer shares the architectural role of the ESP8266 flash address. Redundant
  partition-size state was removed; physical operations remain bounded to the
  two configured banks. No firmware compiler flags were changed.

Initial sharing alone saved only 96/144 B. Subsequent measurements rejected
larger variants and retained the substantially smaller header/state layout.
Disk-exhausted compilation attempts were excluded and rerun successfully after
compressing older generated maps/ELFs. The final code also strengthens type
publication for continuation sectors before any payload is written.

## Format and failure behavior

Catalog generation/revision, roots, retirement/wear fields and CRC remain in a
256-byte record. V4 adds metadata-sector IDs at 176/178 and its own physical
address at 180. Publication reserves a fresh sequence for every attempt/mirror,
so a later verified retry supersedes an uncertain earlier commit. Closing in
the commit/read-back window applies durable publication before freeing sectors.
Uncertain unresolved commits invalidate the volume for a later remount.

Roots use the existing record prefix, with type published before payload.
Continuation sectors have a four-byte data type, also verified before payload
or replay. File bytes can therefore contain valid-looking catalog records
without becoming mount candidates. The logical payload bound for N sectors is
`4092*N + 4 - 32 - pathBytes - 2*N`; the type overhead is four bytes per
continuation, and the root sector table still must fit its sector.

Mount discovers logs anywhere in both banks. Typed data sectors are skipped.
Catalog sectors and sectors whose first type word is erased are scanned in
256-byte slots, including logs whose first slot never committed. Unknown
non-erased sector types never expose the remaining payload as catalog candidates;
this also protects discovery after a single-bit data-type fault. CRC-valid V4
images belonging to another physical address or bank geometry are ignored, so
raw legacy files containing such bytes do not prevent migration. A second scan is used on suspected committed damage. Discovery
uses a bounded buffer and is more expensive than probing two fixed sectors.
Unknown discovery read errors fail safely; known retired sectors are excluded.
On that fault path, temporary error tracking reuses the existing per-bank
reference-array storage and is released before rebuilding the live index.
No automatic format follows nonblank/ambiguous corruption.

V1/V2/V3 are read-only until mutation migrates live files through the spare bank.
The source remains intact until publication. The additional four-byte framing
can require more capacity for files at legacy sector boundaries; migration may
fail safely if the destination cannot hold them. Earlier firmware cannot read
V4. There is still a spare bank, at most 32 files, and up to 32 retired sectors
per bank (now shared by data and movable metadata).

A log rotation temporarily needs one free sector. Required space checks may
collect to the other bank; an abandoned peer log can fail one publication and
set a RAM hint forcing collection on the next attempt. Publication can fail
after one complete new catalog became durable; failure means old or complete
new content, rather than guaranteed rollback. Damaged/unreadable newest
metadata is treated conservatively rather than silently selecting stale files.
Wear history remains approximate; this does not promise equal erase counts or
a flash-lifetime estimate.

## Distribution and artifacts

The 128 KiB host fixture used 24 distinct physical catalog sectors across 360
one-byte updates while preserving a 9000-byte cold file. Original metadata
sectors were also reused as data. This demonstrates movement through the bank,
not an eight-sector metadata pool or a lifetime benchmark.

Artifacts: `/tmp/ardfs-moving-20261029`. Final snapshot is `opt8`; measurement
logs are `<board>-before.log`, `<board>-final.log` and `<board>-opt8.log`.
The adjacent JSON records image/source hashes, compiler output and target type
sizes. Native dependency files verify the isolated source paths. Older generated
maps and some trial ELFs are gzip-compressed to recover disk space; source
snapshots and uploadable binaries are retained. Initial failed test/build
iterations are not final validation results.

## Final validation

- `bash tests/run-config-memory.sh`: exit 0, final source, ASan/UBSan;
  log `opt8-validated-suite.log`. Covers both native flash adapters, fragmentation,
  sector retirement, reader pins, recovery, bounded scans and metadata movement.
  Moving-metadata sweeps passed 174 rotation cuts and 396 migration cuts per
  adapter, plus 231/194 tail-torn framing cuts on ESP8266/ESP32.
- `bash tests/run-littlefs-storage.sh`: exit 0, log `littlefs-validated.log`;
  native-only header changes are excluded from this backend.
- Both final Arduino builds completed cleanly. Dependency files identify the
  `opt8` headers; all production source bytes match that frozen snapshot.
  Only the three native ArdFS headers differ from the V3 measurement snapshot.
- `git diff --check`: passed.

Tests use a host flash simulator; they do not replace power-fault testing on
physical devices. Failed disk-exhausted runs are not counted as validation.
