# Pull request checks

The `PR checks` workflow runs on pull requests (including forks), pushes to `main`
and manual dispatch. Its three stable check names are `Host tests`,
`Arduino builds` and `Lint and assets`. Jobs have read-only repository access and
no repository secrets. Actions are pinned to commit hashes; tool/core/API-test
versions are explicit in the workflow.

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
ESP32-C5/C6/H2. It also compiles the public API feature-switch matrix on ESP8266.
Temporary build directories are cleaned on exit. `ARDUINO_CLI` can select a local
CLI executable. These builds do not upload firmware or contact devices.

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
