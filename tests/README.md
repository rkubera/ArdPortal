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
formatting without modifying files. CI runs this check.

The C++ resource generators apply the same formatter before writing or checking
headers. Install `clang-format-14`, use the formatter bundled with Arduino IDE 2
on macOS, or set `CLANG_FORMAT` to the formatter executable. The generated data
and embedded HTML/JavaScript/Python strings retain their original bytes.
