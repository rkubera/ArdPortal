# Pull request checks

The `PR checks` workflow runs on pull requests (including forks), pushes to `main`
and manual dispatch. It runs `Host tests`, `Lint and assets`, and 14 firmware jobs
named `Firmware / Arduino / ...` or `Firmware / Matter SDK / ...`. Jobs have read-only repository access and
no repository secrets. Actions are pinned to commit hashes; tool/core/API-test
versions are explicit in the workflow. Only GitHub-owned actions are used;
Arduino CLI/Lint are downloaded from official releases with pinned SHA-256 hashes.

## Host tests

Install Python 3.12, Node.js 24, a C++17 compiler and `clang++`. Run:

```sh
bash tests/run-host.sh
```

The control model suite also checks all class/type combinations and verifies that disabled class names are absent from compiled profiles.

This covers configuration persistence/recovery/retries, native Zigbee/OTA,
MQTT/HA topics, networking, HTTP memory, runtime state, callbacks and frontend
behavior. Native suites use AddressSanitizer and UndefinedBehaviorSanitizer;
CI disables leak detection to match the existing cross-platform fixtures.

`python3 tests/storage-startup-test.py` checks startup and format storage
batching, time/step bounds and clock rollover. It also runs a native 1 MiB
volume and complete Portal first-use/reload startup with simulated application
work between calls, while checking that normal writes remain single-step.
`node tests/storage-format-ui-test.js` checks the indeterminate Device dialog,
paint-before-request timing, duplicate prevention, restart handoff and errors.

`python3 tests/asset-crc-test.py` compares shared CRC combination and byte
updates against Python zlib, including empty inputs and cache/page boundaries.
`portal-profile-assets-test.py` also checks complete gzip assets emitted by C++.

The job also tests generated gateway exports with the versions listed in
`requirements-host.txt`, zigbee-herdsman-converters 26.115.1 and the actual
Zigbee2MQTT 2.14.2 Home Assistant Discovery mapper. To run those tests locally,
install the Python requirements in a virtual environment and the Node dependency
in a temporary directory, then set `ZHC_NODE_MODULES` to that directory's
`node_modules` and `Z2M_HA_SOURCE` to the upstream `lib/extension/homeassistant.ts`.
Run `tests/zha-quirk-test.py` and `tests/zigbee-converter-test.mjs` with Python and
Node respectively. Dependency installation must not modify the library tree.

## Arduino builds

With Arduino CLI 1.5.1, install ESP8266 core 3.1.2 and Espressif ESP32 core 3.3.12
using their official package indexes, then run:

```sh
bash tests/compile-arduino.sh
```

CI selects nine representative Arduino builds. Each has its own status and log;
failures do not cancel other builds, and each runner installs only its required core.

| Platform | Firmware configuration |
| --- | --- |
| ESP8266 | Portal; public API with all features; headless logging |
| ESP32 | Portal; network-free API with Wi-Fi linker exclusion check |
| ESP32-C6 | Zigbee end device; Matter Wi-Fi with all controls |
| ESP32-H2 | Zigbee end device; Matter Thread |

The complete local suite retains 19 profiles, including C5 Zigbee, additional C6
Matter configurations and individual ESP8266 feature exclusions. CI omits these
additional firmware builds to reduce repeated compilation; host tests continue
checking feature guards, but do not replace target compilation.

Run `bash tests/compile-arduino.sh --list` for profile IDs. Pass one ID to compile
only that firmware, for example `bash tests/compile-arduino.sh zigbee-c5-ed`.
Omitting the argument (or passing `all`) still runs the complete local matrix.
Temporary build directories are cleaned on exit. `ARDUINO_CLI` can select a local
CLI executable. For ESP8266 builds only, a narrow Python warning filter hides
core 3.1.2's known `elf2bin.py` non-raw `\s` regex warning under Python 3.12.
Other Python and compiler warnings remain enabled; caller `PYTHONWARNINGS`
filters take precedence. These builds do not upload firmware or contact devices.

## Matter SDK builds

Five ESP32-C6 source builds check the SDK configurations below. Names use the
same `Firmware / family / configuration` pattern as Arduino jobs. Arduino builds
check compatibility with the packaged board core; SDK builds additionally check
linked transports and SDK configuration, so these are separate integration paths.

| Transport | Credential source | Power | Thread device type |
| --- | --- | --- | --- |
| Wi-Fi | SDK development credentials | Connected saving | MTD (unused with Wi-Fi) |
| Thread | SDK development credentials | ICD | MTD |
| Thread + portal Wi-Fi | Own certificates (factory) | Off | MTD |
| Thread + portal Wi-Fi | SDK development credentials | ICD | MTD |
| Thread + portal Wi-Fi | SDK development credentials | Off | Router-capable FTD |

