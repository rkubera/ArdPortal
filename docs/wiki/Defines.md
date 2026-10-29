# All compile-time defines

[Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home) · [All topics](https://github.com/rkubera/ArdPortal/wiki/Home#start-here) · [Source](https://github.com/rkubera/ArdPortal)

## On this page

- [Optional modules](#optional-modules)
- [Storage backend](#storage-backend)
- [Definition budget](#definition-budget)
- [Network memory reserves](#network-memory-reserves)
- [Zigbee](#zigbee-esp32-c5c6h2)
- [Matter](#matter)

## Optional modules

Set feature defines in the main `.ino`, before including `ArdPortal.h`.
Module flags default to `1` (enabled), except Zigbee, Matter, the Matter traffic
console and the log callback, which default to `0`. `ARDPORTAL_ENABLE_CONSOLE_MESSAGES`
follows `ARDPORTAL_ENABLE_LOG_CALLBACK` unless explicitly set. All enable flags
accept only `0` or `1`:

```cpp
#define ARDPORTAL_ENABLE_OTA 0
#define ARDPORTAL_ENABLE_MQTT 0
#include <ArdPortal.h>
```

| Define | Default / effect |
| --- | --- |
| `ARDPORTAL_ENABLE_WWW` | Defaults to `1`. Set `0` to stop HTTP/WWW and WebSockets, including HTTP API, web Console, uploads and HTTP converter downloads. Omits HTTP transport/buffers and captive DNS. Disables HTTP OTA; Zigbee OTA remains controlled by `ARDPORTAL_ENABLE_OTA`. Converter export through the C++ API remains available. Wi-Fi, AP, MQTT, Zigbee, storage and programmatic APIs remain available. |
| `ARDPORTAL_ENABLE_WIFI` | Defaults to `1`. Set `0` to disable station Wi-Fi and AP; forces WWW and MQTT off, including MQTT TLS/HA. Zigbee, storage and local AppConfig APIs remain available. Wi-Fi/AP and MQTT public APIs are unavailable. |
| `ARDPORTAL_ENABLE_ZIGBEE` | Defaults to `0`. Set `1` to enable native Zigbee on ESP32-C5/C6/H2 with a matching SDK/board mode. Set `0` to omit its adapter and endpoints. Independent of MQTT; cannot coexist with Matter. See [Zigbee options](#zigbee-esp32-c5c6h2). |
| `ARDPORTAL_ENABLE_MATTER` | Defaults to `0`. Set `1` to enable native Matter with a supported ESP32 SDK; transport is selected separately. Set `0` to omit its adapter and SDK dependency from ArdPortal. Requires dynamic pages; cannot coexist with Zigbee. See [Matter options](#matter). |
| `ARDPORTAL_ENABLE_MATTER_CONSOLE` | Defaults to `0`. Set `1` to add Matter traffic history and a dedicated web Console tab/WebSocket; requires Matter, WWW and Console. See [Matter diagnostics](#matter-command-console-and-lifecycle-messages). |
| `ARDPORTAL_ENABLE_POWER_MANAGEMENT` | Defaults to `1`. Set `0` to remove the power manager, GPIO/timer wake handling and `portal.power()` API. |
| `ARDPORTAL_ENABLE_OTA` | Removes OTA implementation and Update page. |
| `ARDPORTAL_ENABLE_MQTT` | Removes MQTT, MQTT Configuration, MQTT command handling and HA. Messages and native protocol diagnostics can remain enabled independently. |
| `ARDPORTAL_ENABLE_MQTT_TLS` | Removes secure MQTT transport and TLS/CA settings. Defaults to `1`; MQTT disabled forces it off. Stored TLS configurations require explicit reconfiguration, with no automatic plaintext fallback. |
| `ARDPORTAL_ENABLE_HA` | Removes HA discovery; local dynamic pages remain available. |
| `ARDPORTAL_ENABLE_CONSOLE_MESSAGES` | Enables lifecycle/power Messages. Defaults to the log callback flag. The web tab requires Console; the callback works without Console or WWW. |
| `ARDPORTAL_ENABLE_LOG_CALLBACK` | Defaults to `0`. Set `1` to enable `onLog()` for sketch-owned output independently of WWW. |
| `ARDPORTAL_ENABLE_CONSOLE` | Removes Console while keeping MQTT and HA enabled. |
| `ARDPORTAL_ENABLE_DYNAMIC_PAGES` | Removes dynamic forms, field renderers/validation and their HTTP/MQTT handling; forces all control flags, HA and dependencies off. |
| `ARDPORTAL_ENABLE_DEPENDENCIES` | Removes `visibleWhen` rules. Programmatic visibility remains independently available. |
| `ARDPORTAL_ENABLE_GROUPS` | Defaults to `0`. Set `1` to enable independent navigation groups and member-page tabs. See [Navigation groups](#navigation-groups). |
| `ARDPORTAL_ENABLE_VISIBILITY` | Defaults to `1`. Enables optional boolean `visible` on fields/pages and runtime `setFieldVisible()` / `setPageVisible()`. Independent of dependencies; disabled with dynamic pages. |

To omit dynamic forms entirely, add `#define ARDPORTAL_ENABLE_DYNAMIC_PAGES 0`
before `ArdPortal.h`. It defaults to `1`. `startAppConfigPageRegistration()`, `setAppConfigStateValue()`
and `emitAppConfigEvent()` then return false and `onAppConfigCommand()` is a no-op. Generic
`getAppConfigValue()`, `setAppConfigValue()`, `removeAppConfigValue()` and their
change callback remain available and persist through the selected storage backend. When Console is
also disabled, WebSocket handling and buffers are omitted as well.

Disabling MQTT automatically forces HA off; Console diagnostics can remain
enabled independently. No shared-header edits or companion build files are required.

The entry header compiles the portal implementation in the sketch translation
unit, so sketch defines apply to its modules and compressed web assets on both
ESP8266 and ESP32. Include `ArdPortal.h` only in the main `.ino`. Additional
application `.cpp` files can include `ArdPortalDeclarations.h` for declarations;
use the same feature defines before that include if they access portal objects.
The storage backend follows `ARDPORTAL_USE_LITTLEFS`.

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
individual flags take precedence when dynamic forms are enabled. Setting `ARDPORTAL_ENABLE_DYNAMIC_PAGES`
to `0` omits all field types. This switch
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

## Storage backend

`ARDPORTAL_USE_LITTLEFS` selects the filesystem for PortalConfig, AppConfig,
raw files and storage maintenance on ESP8266 and ESP32.

| Define | Default | Effect |
| --- | --- | --- |
| `ARDPORTAL_USE_LITTLEFS` | `0` | `0` selects native ArdFS; `1` selects the Arduino core LittleFS backend. Only `0` and `1` are accepted. |

```cpp
#define ARDPORTAL_USE_LITTLEFS 1
#include <ArdPortal.h>
```

Set the flag before including Portal headers, with the same value in every
application translation unit. The Portal configuration API and storage callbacks
apply to either backend. Standalone `ArdFS` uses the native filesystem.

LittleFS uses the board's filesystem region on ESP8266 and the default `spiffs`
partition on ESP32. Its volume must be formatted as LittleFS. Mount failure does
not automatically format the partition; explicit storage formatting erases its
contents. Do not mount overlapping flash regions with multiple filesystem
instances. See [Storage API](https://github.com/rkubera/ArdPortal/wiki/FS-API#selecting-littlefs)
and the [LittleFS example](https://github.com/rkubera/ArdPortal/blob/main/examples/LittleFS/LittleFS.ino).

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
translation units. See [Memory and limits](https://github.com/rkubera/ArdPortal/wiki/Memory-and-Limits) for reserve
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

## Matter

Matter is opt-in and independent of MQTT/Home Assistant Discovery. Define library
options before `#include <ArdPortal.h>` in the sketch. They select ArdPortal code;
they do not rebuild a precompiled SDK or add radio/ICD/factory-provider support.
Use an ESP32 board with the required Matter SDK and app/NVS partition layout.
ESP8266 is not supported. See the [Matter build guide](Matter).

### Library options

| Define | Default | Values and effect |
| --- | --- | --- |
| `ARDPORTAL_ENABLE_MATTER` | `0` | `1` includes the native Matter adapter, endpoint mapping, lifecycle and enabled portal/API integration. `0` excludes the adapter and its SDK dependency from ArdPortal. Requires `ARDPORTAL_ENABLE_DYNAMIC_PAGES=1`; cannot coexist with `ARDPORTAL_ENABLE_ZIGBEE=1`. |
| `ARDPORTAL_MATTER_THREAD` | `1` on ESP32-H2, otherwise `0` | `0`: Matter uses Wi-Fi and requires `ARDPORTAL_ENABLE_WIFI=1`. `1`: Matter uses Thread on ESP32-C5/C6/H2 with a Thread-enabled SDK. This selects one Matter transport; it does not mean both transports are used simultaneously. Thread can retain a separate Wi-Fi portal when the SDK supports it. |
| `ARDPORTAL_MATTER_PRODUCTION` | `0` | `0`: SDK development credentials and development pairing-code options. `1`: factory credential providers; requires a source SDK built for production with test setup disabled. This does not generate/install certificates or certify the device. Select through `ARDPORTAL_CREDENTIALS` in source builds. |
| `ARDPORTAL_MATTER_MAX_ENDPOINTS` | `16` | Integer `1..64`: upper limit on mapped native control endpoints, not the number of portal fields or total SDK memory. Raising it allows larger layouts but does not guarantee enough flash/heap; unsupported fields do not gain mappings. |
| `ARDPORTAL_MATTER_THREAD_ROUTER` | SDK FTD selection for Matter Thread; otherwise `0` | `1`: compile for router-capable FTD; `0`: MTD/end-device build. Must match the SDK. FTD requires ICD and IEEE 802.15.4 radio sleep disabled; invalid combinations fail compilation. It enables eligibility, not a forced router role. |
| `ARDPORTAL_ENABLE_MATTER_CONSOLE` | `0` | `1`: add the human-readable Matter traffic tab and its dedicated read-only WebSocket/history. Requires Matter, WWW and Console; normalized to `0` when those prerequisites are disabled. Separate from lifecycle Messages. |

All boolean defines accept only `0` or `1`. Invalid transport/device-type choices
fail compilation rather than silently selecting another radio. On ESP32-C5 with
the precompiled Arduino core, the **Matter Network** board menu must also match
`ARDPORTAL_MATTER_THREAD`.

### Minimal Arduino selection

```cpp
// Before #include <ArdPortal.h>:
#define ARDPORTAL_ENABLE_MATTER 1
#define ARDPORTAL_MATTER_THREAD 0 // 0: Matter Wi-Fi; 1: Matter Thread
#define ARDPORTAL_ENABLE_MQTT 0   // Optional; Matter does not need MQTT
#define ARDPORTAL_ENABLE_HA 0     // Disables MQTT Discovery, not HA Matter support
#include <ArdPortal.h>
```

Leave `ARDPORTAL_ENABLE_DYNAMIC_PAGES` enabled (default `1`). Individual
`ARDPORTAL_ENABLE_CONTROL_*` flags include/exclude their native endpoint code.
`ARDPORTAL_ENABLE_CLASSES=1` (default) is needed to retain explicit class-based
mappings such as brightness and measurement sensors. A class does not add a
mapping for an unsupported control; see [control mappings](Matter#mappings).
Disabling WWW removes web pages/APIs but does not require disabling Matter.

### Source SDK settings are not sketch defines

Pass these CMake settings to `idf.py` when building `idf/MatterPortal`. They select
SDK configuration fragments and matching library settings. Use a fresh build
directory when changing transport, credential, power or Thread device type.

| CMake setting | Default | Choices |
| --- | --- | --- |
| `ARDPORTAL_PROFILE` | `wifi` | `wifi`: Matter and portal over Wi-Fi; `thread`: Matter Thread without Wi-Fi; `thread-wifi`: Matter exclusively over Thread with a separate Wi-Fi portal. |
| `ARDPORTAL_CREDENTIALS` | `development` | `development`: SDK test credentials; `production`: own certificates and commissioning data provisioned into the dedicated `fctry` partition. |
| `ARDPORTAL_POWER` | `off` | `off`: no extra SDK PM/ICD support; `connected`: connected saving for Wi-Fi; `icd`: Short Idle Time ICD for Thread or Thread + Wi-Fi portal. Compiling support does not by itself change the default runtime `AlwaysOn` policy. |
| `ARDPORTAL_THREAD_ROLE` | `mtd` | `mtd`: end device; `router`: router-capable FTD for Thread profiles with power `off`. The network determines whether its current role is Child, Router or Leader. |

For example, build Matter over Thread with an always-listening router-capable
radio and a separate Wi-Fi portal:

```sh
idf.py -C idf/MatterPortal -B build-thread-router \
  -DARDPORTAL_PROFILE=thread-wifi \
  -DARDPORTAL_THREAD_ROLE=router -DARDPORTAL_POWER=off \
  -DIDF_TARGET=esp32c6 build
```

Do not force `ARDPORTAL_MATTER_THREAD_ROUTER=1` in a sketch backed by an MTD SDK:
the define cannot replace an FTD build. Similarly, setting the production flag
cannot replace factory provisioning. See [credential profiles](Matter#credential-profiles),
[Thread device mode](Matter#thread-device-mode-and-routing) and
[power management](Matter#power-management) for the full requirements.

### Matter command console and lifecycle Messages

```cpp
// Optional additions before #include <ArdPortal.h>:
#define ARDPORTAL_ENABLE_MATTER_CONSOLE 1 // Commands and reported control values
#define ARDPORTAL_ENABLE_CONSOLE_MESSAGES 1 // Pairing, connectivity and power decisions
```

The traffic tab is **Console → Matter**. It shows readable commands,
accepted/rejected outcomes and application state updates sent to Matter.
`ARDPORTAL_ENABLE_CONSOLE_MESSAGES` is independent and controls lifecycle messages;
it normally defaults to `0`, or `1` when `ARDPORTAL_ENABLE_LOG_CALLBACK=1` unless
explicitly overridden. The MatterPortal example enables both diagnostic views.
Turning the traffic tab off removes its web history/socket; a separately enabled
log callback can still receive Matter traffic without WWW.

### Log callback without WWW

`ARDPORTAL_ENABLE_LOG_CALLBACK` defaults to `0`. Enable it before including
`ArdPortal.h` to expose `portal.onLog()` and `ArdPortal::LogChannel`:

```cpp
#define ARDPORTAL_ENABLE_WWW 0
#define ARDPORTAL_ENABLE_LOG_CALLBACK 1
#include <ArdPortal.h>

// After Serial.begin(), before portal.begin():
portal.onLog([](ArdPortal::LogChannel channel, const String& message) {
  if (Serial.availableForWrite() >= int(message.length() + 5)) {
    Serial.print(unsigned(channel));
    Serial.print(": ");
    Serial.println(message);
  }
});
```

Channels are `Messages`, `Mqtt`, `Matter`, and `Zigbee` (in that order, 0–3).
The sink receives raw text before WWW timestamps/truncation and before history
memory checks. Its callback must not block; it runs synchronously during portal
work, never directly from a Matter/Zigbee SDK event handler. The message reference
is valid only for the call. Register a noncapturing function/lambda; `onLog(nullptr)`
detaches it. Logging recursively inside the sink is not forwarded back to it.
Register before `begin()` to include startup diagnostics.

Enabling the callback defaults `ARDPORTAL_ENABLE_CONSOLE_MESSAGES` to `1`; an
explicit `0` still removes Messages (including Matter lifecycle/power diagnostics).
MQTT/Matter/Zigbee traffic follows its module define. The callback also enables
Matter traffic formatting independently of `ARDPORTAL_ENABLE_MATTER_CONSOLE`,
which remains a WWW-only tab. Disabling WWW excludes console histories and
WebSockets; native Zigbee keeps only its bounded SDK event/traffic queues.
Disabling the callback removes its handler pointer and dispatch guard entirely.
This API does not intercept arbitrary `Serial` or SDK ESP_LOG output.
See `examples/UsbDiagnostics/UsbDiagnostics.ino`.

### Navigation groups

`ARDPORTAL_ENABLE_GROUPS` defaults to `0`. Set it to `1` to register independent
JSON groups and display their pages as tabs.

```cpp
#define ARDPORTAL_ENABLE_GROUPS 1
#include <ArdPortal.h>
```

Register a group with `{"group":{"id":"settings","name":"Settings"}}`
through `startAppConfigPageRegistration()`, then register member pages with
`"group":"settings"` alongside their page properties. Wait for each registration
callback before starting the next job. Groups can also be registered after their
pages. An unregistered group leaves its member pages accessible by URL but absent
from navigation.

The group replaces its member pages in the main menu and links to its first
visible page; members appear as tabs with their own `/p/<page-id>` URLs. A group
with no visible members is hidden. Group and page `order` values control their
positions; equal values preserve registration order. Navigation groups do not
change scene field grouping.

With `0`, independent group registration is rejected and page `group` metadata
is ignored, so pages use ordinary navigation. Group code and metadata storage
are compiled out. With `1`, up to 16 groups share a 16 KiB serialized metadata
limit, separately from page and field limits.

See [Dynamic Pages](https://github.com/rkubera/ArdPortal/wiki/Dynamic-Pages#navigation-groups-optional)
for complete JSON examples.

### Programmatic visibility

`ARDPORTAL_ENABLE_VISIBILITY` defaults to `1`. It enables the boolean `visible`
property on pages and fields (omitted means `true`) and these runtime methods:

```cpp
portal.setPageVisible("advanced_settings", true);
portal.setFieldVisible("advanced_option", false);
```

Call the methods after successful registration. They return `false` for a null
or unknown ID and `true` for an existing ID, including when no change is needed.
Changes are RAM-only; restart restores the JSON defaults. Connected portal
clients update without a reload. Hidden fields keep their values.

Visibility combines the field flag, its page flag and any `visibleWhen` rules:
all must allow the field. Setting `visible=true` does not bypass a false
condition. This feature is independent of `ARDPORTAL_ENABLE_DEPENDENCIES`;
programmatic visibility works with dependencies disabled. HA discovery follows
visibility; Zigbee and Matter endpoints remain registered.

Set `ARDPORTAL_ENABLE_VISIBILITY=0` to compile out the setters and programmatic
visibility handling. Registration then rejects an explicit `visible` property,
even `visible:true`. Disabling dynamic pages forces visibility off as well.
Groups have no visibility flag or setter; hide/show their member pages instead.

Use identical feature flags in every translation unit. In PlatformIO, place
project-wide definitions in `build_flags`, for example:

```ini
build_flags =
    -DARDPORTAL_ENABLE_GROUPS=1
    -DARDPORTAL_ENABLE_VISIBILITY=1
```

See [Dynamic Pages](https://github.com/rkubera/ArdPortal/wiki/Dynamic-Pages#programmatic-visibility)
and [Dependent Fields](https://github.com/rkubera/ArdPortal/wiki/Dependent-Fields).

### Page registration limit

`ARDPORTAL_APP_CONFIG_MAX_PAGES` defaults to `16`. Set an integer from `1` to
`1024` before including ArdPortal (use the same value in every translation unit).
This configures page slots independently of the group limit and the shared
1024-field/entity limit. See [Dynamic Pages](Dynamic-Pages.md).
