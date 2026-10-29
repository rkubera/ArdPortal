# ArdFS sector buffering and bad-sector maps — 2026-10-28

Clean Arduino CLI builds compare baseline commit `e59072c` with the retained
implementation. All builds use unchanged `DynamicPages`, ESP8266 core 3.1.2 and
ESP32 core 3.3.12 with default board settings and unchanged enabled features.
Final dependency files reference the isolated library snapshot; every source
file was checked byte-for-byte against the workspace.

## Firmware size

| Target | Before feature | First complete version | Previous optimization | Current | Saved this round | Remaining growth |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| ESP8266 Generic | 584,512 B | 589,312 B | 587,744 B | 587,728 B | 16 B | 3,216 B (+0.55%) |
| ESP32 Dev Module | 1,275,024 B | 1,279,584 B | 1,277,296 B | 1,277,088 B | 208 B | 2,064 B (+0.16%) |

Compared with the first complete implementation, this recovers 1,584 B / 2,496 B,
or 33.0% / 54.7% of the feature's original firmware growth. Measurements are the
uploadable `.bin` sizes. Core-reported flash code is 546,324 B / 1,276,834 B.
ESP8266 IRAM usage is unchanged.

The retained version is `pass7` in `/tmp/ardfs-sector-opt3-20261028`.
A fresh baseline and nine variants were clean-built for both targets this round.
The baseline matches the previous measured result after making `queueWrite()`
private. Shared erase/verification helpers in passes 8/9 increased firmware and
were discarded. Output binaries, dependency files, logs and source snapshots are
retained; generated intermediate directories of completed earlier trials were
removed to recover disk space. The JSON report preserves all three rounds.

This round saves 16 B / 208 B of flash and 80 B / 72 B of static RAM. The small
net flash gain includes a safety fix that was absent from the baseline; it was
retained despite consuming part of the optimization gains.

## Retained changes

The third round reuses the verified sector image for record-header publication,
removes duplicate retry handling and unnecessary compaction counters/phases,
and caches the physical address of the current data sector. Formatting reuses
its existing metadata buffer, and successor-marker checks reuse writer scratch.
The writer's fixed state shrinks from 404 to 372 bytes on both targets.

Pending tail retirements now update the destination bank's RAM cache directly,
removing a third permanently stored 80-byte BankMap. Decoding during mounting
uses an 80-byte local temporary instead. Retiring an unused tail sector leaves
previously committed logical addresses unchanged. Tests read existing files both
during such a pending write and after abandoning it, then save and remount again.

An additional regression reproduced data loss on the baseline: abandoning a
compaction after publishing a newer header, then reusing that bank with failed
metadata erases could overwrite data underneath the still-valid newer header.
The cached generation now identifies this reuse. Before touching any records,
the writer verifies that both metadata sectors were erased; explicit/silent
erase failures or unreadable/non-erased metadata abort safely. This extra scan
runs only when reusing a bank prepared by an abandoned newer transaction.
Both explicit and silent metadata-erase failures are covered by the new test.

Earlier retained changes:

- Data sectors are erased immediately before programming their retained image.
  Preparation no longer scans and erases the entire reserved record extent.
- The true successor marker is checked after the final buffered data sector is
  written and any retirement has changed the physical mapping. Only then can
  the record and map be published. Empty trailing buffers do not erase past EOF.
- Formatting and writing share a small map-publication job. Formatting uses a
  256-byte buffer instead of allocating a full writer and sector image.
- Index entries and writer handles own their names in the same allocation as
  their fixed state. Mounting fills the index entry directly, avoiding temporary
  String construction and duplicate name allocations. Names remain bounded to
  72 bytes and do not depend on caller-buffer lifetime.
- The expected record CRC combines the header/path CRC with the CRC of accepted
  input. One complete final read of the path and data verifies that CRC before
  publication, including corruption occurring after per-sector verification.
- Logical reads perform chunk and sector splitting; physical reads issue one
  bounded transfer. Input is locked once final flushing begins.