`ARDPORTAL_CREDENTIALS=production` selects factory credential providers and the
`fctry` partition. CI checks compilation without provisioning real certificates;
it does not certify the product or test commissioning with production credentials.
The other builds use `ARDPORTAL_CREDENTIALS=development`. FTD enables router
eligibility; the Thread network determines the actual role.

This replaces the previous 11 SDK builds with representative coverage instead of
testing every combination of credentials, transport and power settings. Host tests
still exercise invalid combinations and configuration guards.

## Lint and generated assets

Arduino Lint 1.3.0 runs with specification compliance. Library Manager-only checks
are disabled (`library-manager: false`) because the external index can lag an
accepted submission; a separate assertion preserves the registered `ArdPortal`
name. Admission/update checks can be run separately when publishing a release.
Five generator `--check` commands verify languages, portal assets, compact
state schemas and both gateway exporters without rewriting files. Regenerate
changed assets locally and commit the generated headers with their sources.

Hardware validation remains separate: host tests and compilation do not establish
RF range, physical actuator behavior, radio coexistence or complete device OTA.

## Arduino code formatting

The root `.clang-format` is the default Arduino IDE 2 configuration. The source,
example sketches, C++ test fixtures and `idf/MatterPortal/main` sources use clang-format 14, matching the IDE.
Run `python3 src/tools/format_code.py` to format them, or add `--check` to verify
formatting without modifying files. CI runs this check with the pinned
`clang-format==14.0.6` PyPI wheel in an isolated virtual environment, avoiding
Ubuntu mirror downloads. The same executable formats generated resources.

The C++ resource generators apply the same formatter before writing or checking
headers. Install `clang-format-14`, use the formatter bundled with Arduino IDE 2
on macOS, or set `CLANG_FORMAT` to the formatter executable. The generated data
and embedded HTML/JavaScript/Python strings retain their original bytes.

### LittleFS storage bridge

Run `bash tests/run-littlefs-storage.sh` for sanitizer-enabled tests of both core
API variants, journal validation/repair/deduplication, interrupted writes, raw
files, mutations, format/mount errors, native-header exclusion and Portal config /
AppConfig persistence using a sketch-local backend define.

## Native ArdFS fragmentation

`bash tests/run-config-memory.sh` includes ESP8266 and ESP32 fault-injecting
flash fixtures. Fragmentation cases allocate a seven-sector file across three
separate holes and assert that neighboring files are not rewritten. They cover
bad cells, root relocation, reader pins, old-format rejection, stale catalogs,
uncertain publication/remount, zero capacity, and every I/O interruption with
torn operations. AddressSanitizer and UndefinedBehaviorSanitizer are enabled.

## Native ArdFS small write cache

`bash tests/run-config-memory.sh` exercises the writer with 4 KiB allocations
rejected, using a shared 256-byte cache plus 256-byte relocation scratch.
ESP8266/ESP32 fixtures cover late program failures after a verified prefix,
a second failed relocation destination, unreadable-prefix rollback, root commit
relocation, metadata spanning multiple pages, and unknown-length reservations
with a metadata prefix larger than one cache page. Existing power-cut sweeps
also run against this writer. The cache size is independent of the V4 metadata layout.

The native wear-leveling fixtures additionally check circular allocation across
restart, static-file bank cycling, reader/capacity maintenance deferral, rejection of missing wear fields, every I/O cut during maintenance, and a 300-sector
occupied range requiring multiple bounded allocation-scan steps. Test-only erase
counters distinguish data-sector distribution from metadata-sector wear.

`moving-metadata.cpp` covers moving catalog pairs across the whole bank,
reclamation of original header sectors for data, full/abandoned log handling,
program/read/erase/silent-erase retirement, catalogs with no first-slot record,
and rejection of independently constructed V1–V3 fixtures without writes. It stores CRC-valid fake
V4 and legacy catalogs in raw payloads, including continuation sectors 0/1;
head- and tail-only torn programs and single-bit type corruption must not
promote payload into catalog records. Native
header buffers carry explicit alignment; ASan/UBSan tests exercise aligned word
access alongside arbitrary byte/sector-table reads.

## MQTT protocol backends

```sh
python3 tests/mqtt-client-test.py
python3 tests/mqtt-backend-test.py
python3 tests/mqtt-packet-test.py
python3 tests/mqtt-api-test.py
```

The standalone client test executes CONNECT/CONNACK, publish framing, partial
writes, SUBACK/UNSUBACK, fragmented retained binary payloads, keepalive, timeout
and malformed-packet rejection under ASan/UBSan. The backend test checks actual
compiler dependencies for both backend selections and disabled MQTT, validates
the feature signature and exercises PubSubClient's header observer. Its small
PubSubClient fixture covers only the observer; real PubSubClient 2.8 is checked
with Arduino firmware builds. `run-config-memory.sh` also checks the selected
native client through portal state publishing and cooperative transmission.
