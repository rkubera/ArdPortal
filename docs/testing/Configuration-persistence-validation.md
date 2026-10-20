# Configuration persistence validation

Date: 2026-10-20. Hardware access was limited to the user's USB-connected
ESP32-C5. No broker, Portal HTTP interface, gateway or other device was accessed.
The existing test sketch retained its Wi-Fi/Zigbee services; test commands and
observations used USB only.

## Reproduced stale configuration after bank reuse

A successful save did not guarantee that the same data was selected after boot.
The raw flash volume uses two banks and an append log within each bank. Compaction
previously erased the destination bank header and one end-marker sector, leaving
older committed records farther into that bank. Later appends could fill that
marker without clearing the following old record header. A boot scan then traversed
those old records and overwrote its file index with stale contents.

Evidence:

- A host regression using the real volume implementation saved value `16` and
  restored `7` after remount. This occurs after reusing a previously filled bank;
  one compaction into an unused bank does not reproduce it.
- On ESP32-C5, a burst test received successful save callbacks and read slider
  value `96` before reboot, then read the old value `61` afterward.
- The incident flash dump contained two valid bank headers. Bank generation 40
  contained newer configuration records followed by committed records left from
  an earlier use of that bank. The later physical records had journal generations
  74–77, overriding newer file-index entries during mount. Their CRCs were valid.
- Journal generation comparison itself correctly selects the newer of the two
  files that the volume presents. The stale volume index hid the actual newer
  records before that comparison could run.

The fix prepares an erased successor header **before** replacing the current log
end marker. A power interruption can therefore expose the prior committed state
or the completed new record, without revealing the older bank tail. The on-flash
record format is unchanged. The fix prevents recurrence; it does not automatically
reconstruct data already superseded by a prior faulty boot.

## Additional resource-failure protection

An independent allocation-injection test reproduced silent rollback when JSON
validation of the newer configuration ran out of memory. Validation now reports
that failure instead of classifying the newer copy as corrupt. It also avoids a
copy of the decoded journal payload and checks memory allocation when retaining
the selected document.

Mount scanning now distinguishes allocation, flash I/O and index-capacity errors.
Such failures abort without switching to an older valid bank or automatically
formatting storage. A volume without a valid bank header retains its existing
initial-format behavior. Genuine journal CRC corruption still permits recovery
from the other configuration file.

## Local regression coverage

Run `bash tests/run-config-memory.sh` from the repository root. AddressSanitizer
and UndefinedBehaviorSanitizer are enabled. The suite passes with:

- newest journal in either slot, generation rollover and corrupted-copy recovery;
- allocation failure during both reads and writes, without stale fallback or overwrite;
- mount allocation and I/O failures without formatting or changing flash contents;
- 40 writes with remount after each write, spanning repeated bank reuse;
- power cuts and torn operations while advancing the end marker in a reused bank;
- the existing 156 raw I/O interruption cases for ordinary writes and compaction;
- rapid changes across three application keys while storage progresses, followed
  by reconstruction of the Portal instance and comparison with the last accepted values.

## USB test protocol

The existing test firmware exposes `get` and persistent `set` operations for
`relay`, `setpoint` and `profile`. Temporary diagnostics add storage status,
explicit flush and restart commands. The firmware was built against this checkout
and uploaded only to its active application slot, preserving ArdFS and pairing.
A 4 MiB backup and the separate incident filesystem dump were retained locally in
`/private/tmp/ardportal-storage-debug`.

A first 90-edit burst on the original firmware accepted all changes and survived
restart. A stronger test forces saves throughout each burst and restarts between
bursts; this reproduced the bank-reuse defect described above.

Before the application-edit queue change below, the direct application API could
return `false` during an active save. The baseline test
tracks the last **accepted** value for each key and waits for storage completion
before reboot. Rejected requests are not counted as saved changes. Application
callers must check acceptance; save completion is reported
separately through `onPortalAndAppConfigSaved()`.

