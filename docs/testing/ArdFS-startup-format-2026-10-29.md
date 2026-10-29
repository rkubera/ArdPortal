# ArdFS portal startup and format scheduling — 2026-10-29

## Scheduling contract

`ArdPortal::serviceStorage()` advances startup storage work, and mount/format
operations while storage is not ready, in batches of at most 64 storage steps.
It checks a 4 ms time budget between steps. A synchronous flash operation or
callback can exceed that budget by itself; the next step is then deferred.

Ordinary transactions after configuration is ready execute one storage step per
portal loop. Batching stops when startup/format completes, including if its
callback queues a normal transaction. The standalone `ArdFS::loop()` contract
is unchanged. No filesystem format, validation, repair or publication rule is
changed. JSON optimization changes are not included in this branch.

## Host measurements

Real native ArdFS, 1 MiB emulated flash, ASan/UBSan:

| Operation | Single storage steps | Portal calls with batching |
| --- | ---: | ---: |
| Mount formatted empty volume | 4101 | 65 |
| Explicit format and remount | 8763 | 137 |
| Complete Portal first-use startup, including format/load | — | 202 |
| Complete Portal reload/startup repair | — | 66 |

For an illustrative 30 ms of application/UI work between portal calls, the
mount scheduling overhead changes from 123030 ms to 1950 ms, and format/remount
from 262890 ms to 4110 ms. These are simulated application-loop delays, not
measurements on M5 hardware. Physical erase/program/read times and application
callbacks add their own time.

## Formatting UI

The Device format handler shows a modal overlay before submitting the format
request. Its progress bar moves back and forth without estimating a percentage.
The overlay prevents background actions and repeated requests. On successful
response completion it hands off to the existing restart overlay; on error it
restores controls, focus and the inline error message. Reduced-motion settings
use a stationary indeterminate indicator.

## Validation

- `python3 tests/storage-startup-test.py`: actual scheduling code, clock rollover,
  time/step bounds, idle handling, format completion/requeued work, real native
  volume, complete Portal first-use/reload, normal-write I/O limits and startup
  I/O failure preserving flash without automatic formatting.
- `bash tests/run-config-memory.sh` and `bash tests/run-littlefs-storage.sh` passed
  with ASan/UBSan.
- JavaScript UI regressions, including `storage-format-ui-test.js`, passed.
- Language metadata, factory reset callbacks, generated identifier/ARIA mapping,
  portal asset tests and actual compiled FULL/BASIC/CUSTOM profile assets passed.
- Firmware profiles `portal-esp8266` and `portal-esp32` compiled successfully.
- Generated language/assets checks, formatting and `git diff --check` passed.
- Local browser inspection verified the popup layout and alternating animation.

Logs are under `/tmp/ardportal-startup-*` and `/tmp/ardportal-format-*`.
