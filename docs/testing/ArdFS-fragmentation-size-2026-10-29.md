# ArdFS physical fragmentation — 2026-10-29

Implemented native V3 sector allocation: a file can span nonadjacent free sectors.
Deletion releases sectors after catalog publication; neighboring files are not
copied. Replacement is copy-on-write. Existing read handles retain their own
sector list and pin their sectors. Bad-sector relocation uses the retained 4 KiB
buffer and preserves the other files' physical addresses.

## Firmware size

Clean Arduino CLI builds of `examples/DynamicPages`, compared with commit
`30aeb93322aefcae8e460eff2ec37d553137a2b0`. Both snapshots use the same local
cores, CLI configuration and default board options; no optimization flags added.

| Target | Core | Before binary | After binary | Increase | Static RAM before → after |
| --- | --- | ---: | ---: | ---: | ---: |
| ESP8266 Generic | 3.1.2 | 587728 B | 592432 B | 4704 B | 43628 → 43740 B |
| ESP32 Dev Module | 3.3.12 | 1277088 B | 1282304 B | 5216 B | 57612 → 57724 B |

These are application `.bin` sizes including image padding. ESP32 compiler
reports 1282062 program bytes. The feature increases code size; it is not a
firmware size reduction. Prior bad-sector optimization measurements remain
historical baselines for their own changes.

Target ELF DWARF reports fixed reader 24 B, writer 396 B and index entry 32 B
on both targets. Previously reader/writer were 20/372 B. Readers additionally
store two bytes per file sector. Entries additionally store their path and two
bytes per sector, subject to allocation alignment. The single dynamic writer
buffer remains 4096 B and is shared with collection. Each initialized bank uses
one reference-count byte per physical sector; up to two bank arrays can remain
allocated. Unknown-length writers reserve a larger provisional sector table
and shrink the index entry on finalization. These figures exclude allocator
overhead and application/JSON buffers.

## Format and limits

Two banks remain, with two metadata sectors per bank. Catalog snapshots are
256 B and include up to 32 roots and 32 retired sector IDs. Per-file metadata
and sector list reside in the root sector. Maximum table length is
`floor((4096 - 32 - pathBytes - 2) / 2)` sectors, further bounded by bank space.
Payload capacity for N sectors is `4096*N - 32 - pathBytes - 2*N`.

V1/V2 remain readable and migrate to V3 in the spare bank on first mutation.
Migration needs enough healthy destination capacity. Old firmware cannot read
V3. Collection is still necessary when copy-on-write cannot fit or the only
surviving metadata log fills; a bank pinned by readers cannot be overwritten.
Rename copies the renamed file. This does not add in-place file editing.

A committed root carries its publication revision and bank-generation-dependent
metadata checksum, preventing a stale catalog from interpreting a reused root.
An unknown metadata read error prevents mounting V3 even if the other metadata
copy is readable: it might be stale after hole reuse. Already excluded bad
metadata sectors are allowed. No automatic formatting occurs on ambiguity.
Uncertain publication invalidates the volume; `begin()` permits a later remount
once the fault is cleared. Unreadable payload is not reconstructed automatically.

## Verification and provenance

`bash tests/run-config-memory.sh` passed (exit 0), exercising ASan/UBSan native
host fixtures, including
physical hole reuse on ESP8266 and ESP32, unchanged neighboring sectors, reader
pins, seek/remount, unknown lengths, source corruption, relocation of a failed
root commit, V1/V2 migration and all-data-sectors-bad/zero-capacity cases.
Fragmentation fault sweeps cover 820 I/O interruption points per target;
bad-sector sweeps cover 902 points for each ESP8266 geometry and 754 for ESP32,
including torn operations. Additional legacy migration sweeps are separate.
`bash tests/run-littlefs-storage.sh` passed (exit 0), checking compatibility with
the optional backend. Final logs are `complete-suite.log` and
`complete-littlefs.log` in the artifact directory.

Artifacts, isolated source snapshots and JSON compiler logs are in
`/tmp/ardfs-fragments-20261029`. Final measured production sources are snapshot
`release2`; source hashes, binary hashes and sizes are recorded in the adjacent
JSON report. Dependency files verify that both builds included its new fragment
implementation. Generated compiler intermediate directories were removed to
recover disk space; binaries, ELFs, logs and dependency files were preserved.
Earlier attempts interrupted by disk exhaustion are not successful validation.
An initial full host run reached the final ESP32 storage test but lacked a test
capability-header stub; the stub was added before rerunning the full suite.