## Final USB result with the bank-boundary fix

Six consecutive bursts of 90 edits each completed, with forced saves throughout
and a restart after every burst. All last accepted values matched after all six
restarts. Across 540 attempted edits, 139 were accepted and 401 returned a busy
rejection while a prior save was active. No successful-save callback reported an
error. This deliberately stresses API backpressure rather than treating rejected
requests as completed writes.

| Burst | Accepted | Busy rejection | Restored relay | Restored slider | Restored profile |
| --- | ---: | ---: | --- | ---: | --- |
| 1 | 21 | 69 | false | 82 | eco |
| 2 | 40 | 50 | false | 89 | normal |
| 3 | 17 | 73 | false | 60 | eco |
| 4 | 19 | 71 | true | 2 | normal |
| 5 | 18 | 72 | false | 0 | eco |
| 6 | 24 | 66 | false | 16 | normal |

The original three test values were then restored (`relay=true`, `setpoint=61`,
`profile=eco`) and verified after a seventh restart. The corrected firmware remains
on the test board. The incident and final JSON results, USB logs, original flash
backup and build/upload logs are retained in the local audit directory above.
Other pre-existing test controls were not intentionally changed by this test.

## Application edits queued during an active save

The follow-up change isolates application edits in a second patch while the active
snapshot is being written or waiting to be applied. Repeated edits replace only
the same key; different keys and deletion markers remain queued. Busy status,
explicit flush and normal restart draining include this patch. Persistent
completion still uses the save callback rather than the setter's return value.

The host regression now requires every rapid setter call to return `true`. It also
covers deletion of a value present in the active snapshot, overwriting an already
queued value, allocation failure without changing a previously accepted value,
and application edits behind another source's pending save. It checks successful
callbacks and reloads the final configuration into a new Portal instance.

### USB result with the application-edit queue

The same six 90-edit bursts now accepted **540/540** setter calls (`true`), with
zero busy rejections. All final values matched before and after each of six
restarts. The original values were restored and checked after a seventh restart.
No observed save callback reported failure.

| Burst | Accepted | Rejected | Restored relay | Restored slider | Restored profile |
| --- | ---: | ---: | --- | ---: | --- |
| 1 | 90 | 0 | true | 88 | eco |
| 2 | 90 | 0 | false | 95 | normal |
| 3 | 90 | 0 | true | 1 | eco |
| 4 | 90 | 0 | false | 8 | normal |
| 5 | 90 | 0 | true | 15 | eco |
| 6 | 90 | 0 | false | 22 | normal |

The initial four-second USB status wait timed out at the end of burst six.
A later status request succeeded with storage idle, a successful storage status
and no storage error. The expected final values were then verified before and
after restart, completing that burst without repeating its writes. The transport
timeout's cause was not established; no assertion or watchdog message was observed
in the captured output. Results are in `queued-results.json`,
`queued-restored-values.json`, `usb-queued.jsonl`, `usb-queued-followup.log`,
`queued-local-tests.log` and the queued build/upload logs in the audit directory.

## Bounded retries and terminal value failure callback

Ordinary failed saves now retain their original snapshot for three total attempts,
with at least 1000 ms between failures. Later edits remain separate; the retry
snapshot is frozen even between attempts. After exhaustion the batch is discarded,
its application values are reported through `onAppConfigValueSaveFailed()`, and
already accepted later edits continue with a fresh attempt budget. The current or
last batch count is available through `portalAndAppConfigSaveAttempts()`.
The ordinary save callback reports every attempt. Failed MQTT connection trials
retain their explicit rollback behavior and do not enter the retry loop.

The host suite injects actual volume write failures and verifies:

- two failed attempts followed by a successful third attempt, retaining fields and
  deletion markers;
