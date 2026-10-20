# Pull request checks

The `PR checks` workflow runs on pull requests (including forks), pushes to `main`
and manual dispatch. Its three stable check names are `Host tests`,
`Arduino builds` and `Lint and assets`. Jobs have read-only repository access and
no repository secrets. Actions are pinned to commit hashes; tool/core/API-test
versions are explicit in the workflow. Only GitHub-owned actions are used;
Arduino CLI/Lint are downloaded from official releases with pinned SHA-256 hashes.

## Host tests

Install Python 3.12, Node.js 24, a C++17 compiler and `clang++`. Run:

```sh
bash tests/run-host.sh
```

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

The script builds DynamicPages for ESP8266 and ESP32, and ZigbeeClimate for
ESP32-C5/C6/H2. It also compiles the public API feature-switch matrix on ESP8266
and the network-free profile on ESP32, checking that its linker map excludes
Wi-Fi driver symbols.
Temporary build directories are cleaned on exit. `ARDUINO_CLI` can select a local
CLI executable. For ESP8266 builds only, a narrow Python warning filter hides
core 3.1.2's known `elf2bin.py` non-raw `\s` regex warning under Python 3.12.
Other Python and compiler warnings remain enabled; caller `PYTHONWARNINGS`
filters take precedence. These builds do not upload firmware or contact devices.

## Feature size measurements

Run `python3 tests/measure-features.py --platform esp8266 --output /tmp/ardportal-sizes`
and repeat with `--platform esp32` and `--platform esp32c6`. Use `--cli` to select
Arduino CLI. Each profile records its parent, complete feature flags, application
binary size, static RAM, compiler log and linker map. Controls are measured one
at a time with identical WWW/MQTT/HA prerequisites; Zigbee uses C6 because the
classic ESP32 and ESP8266 do not support it. Numeric limits are also probed.
`--only PROFILE...` selects cases and `--resume` skips successful measurements.
Profiles reuse a build directory per platform; concurrent jobs must use separate
`--work-root` directories and disjoint `--only` profiles. `--library-root` selects a source
snapshot for before/after comparisons. Static RAM excludes runtime heap and stack.
For faster ESP32 runs, `--application-only` omits bootloader/merged-image packaging
while retaining normal application compilation, linking and `.bin` generation.

`tests/report-feature-sizes.py --before BEFORE --after AFTER --output OUTPUT`
renders Markdown, CSV and JSON. See the measured
[feature size report](../docs/testing/Feature-size-report.md).

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
