# Zigbee validation and tested scope

[Zigbee guide](https://github.com/rkubera/ArdPortal/wiki/Zigbee) · [Control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls)

## Hardware and software

Tests on 15–16 October 2026 used an ESP32-C5 with 8 MiB PSRAM, Arduino-ESP32 3.3.12 and Zigbee2MQTT 2.14.2. The test fixture registers 22 application fields across 48 application endpoints, with an additional native OTA endpoint. Application actuator callbacks simulate states; these tests verify transport, value validation, callback routing and state synchronization, not physical motors, valves or locks.

## Control exchanges

Settings, telemetry and actions were tested using the generated layout converter. Read/write controls were exercised in both directions where the mapping supports them. Telemetry was changed to a different value to verify an actual report rather than a cached gateway value. Read-only sensors and status endpoints were tested as reports; they do not become writable through a converter. Unsupported operations remain listed on each control page.

The tested families include switch, slider, select, numeric/temperature/humidity text, binary sensor, RGB light, thermostat, cover, lock, fan, valve, humidifier, water heater, alarm panel, siren, vacuum, mower, action, scene and presence. Thermostat retests include large single-target changes, independent `heat_cool` thresholds and mode OFF. Action tests include repeated invocation and simulated post-command state. Arbitrary vacuum JSON commands and the lock OPEN latch operation are not native Zigbee operations.

Host regression suites cover lifecycle/retries, endpoint staging, mappings and typed values, bounded queues, newer-command ordering, report retry/defer behavior, scheduler capacity, time synchronization, converter output and OTA parsing/validation/restart. The JavaScript exporter is also tested with the actual zigbee-herdsman-converters API. ZHA exporter tests are API-level checks; no equivalent physical ZHA result is claimed here.

## OTA results

| Run | Result | Interpretation |
| --- | --- | --- |
| Initial Wi-Fi run | Failed: false activity timeout while frames were arriving | Timestamp ordering/rollover bug reproduced in a regression test and corrected |
| Retry with timeout correction | Failed at 82.1% after a real 60-second reception gap | Communication loss; underlying trigger was not established |
| Further Wi-Fi retry | Failed at 56.33% with SDK `mac/mac.c:2539`, resolved to `zb_mac_ack_timeout` | SDK assertion path identified; triggering cause remains unproven |
| Wi-Fi disabled, 15 October | **Passed**, 16:05:18–18:21:09 Europe/Warsaw, 1,538,126 bytes | Validated image, callback, version-2 boot, rejoin and post-update controls |
| Wi-Fi enabled, 15 October | Stopped by user at last reported 72.04% | Interrupted test, not a transfer failure or pass |
| Wi-Fi/WWW enabled, 16 October | **Passed**, 10:10:20–12:37:22 Europe/Warsaw, 1,670,878 bytes | Gateway success 1→2, callback, version-2 boot, Wi-Fi/Zigbee rejoin and post-update controls |

Both passing runs include an independent USB firmware-version read and switch checks in both directions. Climate was changed to heat and then OFF remotely and verified in application state; the Wi-Fi-enabled run also verified OFF through the Portal HTTP API. Transfer completion alone was not treated as proof of firmware activation.

## What the results establish

The tested implementation can complete OTA with and without Wi-Fi while retaining the same control layout and pairing. The images differ in size; these runs are not a size-matched performance benchmark. Earlier intermittent failures were not reproduced in the final Wi-Fi-enabled run. Merely enabling Wi-Fi is therefore not an established cause, and neither unstable USB power nor antenna movement was proven by the recorded evidence. Preserve serial and gateway logs when investigating another failure.

These results do not certify every gateway, chip, role or compile-time combination. C6/H2 and Router behavior require their own hardware validation. Application callbacks must implement real hardware behavior, and gateway frontends may display composite controls differently. See the [coverage matrix](https://github.com/rkubera/ArdPortal/wiki/Zigbee#complete-current-control-coverage) for current functional limits.
