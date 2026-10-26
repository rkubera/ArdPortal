# Shared control model validation

Date: 2026-10-23. Branch: `codex/control-model-classes`.

## Scope

The catalogue covers all 30 field types and 119 semantic classes; 12 types have optional classes. Matter has a separate optional adapter; see `Matter.md` and `tests/matter-fields-test.py`. `class` can be omitted for every field type.

## Automated checks

- `bash tests/run-host.sh` with ASan/UBSan: passed, including persistence, allocation failures, runtime state, HTTP streaming, frontend and Zigbee lifecycle/OTA/exports.
- `tests/control-model-test.py`: passed for every class/type pair, optional classes on all types, HA-independent normalization, capabilities, calendar/IR/events and disabled names in compiled profiles. Common JSON structural keys such as temperature/humidity remain in ArdJSON independently of control classes.
- Real zigbee-herdsman-converters 26.115.1 and Zigbee2MQTT 2.14.2 Discovery mapper: passed, including outlet, numeric class/unit and scalar On/Off light.
- Real zha 2.3.1, zigpy 2.3.0 and zha-quirks 2.3.0: passed.
- Generated language/control/portal/Z2M/ZHA resources: freshness checks passed. Arduino clang-format 14 check passed.
- `tests/compile-arduino.sh`: passed on ESP8266 3.1.2 and ESP32 3.3.12, covering DynamicPages ESP8266/ESP32, ZigbeeClimate C5/C6/H2, public API feature matrix and network-free ESP32 symbol checks.
- Additional final builds: full DynamicPages ESP8266/ESP32, ZigbeeClimate C6 and DynamicPages ESP8266 with `ARDPORTAL_ENABLE_CLASSES=0`: passed.

## Firmware and static RAM

Identical Arduino CLI/core/FQBN/example for the ESP8266 comparison. The baseline was built from the original Git HEAD in a temporary checkout. Values are compiler output; RAM includes initialized data, read-only constants assigned to RAM and BSS. These are not runtime peak-heap measurements.

| ESP8266 DynamicPages configuration | Flash bytes | Static RAM bytes | Delta flash / RAM |
| --- | ---: | ---: | ---: |
| Original HEAD | 542720 | 43172 | — |
| Shared model, classes enabled | 547524 | 43432 | +4804 / +260 |
| Shared model, classes disabled | 540740 | 42964 | -1980 / -208 |

`ArdControl::Model` is statically limited to four bytes. The Zigbee `Spec` remains at most 24 bytes: the one-byte class ID uses the existing layout padding. Class names, units, masks and adapter class names reside in program memory with per-control guards. No per-protocol copy of AppConfig state was introduced.

## Limits and migration

This validation does not establish physical RF behavior or actuator operation. Generic Analog/Binary endpoints remain explicitly generic; additional classes do not imply new native clusters. IR transceiver Discovery currently exposes its transmitter side; use separate transmitter/receiver fields to discover both.

Replace legacy properties and regenerate/re-interview gateway layouts as described in [Control classes](../wiki/Control-Classes.md). No legacy aliases are retained.