- later edits to the same field remain queued and ultimately win;
- explicit flush calls and loop timestamps sampled before completion cannot shorten retry backoff;
- exactly three failed attempts, per-value terminal callbacks with the attempted
  value or deletion, and no fourth attempt even after storage recovers;
- a failed full snapshot reports its submitted fields and deleted keys;
- already accepted newer edits survive exhaustion and a terminal callback can
  enqueue an independent recovery edit without that edit being cleared;
- MQTT acknowledgement state survives failed attempts and clears on success;
- a fresh batch resets its attempt counter to one, and successful values survive
  reconstruction of the Portal instance.

The failure injections run locally with ASan/UBSan; flash failures were not
artificially induced on the physical ESP32-C5. The build and upload of the updated
firmware are recorded in `build-retry.log` and `upload-retry.log` in the local audit
directory. Regression output is in `retry-local-tests.log`.

The USB smoke test of the retry-capable build accepted 90/90 rapid edits and
restored the expected values after restart. Original test values were then
restored and checked after another restart. Results are in
`retry-smoke-results.json`, `retry-smoke-restored-values.json` and
`usb-retry-smoke.jsonl`. The final build additionally uses signed elapsed time for
retry backoff so a completion timestamp newer than the loop's sampled time cannot
cause an immediate retry; the stale-timestamp case is covered by the host suite.

## Startup repair before accepting persistent edits

Portal now requests a repairing read before announcing configuration readiness.
A missing or invalid alternate is streamed from the verified surviving document,
using the same journal generation. The alternate is flushed, closed and read
back completely. The surviving slot is never opened for writing. Two valid
copies, even with different generations, cause no repair write.

Repair is bounded to three attempts with one-second failure backoff. If it cannot
finish, the surviving data remains available to restore runtime configuration,
while ordinary persistent edits are blocked. A subsequent successful startup
repair restores write acceptance; explicit factory reset/format remain deliberate
recovery options. If neither copy is valid, no repair is attempted from unverified
data. Standalone ArdFS reads remain read-only unless their third argument is true.

Host regression coverage includes:

- either slot as the survivor, with the alternate missing or invalid;
- byte-for-byte preservation of the surviving file;
- no writes when both copies are valid;
- three failed repairs returning the surviving data and an error;
- Portal restoring that data while refusing new edits after repair exhaustion;
- a later successful startup repairing redundancy and accepting edits again;
- 50 raw I/O interruption/torn-operation cases across repair, each retaining a
  recoverable original and completing repair after simulated power returns.

The updated host suite passes with ASan/UBSan. The hardware fixture and its backup
are in the same local audit directory (`before-repair-hardware.bin`,
`one-bad-copy-sector.bin`, `repair-hardware-fixture.json`). The fixture modifies
only an inner CRC in the older configuration record, maintaining that raw volume
record's CRC so startup must detect an invalid logical configuration copy. The
newest valid configuration record and pairing data are not changed by the fixture.

### Physical ESP32-C5 startup repair result

The fixture was applied through USB only to the older `/ardportal.json` record
(generation 213), in sector `0x3cf000`, after backing up the full ArdFS partition.
The newer `/ardportal.json.journal` generation 214 was kept valid and untouched.
The final repair-capable firmware was written to the existing active application
slot without changing partition or pairing data.

After boot, USB reported storage idle, storage OK and no storage error. The
original values (`relay=true`, `setpoint=61`, `profile=eco`) were preserved. A new
raw partition dump independently confirmed **two valid journal copies, both
at generation 214**. Both full payload hashes matched the surviving original
configuration, not merely the three observed test controls. The volume bank
remained at generation 44. The readback reset returned the board to its normal
firmware.

Evidence: `repair-hardware-result.json`, `after-repair-hardware.bin`,
`read-before-repair.log`, `upload-repair-fixture.log`, `read-after-repair.log` and
`build-repair.log` in the local audit directory. The host suite remains green,
including the 50 interrupted-repair cases and existing write/compaction cases.