Earlier retained optimizations share address translation, capacity checks,
allocation and header publication; the cooperative storage engine queues one
input slice instead of calling the synchronous compatibility writer. Map
serialization preserves its explicit little-endian format and portable fallback.
Moving maps to a separate heap allocation increased firmware and was discarded.
No compiler options or enabled features were changed to obtain these results.

## Memory

| Target | Static RAM before feature → current | Reader state | Writer fixed state | Writer name | Sector buffer |
| --- | ---: | ---: | ---: | ---: | ---: |
| ESP8266 | 43,516 → 43,628 B | 20 B | 372 B | path length + 1 | 4,096 B |
| ESP32 | 57,508 → 57,612 B | 20 B | 372 B | path length + 1 | 4,096 B |

The name and writer state occupy one allocation. The writer and any compaction
copy share one dynamically allocated sector buffer, released on commit or close.
Each index entry uses 20 fixed bytes plus its name and terminator in one allocation.
Formatting needs only 256 buffer bytes. Sizes exclude allocator overhead;
whole-program peak heap was not measured.

## Preserved behavior and flash layout

Accepted slices remain available until their current physical sector has been
programmed and verified. Each step handles at most 256 logical bytes or one
sector erase. Two failed attempts retire a destination sector and retry from
the retained image without requesting input again.

Each bank has its own generation and map revisions, containing at most 32 sorted
physical bad-sector numbers. Earlier committed logical addresses remain stable
when a tail sector is retired. Compaction reads through the source map and builds
the destination using its own map. CRC and commit markers protect publication.
Normal updates replace and verify the older metadata copy before refreshing its
mirror. A single surviving metadata sector uses an emergency append log, skipping
torn entries; the only valid map copy is never erased.

Format v2 reserves two 4 KiB metadata sectors per bank, one more than v1. Each
retired data sector consumes another 4 KiB of capacity. There is no separate fixed
replacement-sector pool. V1 banks remain readable and migrate atomically on the
first write if their live records fit. Older firmware cannot read v2 banks.
Only a completely erased partition is initialized automatically; ambiguous
corruption and I/O errors do not trigger formatting. A bad-sector map cannot
reconstruct already committed data that has become unreadable.

The low-level writer accepts irregular consecutive fragments and unknown final
length. `flush()` publishes the complete record. In-place changes within an
existing committed file remain a future API using copy-on-write publication.

## Validation

`tests/run-config-memory.sh` passed with address and undefined-behavior sanitizers,
including 2,518 raw-file and 3,710 large-JSON I/O power-cut points, 46 native journal
cuts, 24 startup-repair cuts, OOM, index capacity, mutations and bounded loop work.
The counts decreased because the optimized path performs fewer I/O operations;
every cut position is still exercised.

Bad-sector tests passed for 256 KiB and 512 KiB geometries, each covering 570 map
cut cases including torn operations. Coverage includes v1 migration, irregular
unknown-length fragments, multiple gaps, metadata read/write failover, both
metadata sectors failing, no spare space, the 32-entry map limit, silent no-op
erasures, corrupt metadata without formatting and subsequent saves after every
map-publication cut. Additional regressions verify late corruption, retirement
of the final partial sector with stale records beyond EOF, writes rejected during
flush, and temporary 72-byte names across rename/remount.

The additional abandoned-publication regression fails on the baseline and passes
on the retained version. Final suite log: `/tmp/ardfs-opt3-final-suite.log`.

The mount OOM test now injects failures through `ArdAllocation` because native
mounting no longer allocates a String. `tests/run-littlefs-storage.sh` passed both
host platforms and Portal backend selection. LittleFS's on-disk format is unchanged.

Artifacts: `/tmp/ardfs-sector-size-20261028`, `/tmp/ardfs-sector-opt-20261028` and
`/tmp/ardfs-sector-opt-more-20261028` and `/tmp/ardfs-sector-opt3-20261028`. Details are in [the JSON report](ArdFS-bad-sectors-size-2026-10-28.json).
