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
