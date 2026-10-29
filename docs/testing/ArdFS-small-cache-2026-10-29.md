# ArdFS small write cache — 2026-10-29

The native writer no longer allocates a 4096-byte sector image. It shares one
512-byte allocation between a 256-byte dirty program page and a 256-byte
relocation scratch page. Collection copies borrow that allocation. This reduces
the shared buffer allocation by 3584 bytes (87.5%); writer control state, the
256-byte input queue, index/sector tables and
reference maps are additional RAM. The optimized writer reuses the drained
input queue for its generated header.

Each destination sector is erased and checked in bounded pages. A dirty page
is retained until program/read-back succeeds. On repeated failure the writer
retires the destination and reconstructs its verified prefix from flash, then
replays the retained dirty page. The producer is not called again for relocation.
The original prefix remains the source even if a second destination fails.
An unreadable source prefix aborts the new write and preserves the previous
committed version; this does not promise recovery from arbitrary read failures.

Root metadata and sector tables are generated in pages into their erased prefix,
without erasing an already streamed root payload. Root relocation regenerates
checksums and the self-referencing sector list. The V3 disk format and bank
capacity are unchanged. V1/V2 migration still uses the spare bank.

## Validation

`bash tests/run-config-memory.sh` passed with AddressSanitizer and
UndefinedBehaviorSanitizer. The small-cache fixture rejects every 4096-byte
allocation and covers both ESP8266 and ESP32 with a 2 MiB simulated partition:
empty/boundary writes, collection, allocation failure, late program failure,
a second failed relocation destination, unreadable-prefix rollback, multi-page
root metadata, and unknown-length writes with a multi-page reserved prefix.

Existing interruption/torn-operation sweeps passed: 952 fragmentation I/O cuts
per platform; 970 bad-sector I/O cuts for each ESP8266 geometry; 822 for ESP32;
2332 large-file replacement/collection cuts; plus journal, mutation, startup
repair and uncertain-publication recovery tests. Each step retains the bounded
I/O contract (one program or erase per step; reads bounded by 256 bytes).

`examples/DynamicPages` also compiled successfully with Arduino CLI for
ESP8266 Generic (core 3.1.2) and ESP32 Dev Module (core 3.3.12), using the
workspace library and default board options. Build logs are
`/tmp/ardfs-cache-esp8266.log` and `/tmp/ardfs-cache-esp32.log`.

Historical firmware size and 4096-byte buffer figures in the earlier V3 report
refer to the implementation before this change.
