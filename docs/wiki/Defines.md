# All compile-time defines

[Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home) · [All topics](https://github.com/rkubera/ArdPortal/wiki/Home#start-here) · [Source](https://github.com/rkubera/ArdPortal)

## On this page

- [Optional modules](#optional-modules)
- [Definition budget](#definition-budget)
- [Network memory reserves](#network-memory-reserves)

## Optional modules

Set feature defines in the main `.ino`, before including `ArdPortal.h`.
Module flags default to `1` (enabled), except `ARDPORTAL_ENABLE_CONSOLE_MESSAGES`,
which defaults to `0`. All enable flags accept only `0` or `1`:

```cpp
#define ARDPORTAL_ENABLE_OTA 0
#define ARDPORTAL_ENABLE_MQTT 0
#include <ArdPortal.h>
```

| Define | Disabled behavior |
| --- | --- |
| `ARDPORTAL_ENABLE_WWW` | Defaults to `1`. Set `0` to stop HTTP/WWW and WebSockets, including HTTP API, web Console, uploads and HTTP converter downloads. Omits HTTP transport/buffers and captive DNS. Disables HTTP OTA; Zigbee OTA remains controlled by `ARDPORTAL_ENABLE_OTA`. Converter export through the C++ API remains available. Wi-Fi, AP, MQTT, Zigbee, storage and programmatic APIs remain available. |
| `ARDPORTAL_ENABLE_WIFI` | Defaults to `1`. Set `0` to disable station Wi-Fi and AP; forces WWW and MQTT off, including MQTT TLS/HA. Zigbee, storage and local AppConfig APIs remain available. Wi-Fi/AP and MQTT public APIs are unavailable. |
| `ARDPORTAL_ENABLE_POWER_MANAGEMENT` | Defaults to `1`. Set `0` to remove the power manager, GPIO/timer wake handling and `portal.power()` API. |
| `ARDPORTAL_ENABLE_OTA` | Removes OTA implementation and Update page. |
| `ARDPORTAL_ENABLE_MQTT` | Removes MQTT, MQTT Configuration, Console and HA. |
| `ARDPORTAL_ENABLE_MQTT_TLS` | Removes secure MQTT transport and TLS/CA settings. Defaults to `1`; MQTT disabled forces it off. Stored TLS configurations require explicit reconfiguration, with no automatic plaintext fallback. |
| `ARDPORTAL_ENABLE_HA` | Removes HA discovery; local dynamic pages remain available. |
| `ARDPORTAL_ENABLE_CONSOLE_MESSAGES` | Enables the Messages diagnostic tab when set to `1`; defaults to `0`, requires Console. |
| `ARDPORTAL_ENABLE_CONSOLE` | Removes Console while keeping MQTT and HA enabled. |
| `ARDPORTAL_ENABLE_DYNAMIC_PAGES` | Removes dynamic forms, field renderers/validation and their HTTP/MQTT handling; forces all control flags, HA and dependencies off. |
| `ARDPORTAL_ENABLE_DEPENDENCIES` | Removes `visibleWhen` evaluation, conditional portal visibility and HA discovery removal/recreation. |

To omit dynamic forms entirely, add `#define ARDPORTAL_ENABLE_DYNAMIC_PAGES 0`
before `ArdPortal.h`. It defaults to `1`. `startAppConfigPageRegistration()`, `setAppConfigStateValue()`
and `emitAppConfigEvent()` then return false and `onAppConfigCommand()` is a no-op. Generic
`getAppConfigValue()`, `setAppConfigValue()`, `removeAppConfigValue()` and their
change callback remain available and persist through ArdFS. When Console is
also disabled, WebSocket handling and buffers are omitted as well.

MinimalPortal enables OTA and disables MQTT and dynamic forms. Disabling MQTT automatically forces HA and
Console off. No shared-header edits or companion build files are required.

The entry header compiles the portal implementation in the sketch translation
unit, so sketch defines apply to its modules and compressed web assets on both
ESP8266 and ESP32. Include `ArdPortal.h` only in the main `.ino`. Additional
application `.cpp` files can include `ArdPortalDeclarations.h` for declarations;
use the same feature defines before that include if they access portal objects.
ArdFS remains separately compiled because it has no portal feature switches.

### Optional field types

Each dynamic field type has its own `ARDPORTAL_ENABLE_CONTROL_<TYPE>` flag.
Set it before including `ArdPortal.h`; every type defaults to `1`. A `0` removes
its field validation, preset/discovery data, JavaScript renderer, CSS and exclusive
language strings. Shared code or captions remain when another enabled field or
built-in page still needs them. Turning off the standalone `switch` does not
remove the power switch inside an enabled `light`.

For a small portal with only four field types:

```cpp
#define ARDPORTAL_ENABLE_CONTROLS 0
#define ARDPORTAL_ENABLE_CONTROL_SLIDER 1
#define ARDPORTAL_ENABLE_CONTROL_TEXT 1
#define ARDPORTAL_ENABLE_CONTROL_SELECT 1
#define ARDPORTAL_ENABLE_CONTROL_EDIT 1
#include <ArdPortal.h>
```

`ARDPORTAL_ENABLE_CONTROLS` changes the default for unspecified control flags;
individual flags take precedence when dynamic forms are enabled. `ESP01-1MB`
uses this four-control selection. `MinimalPortal` instead sets
`ARDPORTAL_ENABLE_DYNAMIC_PAGES` to `0`, omitting all field types. This switch
also overrides explicitly enabled control, HA and dependency flags. OTA, MQTT
and Console can still be enabled independently. Disabled field definitions are
rejected by `startAppConfigPageRegistration()`; they are not silently converted into another type.
Guard your own JSON definitions and registration calls too when they contain
excluded types; literals owned by the sketch cannot be removed by the library.

| Field type | Compile-time flag |
| --- | --- |
| `slider` | `ARDPORTAL_ENABLE_CONTROL_SLIDER` |
| `text` | `ARDPORTAL_ENABLE_CONTROL_TEXT` |
| `select` | `ARDPORTAL_ENABLE_CONTROL_SELECT` |
| `edit` | `ARDPORTAL_ENABLE_CONTROL_EDIT` |
| `switch` | `ARDPORTAL_ENABLE_CONTROL_SWITCH` |
| `climate` | `ARDPORTAL_ENABLE_CONTROL_CLIMATE` |
| `binary_sensor` | `ARDPORTAL_ENABLE_CONTROL_BINARY_SENSOR` |
| `date` | `ARDPORTAL_ENABLE_CONTROL_DATE` |
| `time` | `ARDPORTAL_ENABLE_CONTROL_TIME` |
| `datetime` | `ARDPORTAL_ENABLE_CONTROL_DATETIME` |
| `button` | `ARDPORTAL_ENABLE_CONTROL_BUTTON` |
| `scene` | `ARDPORTAL_ENABLE_CONTROL_SCENE` |
| `notify` | `ARDPORTAL_ENABLE_CONTROL_NOTIFY` |
| `infrared` | `ARDPORTAL_ENABLE_CONTROL_INFRARED` |
| `event` | `ARDPORTAL_ENABLE_CONTROL_EVENT` |
| `device_trigger` | `ARDPORTAL_ENABLE_CONTROL_DEVICE_TRIGGER` |
| `tag` | `ARDPORTAL_ENABLE_CONTROL_TAG` |
| `device_tracker` | `ARDPORTAL_ENABLE_CONTROL_DEVICE_TRACKER` |
| `light` | `ARDPORTAL_ENABLE_CONTROL_LIGHT` |
| `fan` | `ARDPORTAL_ENABLE_CONTROL_FAN` |
| `cover` | `ARDPORTAL_ENABLE_CONTROL_COVER` |
| `valve` | `ARDPORTAL_ENABLE_CONTROL_VALVE` |
| `lock` | `ARDPORTAL_ENABLE_CONTROL_LOCK` |
| `alarm_control_panel` | `ARDPORTAL_ENABLE_CONTROL_ALARM_CONTROL_PANEL` |
| `humidifier` | `ARDPORTAL_ENABLE_CONTROL_HUMIDIFIER` |
| `siren` | `ARDPORTAL_ENABLE_CONTROL_SIREN` |
| `vacuum` | `ARDPORTAL_ENABLE_CONTROL_VACUUM` |
| `lawn_mower` | `ARDPORTAL_ENABLE_CONTROL_LAWN_MOWER` |
| `water_heater` | `ARDPORTAL_ENABLE_CONTROL_WATER_HEATER` |
| `update` | `ARDPORTAL_ENABLE_CONTROL_UPDATE` |

Graphical on/off fields use `switch`. Numeric editable ranges use `slider`, which
maps to an HA `number`; read-only numbers and strings use `text`, which maps to an
HA `sensor`. Both scalar types accept optional `ha` properties and measurement
metadata such as `unit_of_measurement`. Device-specific HA entities remain
separate because their commands, states and discovery contracts differ.

Web assets stream from flash without constructing a combined buffer in RAM.
Compiler-selected DEFLATE blocks form a single gzip stream with one checksum
and footer. The HTTP response has the exact total size, and the browser decodes
it as one CSS, JavaScript or JSON document. No additional compression profile is
needed for a custom selection of controls.

## Definition budget

| Define | Default | Unit and behavior |
| --- | --- | --- |
| `ARDPORTAL_APP_CONFIG_MAX_DEFINITION_BYTES` | `256UL * 1024UL` | Total source bytes shared by panel pages and HA-only entities. Must be 1–UINT32_MAX. No buffer of this size is allocated. It does not change persistent AppConfig capacity or the 1024-field limit. |

```cpp
#define ARDPORTAL_APP_CONFIG_MAX_DEFINITION_BYTES (128UL * 1024UL)
#include <ArdPortal.h>
```

## Network memory reserves

| Define | ESP32 default | ESP8266 default | Meaning |
| --- | ---: | ---: | --- |
| `ARDPORTAL_NETWORK_HEAP_RESERVE` | 24576 | 4096 | Free byte-addressable internal heap to preserve beyond estimated work |
| `ARDPORTAL_NETWORK_DMA_RESERVE` | 16384 | Not measured/enforced | Additional DMA-capable free-memory check on ESP32; overlaps heap8 |
| `ARDPORTAL_NETWORK_BLOCK_RESERVE` | 4096 | 1024 | Largest contiguous block headroom beyond a work allocation |

Values are bytes. Set them before the library include consistently in all
translation units. See [Memory and limits](https://github.com/rkubera/ArdPortal/wiki/Memory-and-Limits) for measured
behavior and why lowering these values can starve networking. Runtime timing
settings belong in `ArdPortal::Options`, not in defines.

There are no supported sketch defines for raising the fixed page count,
registered-field count, persistent AppConfig size, JSON hard depth or MQTT packet
capacity. Those are source constants. Per-operation JSON limits and portal timing
are configured through `Limits` and `Options` respectively.

---

[Back to documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

## Zigbee (ESP32-C5/C6/H2)

`ARDPORTAL_ENABLE_ZIGBEE` defaults to **0**. Set it to **1** to include the
native Zigbee network operations, supported control mappings, and optional WWW management/Console with a dedicated WebSocket. ESP32-H2 forces Wi-Fi/AP/WWW/MQTT off.
`ARDPORTAL_ZIGBEE_ROUTER` defaults to **0** (End Device); **1** selects Router.
Choose the matching ED/ZCZR board mode. Both flags accept only 0 or 1. Zigbee
is independent of MQTT and HA. Its log tab requires `ARDPORTAL_ENABLE_CONSOLE`; Messages additionally requires `ARDPORTAL_ENABLE_CONSOLE_MESSAGES`.
See [Zigbee](https://github.com/rkubera/ArdPortal/wiki/Zigbee) for partitions,
automatic boot pairing/rejoin, retries, callbacks and reset behavior.

### Zigbee OTA image identity

| Define | Default | Purpose |
| --- | --- | --- |
| `ARDPORTAL_ZIGBEE_OTA_MANUFACTURER` | `0xffff` | Manufacturer identifier matched against Zigbee OTA images. |
| `ARDPORTAL_ZIGBEE_OTA_IMAGE_TYPE` | `1` | Product/image type matched against OTA images. |
| `ARDPORTAL_ZIGBEE_OTA_FILE_VERSION` | `1UL` | Application firmware's current numeric OTA version; increment for new images. |

These apply to the native `update` mapping when Zigbee, dynamic pages, the update
control and OTA are enabled. See [Zigbee firmware updates](https://github.com/rkubera/ArdPortal/wiki/Zigbee#firmware-updates-over-zigbee).

### Zigbee Serial diagnostics

`ARDPORTAL_ZIGBEE_DEBUG_SERIAL` defaults to **0**. Define it as **1** before
including `ArdPortal.h` to mirror Zigbee lifecycle, received APS frames, queued
reports and command results to `Serial`. The sketch must call `Serial.begin()`.
It works without the Console WebSocket. Writes run from the Arduino loop, not
SDK callbacks. If the complete line does not fit the available Serial transmit
buffer, that Serial copy is skipped rather than waiting; Console history remains.

```cpp
#define ARDPORTAL_ENABLE_ZIGBEE 1
#define ARDPORTAL_ZIGBEE_DEBUG_SERIAL 1
#include <ArdPortal.h>
// In setup(): Serial.begin(115200);
```

A queued TX report is not a coordinator acknowledgement.

### MQTT Serial diagnostics

`ARDPORTAL_MQTT_DEBUG_SERIAL` defaults to **0**. Define it as **1** before
including `ArdPortal.h` to send MQTT connection attempts, connection results,
disconnect reasons and MQTT RX/TX traffic to `Serial`. Call `Serial.begin()`
in the sketch. This is independent of `ARDPORTAL_ENABLE_CONSOLE` and
`ARDPORTAL_ENABLE_CONSOLE_MESSAGES`; MQTT must be enabled. No credentials are
logged by the connection diagnostics. Topic payloads are included in traffic
diagnostics. Lines that cannot fit the available transmit buffer are skipped
rather than blocking the portal loop. A queued publication is not a broker
acknowledgement.

```cpp
#define ARDPORTAL_MQTT_DEBUG_SERIAL 1
#include <ArdPortal.h>
// In setup(): Serial.begin(115200);
```

### Zigbee2MQTT converter download

`ARDPORTAL_ENABLE_ZIGBEE_CONVERTER` defaults to **1**. Converter generation
and the download button on the Zigbee page are enabled automatically. Set it to
**0** before including `ArdPortal.h` to disable them. It requires Zigbee and dynamic pages to be enabled. Disabled
firmware does not include the generator or its download state.

```cpp
#define ARDPORTAL_ENABLE_ZIGBEE 1
#define ARDPORTAL_ENABLE_ZIGBEE_CONVERTER 1
#include <ArdPortal.h>
```

See [Zigbee converter download](https://github.com/rkubera/ArdPortal/wiki/Zigbee#zigbee2mqtt-converter-download).

### ZHA quirk download

`ARDPORTAL_ENABLE_ZHA_QUIRK` defaults to **1**, independently of the Zigbee2MQTT
converter define. It requires Zigbee and dynamic pages. Set it to **0** before
including `ArdPortal.h` to remove the Python exporter. A ZHA-only build can use:

```cpp
#define ARDPORTAL_ENABLE_ZIGBEE 1
#define ARDPORTAL_ENABLE_ZIGBEE_CONVERTER 0
#define ARDPORTAL_ENABLE_ZHA_QUIRK 1
#include <ArdPortal.h>
```

Both exporters share endpoint metadata and the same layout model identifier.
See [ZHA quirk download](https://github.com/rkubera/ArdPortal/wiki/Zigbee#zha-quirk-download).


## WWW and Wi-Fi combinations

```cpp
// No web server, with Wi-Fi/AP and MQTT controlled through the C++ API.
#define ARDPORTAL_ENABLE_WWW 0
#include <ArdPortal.h>
```

```cpp
// Zigbee-only networking: no Wi-Fi, AP, WWW or MQTT.
#define ARDPORTAL_ENABLE_WIFI 0
#define ARDPORTAL_ENABLE_ZIGBEE 1
#include <ArdPortal.h>
```

Wi-Fi disabled overrides even explicit `ARDPORTAL_ENABLE_WWW=1` and
`ARDPORTAL_ENABLE_MQTT=1`. It disables Wi-Fi radio use by ArdPortal, NTP over
Wi-Fi and Wi-Fi/Zigbee coexistence setup. Existing saved network settings are
preserved; re-enabling Wi-Fi in a later firmware can use them again. Both flags
are compile-time options, not runtime switches. Continue `begin()`/`loop()` for
storage, AppConfig and Zigbee. Disabling WWW removes HTTP OTA. Zigbee OTA remains available when
`ARDPORTAL_ENABLE_OTA=1` and the Zigbee update control is enabled. An AP with WWW disabled can still run,
but it cannot serve the configuration interface.


## Public API follows enabled features

Calls to disabled features are compile errors, rather than silent no-ops or
methods returning false. The corresponding public callback types are omitted.

| API | Required feature |
|---|---|
| Wi-Fi/AP control, scans, callbacks, status/IP and policy types | Wi-Fi |
| MQTT component, message callback, wrappers and status | MQTT |
| `tlsClockReady()` | MQTT TLS |
| Page registration, progress/error/callbacks and `setAppConfigStateValue()` | Dynamic pages |
| `addAppConfigEntity()` | Dynamic pages and HA |
| `queueAppConfigStatePublish()` | Dynamic pages and MQTT |
| `onAppConfigCommand()` and its callback type | Dynamic pages and at least one action-capable control |
| `emitAppConfigEvent()` | Dynamic pages and at least one event/device-trigger/tag control |
| `log()` / `logMessage()` | Console Messages |
| Zigbee methods/component/types | Zigbee |
| OTA component | OTA |

Generic stored AppConfig get/set/remove methods and storage callbacks remain
available without dynamic pages or networking. `PortalConfig` retains its saved
settings independently of which transports are enabled. WWW disabled does not
remove APIs needed to control enabled Wi-Fi/AP/MQTT/Zigbee from the sketch. Some
status/logging methods remain private helpers for internal operation; they are
not callable from application code when their feature is disabled.

Converter/quirk defines gate both their API and format-specific implementation.
WWW additionally gates only HTTP download support; disabling both exporter defines
removes the public export API. See [Zigbee export API](https://github.com/rkubera/ArdPortal/wiki/Zigbee#export-converters-through-the-c-api-without-www).

ESP32-H2 automatically forces Wi-Fi, AP, WWW and MQTT off. Native Zigbee
support is available in End Device and Router roles.

## Typed text measurements

`ARDPORTAL_ENABLE_CLASSES` defaults to `1`. Set it to `0` to omit optional semantic class names, units and automatic classifications. All controls still work without `class`. Each catalogue entry also follows its applicable `ARDPORTAL_ENABLE_CONTROL_*` flags; slider classes remain available when text controls are disabled. See [Control classes](Control-Classes).
