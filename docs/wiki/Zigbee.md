# Zigbee

[Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home) · [Defines](https://github.com/rkubera/ArdPortal/wiki/Defines)

## Enable on ESP32-C5/C6/H2

This module is in the working tree and is not included in the 0.7.8 release.
Validated by compilation with Arduino-ESP32 **3.3.12** and host tests; physical
coordinator interoperability and Wi-Fi/Zigbee coexistence still require device testing.

This optional module uses the ESP32-C5/C6/H2 native radio and the Zigbee SDK bundled
with Arduino-ESP32. It is disabled by default. The implementation provides network management, diagnostics and automatic
standard-cluster mapping for supported AppConfig controls. Dynamic form definitions
are not transmitted to gateway applications.

After losing all active links, a device with saved network identity schedules a
rejoin after 30 seconds. If that operation exhausts its retry or timeout limits,
another rejoin is scheduled after 30 seconds. Recovery preserves network data
and runs cooperatively in `loop()`; HTTP remains available. Unpaired devices
also retry pairing after each unsuccessful series, with the same 30-second pause,
until joining succeeds. The coordinator must allow joining. Stop pairing cancels
both active attempts and scheduled retries, including during the pause. A factory
reset does not schedule automatic retries in the current session.

```cpp
#define ARDPORTAL_ENABLE_ZIGBEE 1
#define ARDPORTAL_ZIGBEE_ROUTER 0 // 0 = End Device, 1 = Router
#include <ArdPortal.h>
```

Supported native targets are **ESP32-C5, ESP32-C6 and ESP32-H2**. Select the actual chip’s
board in Arduino IDE. Fallback model identity is `ArdPortal-C5`, `ArdPortal-C6` or `ArdPortal-H2`;
manufacturer stays `ArdPortal`. Use the matching chip firmware for Zigbee OTA.
The C5/C6 partition tables in the examples are for 4 MiB flash or larger, with the
same flash layout. For CLI commands below, replace `esp32c6` with `esp32c5` for C5.
ESP32-C3 remains unsupported for native Zigbee; ESP32-H2 automatically disables Wi-Fi/AP, WWW and MQTT.

Select **Zigbee ED** in Arduino IDE for End Device, or **Zigbee ZCZR** for Router.
A role/mode mismatch is a compile error. The same role must be used in all
translation units. A single ArdPortal instance owns the native Zigbee stack;
do not also start Arduino's `Zigbee` singleton or define another
`esp_zb_app_signal_handler` in the sketch.

Use the **ZigbeePortal** example on a board with at least **4 MiB flash**. Keep its
`partitions.csv` with the sketch: it supplies two **1792 KiB OTA slots**, **320 KiB
ArdFS**, dedicated `zb_storage` and `zb_fct`, and a coredump partition. The default
Arduino Zigbee table has 1280 KiB app slots, too small for the full ArdPortal build.
In Arduino IDE select **Minimal SPIFFS (1.9MB APP with OTA)** to avoid its smaller
size check; the sketch-local CSV overrides the actual partition layout. For CLI:

```sh
arduino-cli compile --fqbn esp32:esp32:esp32c6:ZigbeeMode=ed,PartitionScheme=zigbee \
  --build-property upload.maximum_size=1835008 examples/ZigbeePortal
```

For Router use `ZigbeeMode=zczr,PartitionScheme=zigbee_zczr` and set
`ARDPORTAL_ZIGBEE_ROUTER=1` before the include (or via the compiler build flags).
Changing partition layout requires a serial firmware upload, not an OTA update.
Do not reuse flash contents from another layout without deliberately migrating
or clearing them.

ESP32-C5/C6 share radio resources between Wi-Fi and Zigbee. Router operation needs
continuous reception. Espressif marks Wi-Fi STA + Zigbee Router as unstable
and concurrent Wi-Fi SoftAP + Router as unsupported. Configure station Wi-Fi
before relying on Router operation; a fallback AP is not a supported simultaneous
Router mode.
Test the actual workload and coordinator. This End Device implementation is
always awake, with RX-on-when-idle explicitly enabled so incoming commands do
not wait for the next end-device poll. It does not implement battery sleep while
serving a Wi-Fi portal. The 3-second keep-alive remains a parent-link maintenance
setting; it is not an intentional application command delay.
See Espressif coexistence documentation for [C6](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-guides/coexist.html) and [C5](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c5/api-guides/coexist.html).

## Zigbee page

Open **Zigbee** in the portal navigation, or `/zigbee` directly. It shows the
compiled role, network state, saved pairing, channel, PAN ID, attempts and errors.

- **Pairing starts automatically** and retries until joined. Enable joining on the coordinator; no Start pairing control is required.
  Stop cancels network steering and scheduled retries. It does not erase an
  existing identity or undo a join which has already completed.
- **Rejoin saved network** preserves Zigbee identity and credentials.
- **Reset pairing and pair again** clears only Zigbee network data and then
  starts joining. It preserves Wi-Fi, PortalConfig and AppConfig.
- **Factory reset** from the main portal also clears Zigbee before restarting;
  it does not begin another pairing attempt before restart. Ordinary restart,
  OTA restart and storage formatting do not clear Zigbee pairing.

At boot, a new device automatically attempts pairing. A previously paired device
attempts rejoin. Defaults per series: **60-second timeout**, **15-second attempt timeout**,
**3-second retry spacing**, **up to 3 attempts**. After a failed series, another
starts after **30 seconds**, with no overall limit. Each series delivers its result
through the operation callback. Initialization and the stack scheduler run in a
separate task; callbacks are delivered from `portal.loop()`. Keep calling it.

## API

| Method | Meaning |
| --- | --- |
| `startZigbeePairing()` | Accept a new-network join; returns `false` if unavailable, busy or already paired. |
| `stopZigbeePairing()` | Accept pairing cancellation; stops retries without clearing saved data. |
| `rejoinZigbee()` | Accept rejoin of a known network; rejects a device without saved pairing. |
| `resetZigbeePairing()` | Accept Zigbee-only reset followed by new pairing. |
| `setZigbeeOptions(options)` | Validate and set limits before startup or the next request; returns `false` for invalid limits. Timings: 1–600000 ms; attempts: 1–20. |
| `zigbeeState()` | Read cached `Starting`, `Idle`, `Pairing`, `Rejoining`, `Resetting`, `Connected` or `Failed` state. |
| `onZigbeeStateChanged(callback)` | `void(ZigbeeState)` for state transitions from the Arduino loop. |
| `onZigbeeOperationFinished(callback)` | `void(ZigbeeOperation, bool success, const String& error)` once per accepted operation, including automatic boot operations. |

```cpp
portal.onZigbeeOperationFinished([](ArdPortal::ZigbeeOperation operation,
                                    bool success, const String& error) {
  // Operations: Pair, Rejoin, ResetPairing, FactoryReset.
  // Copy error if needed after this callback. Do not block here.
});
ArdPortal::ZigbeeOptions options;
options.timeoutMs = 60000;
options.attemptTimeoutMs = 15000;
options.retryMs = 3000;
options.maxAttempts = 3;
portal.setZigbeeOptions(options);
portal.begin();
```

`true` from a command means accepted, not connected. Rejected requests do not
produce a completion callback. Stop reports `success=false` with `Pairing stopped`.
Full factory reset cancels a pending network operation and reports that cancellation
before completing its Zigbee reset. A reset error is diagnostic; it does not cancel
the portal's restart. A timeout does not erase network credentials; a late native
join result may still update connection state without repeating the callback.

## Dedicated console

The Zigbee connection status appears after the IP address in the shared header on every
portal page. The Zigbee page retains network details and pairing controls without
duplicating that connection indicator.

The **Zigbee tab in Console** opens **`/api/zigbee/console`**, a dedicated WebSocket with its own
session, cursor and 16-record history. It does not use `/api/console` (MQTT/Messages)
or `/api/events` (AppConfig). It works with MQTT and HA disabled. Enable `ARDPORTAL_ENABLE_CONSOLE` to show the console; enable `ARDPORTAL_ENABLE_CONSOLE_MESSAGES` separately for Messages. The Zigbee network page only polls network/operation status.

The console displays lifecycle diagnostics and decoded Zigbee payloads as JSON.
When `ARDPORTAL_ENABLE_CONSOLE_MESSAGES` is enabled, pairing/rejoin attempts,
successful pairing or rejoin, link loss, retry scheduling and operation failures
also appear in Messages. Frame payloads and SDK signal traffic remain in the
dedicated Zigbee tab. This forwarding does not replace application callbacks.
Timestamps follow the same rules as Messages: device uptime before the clock is
valid, then UTC record timestamps displayed in the browser's local timezone and
selected language. Existing uptime records retain their original timestamps.
Incoming application frames include a metadata record (source, endpoints, profile,
cluster, LQI and APS status), followed by decoded ZCL records associated with the
transaction sequence. Frames containing multiple attributes produce one record
per attribute instead of allocating a complete JSON tree.

```text
RX payload={"src":0,"fromEp":1,"ep":2,"profile":260,"cluster":6,"LQI":112,"status":0}
RX ep=2 seq=134 payload={"cmd":"default_response","to":"report_attributes","status":"SUCCESS"}
RX applied ep=2 payload={"cluster":6,"attr":0,"type":16,"value":true}
TX queued ep=3 payload={"cmd":"report_attributes","cluster":13,"attr":85,"type":57,"value":20.5}
```

The decoder covers standard read/write/report attributes and their responses,
reporting configuration, attribute discovery and supported On/Off, level, color,
cover, lock and warning commands. Lock PINs are redacted. Unsupported profiles,
commands or scalar formats are explicitly marked `decoded:false`; truncated
frames are identified without reading beyond the received buffer. Raw hexadecimal
previews are not displayed.

Values use native cluster units (a thermostat setpoint of `2050` means 20.50 °C).
`TX queued` means the SDK accepted the report for transmission, not a coordinator
acknowledgement. A received default response with `SUCCESS` confirms the matching
ZCL operation. The MQTT JSON assembled by Zigbee2MQTT is not available on the ESP.

This is application/stack diagnostics, not an over-the-air sniffer. Arbitrary
SDK-generated TX frames (such as its automatic read responses) and routed MAC
traffic are not intercepted. History and queues are bounded; older records can
be discarded under load. Log records are emitted without dynamic allocation in
SDK callbacks, then transferred to the WebSocket from `loop()`.

HTTP resources: `GET /api/zigbee/status`, `POST /api/zigbee/pair`,
`POST /api/zigbee/stop`, `POST /api/zigbee/rejoin`, and
`POST /api/zigbee/reset` with body `confirm=RESET-ZIGBEE`.

Endpoint 1 is reserved for the network-management configuration tool; native
application endpoints are described below.

## Automatic native controls

With Zigbee and dynamic pages enabled, initial AppConfig fields are automatically
mapped to standard ZCL endpoints. No MQTT topics or HA discovery settings are
needed. The implementation works with MQTT and HA disabled.

| AppConfig type | Standard mapping | Supported features |
| --- | --- | --- |
| `switch` | On/Off | Read/write boolean state |
| `slider` | Analog Output | Numeric setting, min/max/resolution |
| `select` | Multistate Output | Write an option's one-based index; report selected index |
| `text`, `textType: "number"` | Analog Input | Read-only numeric measurement |
| `text`, `textType: "temperature"` | Temperature Measurement | Celsius in signed hundredths |
| `text`, `textType: "humidity"` | Relative Humidity Measurement | Percent in hundredths |
| Other measurement `text` subtypes | Standard measurement clusters | Pressure, illuminance, flow, CO₂, PM2.5, wind speed, AC voltage/current/power; see [text subtypes](https://github.com/rkubera/ArdPortal/wiki/Control-text#text-subtypes-and-automatic-patterns) |
| `binary_sensor` | Binary Input | Read-only boolean measurement |
| `light` | On/Off, Level Control, Color Control | Power, brightness and RGB via standard hue/saturation |
| `climate` | Thermostat, optional Fan Control | Current/target Celsius temperature, standard system/fan modes, heating/cooling state |
| `cover` | Window Covering | Open/close/stop, lift percentage and tilt percentage |
| `lock` | Door Lock | Lock/unlock commands and actual lock state |
| `siren` | IAS Warning Device | Start/stop warning, duration and four standard volume levels |
| `update` | OTA Upgrade client | Receive and validate a matching Zigbee firmware image, then schedule portal restart |

Plain `text` (omitted `textType` or `textType: "text"`) does not create a Zigbee
endpoint. Other supported text subtypes select a standard measurement explicitly,
including when their initial reading is `null`. HA metadata does not select the
Zigbee subtype. Other unsupported control types retain their portal/HA behavior.

Register initial pages in `setup()` or chain their registration-finished callbacks.
ArdPortal waits for ongoing registrations and one second without new fields, then
stages one field per loop before starting Zigbee. Standard endpoints are frozen
before stack startup: fields added afterwards need a device restart to appear in
Zigbee. Keep initial registration order stable to retain endpoint numbers. Endpoint
1 is reserved for network management; mapped fields start at endpoint 2. The native
mapping limit is 239 application endpoints. A limit/allocation failure stops Zigbee
startup and records a diagnostic; it does not delete AppConfig fields.

For a focused slider + thermostat example, use **File → Examples → ArdPortal →
ZigbeeClimate** ([sketch](https://github.com/rkubera/ArdPortal/blob/main/examples/ZigbeeClimate/ZigbeeClimate.ino)).
It disables MQTT/HA, registers one page asynchronously, simulates room temperature
with a RAM-only slider, persists thermostat target/mode edits, and reports Celsius
current temperature/heating activity without saving telemetry. Keep its partition
table and use the ESP32-C5/C6/H2 role/board settings described above. No sensor or relay
hardware is required; generic Analog Output slider display depends on the gateway.

The included **ZigbeePortal** example registers native controls from flash and shows
application callbacks. Preserve the enabled control types used by the example.
For real sensors, call `setAppConfigStateValue()` with current readings. For settings,
`setAppConfigValue()` updates the common state and schedules ArdFS persistence.
Changes from the portal, MQTT or application API update Zigbee attributes as well.

Zigbee commands invoke `onAppConfigValueChanged()` or `onAppConfigCommand()` with
`ChangeSource::Zigbee`, from the Arduino loop. The application still drives hardware.
Lock/unlock and cover open/close/stop invoke the matching action callback; accepted
commands do not fabricate a successful physical movement. Report actual state with
`setAppConfigStateValue()`. Percentage settings use existing value-change callbacks.
Zigbee writes obey `persist`; `persist: false` remains RAM-only. Flash commits do not
execute an already accepted Zigbee command a second time.

Reports are sent after joining and when values change. A changed field marks only
its mapped endpoint; unrelated telemetry does not restart a complete publication
pass. Work is bounded to one
application command and one field's attributes per loop, with network heap/DMA
reserves and a retry delay on transport/memory pressure. Startup attribute buffers
and labels are freed after the SDK has copied them. The dedicated Zigbee console
includes endpoint registration, accepted/rejected commands, queued attribute reports
and OTA lifecycle diagnostics.

Standard protocol support does not guarantee an identical control or automatic
exposure by every gateway. In particular, generic Analog/Binary/Multistate clusters
may need gateway support or a device converter. Option names are not currently
published as a Multistate StateText array. Custom replacement `controls` must retain
the property/action names used by the presets to receive corresponding commands.
`visibleWhen` and page dependency rules do not affect Zigbee: all supported
controls remain registered and visible in the gateway, including fields hidden
in ArdPortal. Zigbee commands for these fields remain accepted, subject to value
validation and read-only restrictions. This does not create a dynamic Zigbee form.

### Complete current control coverage

“Composite” means one ArdPortal field is split into several independent standard
endpoints. A native light/climate/cover can also use several clusters, but is not
split by this generic decomposition. Endpoint counts below are for the built-in
presets; replacing `controls` can change them. No supplied converter is needed for
the library’s transport, but gateway UI support for generic clusters is not guaranteed.

| Control | Current Zigbee mapping | Scope / exclusions |
| --- | --- | --- |
| [slider](https://github.com/rkubera/ArdPortal/wiki/Control-slider#integration-with-zigbee) | Native / generic Analog Output | Value writes/reports, min/max/step; float32 and gateway support limits. |
| [text](https://github.com/rkubera/ArdPortal/wiki/Control-text#integration-with-zigbee) | Native measurements; plain text unmapped | number, temperature, humidity, pressure, illuminance, flow, CO₂, PM2.5, wind speed, AC voltage/current/power. Plain text has no endpoint. |
| [select](https://github.com/rkubera/ArdPortal/wiki/Control-select#integration-with-zigbee) | Native / generic Multistate Output | Option index writes/reports; names/translations not sent as StateText. |
| [edit](https://github.com/rkubera/ArdPortal/wiki/Control-edit#integration-with-zigbee) | Not mapped | No editable-string endpoint. |
| [switch](https://github.com/rkubera/ArdPortal/wiki/Control-switch#integration-with-zigbee) | Native | On/off writes and reports. |
| [climate](https://github.com/rkubera/ArdPortal/wiki/Control-climate#integration-with-zigbee) | Native, multiple clusters | Target/current °C, standard modes and fan modes; heating/cooling bits. No presets, dual targets or fan/stage activity bits. |
| [binary_sensor](https://github.com/rkubera/ArdPortal/wiki/Control-binary_sensor#integration-with-zigbee) | Native / generic Binary Input | Read-only boolean; no automatic IAS/occupancy semantic classification. |
| [date](https://github.com/rkubera/ArdPortal/wiki/Control-date#integration-with-zigbee) | Not mapped | No calendar-date setting endpoint. |
| [time](https://github.com/rkubera/ArdPortal/wiki/Control-time#integration-with-zigbee) | Not mapped | No time-of-day setting endpoint. |
| [datetime](https://github.com/rkubera/ArdPortal/wiki/Control-datetime#integration-with-zigbee) | Not mapped | No arbitrary timestamp setting endpoint. |
| [button](https://github.com/rkubera/ArdPortal/wiki/Control-button#integration-with-zigbee) | Generic, one momentary endpoint | ON invokes PRESS; reports idle OFF. May appear as a switch. |
| [scene](https://github.com/rkubera/ArdPortal/wiki/Control-scene#integration-with-zigbee) | Generic, one momentary endpoint | ON invokes the scene callback; reports idle OFF. No Scenes scene table. |
| [notify](https://github.com/rkubera/ArdPortal/wiki/Control-notify#integration-with-zigbee) | Not mapped | No text notification transport. |
| [infrared](https://github.com/rkubera/ArdPortal/wiki/Control-infrared#integration-with-zigbee) | Not mapped | No arbitrary IR/JSON action transport. |
| [event](https://github.com/rkubera/ArdPortal/wiki/Control-event#integration-with-zigbee) | Not mapped | No application gesture/event stream. |
| [device_trigger](https://github.com/rkubera/ArdPortal/wiki/Control-device_trigger#integration-with-zigbee) | Not mapped | No Zigbee mapping for MQTT automation-trigger metadata. |
| [tag](https://github.com/rkubera/ArdPortal/wiki/Control-tag#integration-with-zigbee) | Not mapped | No tag scan/identifier transport. |
| [device_tracker](https://github.com/rkubera/ArdPortal/wiki/Control-device_tracker#integration-with-zigbee) | Generic, one status endpoint | Read-only 1=not_home, 2=home; no location/zone/GPS data. |
| [light](https://github.com/rkubera/ArdPortal/wiki/Control-light#integration-with-zigbee) | Native, multiple clusters | On/off, brightness, RGB via XY or hue/saturation; quantization, no CCT/effects. |
| [fan](https://github.com/rkubera/ArdPortal/wiki/Control-fan#integration-with-zigbee) | Composite — 4 endpoints | Power, speed percentage, oscillation, direction. Separate generic controls; no native Fan Control endpoint. |
| [cover](https://github.com/rkubera/ArdPortal/wiki/Control-cover#integration-with-zigbee) | Native, multiple clusters | OPEN/CLOSE/STOP, position/tilt; no separate movement-state report or absolute encoder commands. |
| [valve](https://github.com/rkubera/ArdPortal/wiki/Control-valve#integration-with-zigbee) | Composite — 4 endpoints | Open/close action endpoints, position, discrete state; no STOP. |
| [lock](https://github.com/rkubera/ArdPortal/wiki/Control-lock#integration-with-zigbee) | Native | LOCK/UNLOCK and state; no OPEN latch action or PIN management. |
| [alarm_control_panel](https://github.com/rkubera/ArdPortal/wiki/Control-alarm_control_panel#integration-with-zigbee) | Composite — 6 endpoints | Five arming/disarm/trigger action endpoints plus discrete state; no IAS ACE/PIN/zone handling. |
| [humidifier](https://github.com/rkubera/ArdPortal/wiki/Control-humidifier#integration-with-zigbee) | Composite — 4 endpoints | Power, target humidity, mode, current humidity; no unified humidifier profile. |
| [siren](https://github.com/rkubera/ArdPortal/wiki/Control-siren#integration-with-zigbee) | Native, partial | Start/stop warning, duration, four volume levels; alarm tone only, no application-state reporting. |
| [vacuum](https://github.com/rkubera/ArdPortal/wiki/Control-vacuum#integration-with-zigbee) | Composite — 8 endpoints | Six action endpoints, fan selection and activity; custom JSON command is not mapped. |
| [lawn_mower](https://github.com/rkubera/ArdPortal/wiki/Control-lawn_mower#integration-with-zigbee) | Composite — 4 endpoints | START/PAUSE/DOCK and activity; no navigation or battery telemetry. |
| [water_heater](https://github.com/rkubera/ArdPortal/wiki/Control-water_heater#integration-with-zigbee) | Composite — 3 endpoints | Target temperature, operation mode, current temperature; no unified heater profile. |
| [update](https://github.com/rkubera/ArdPortal/wiki/Control-update#integration-with-zigbee) | Native OTA client | One owner; matching image reception/validation and deferred restart. Version strings are not synchronized. |

### Composite controls without supplied converters

ArdPortal now decomposes `fan`, `valve`, `humidifier`, `water_heater`,
`alarm_control_panel`, `vacuum`, `lawn_mower`, `button` and `scene` into separate
standard endpoints. `device_tracker` reports its two preset presence states through
Multistate Input. No manufacturer-specific cluster or gateway converter is supplied.

| Control | Current separate endpoints / scope |
| --- | --- |
| [fan](https://github.com/rkubera/ArdPortal/wiki/Control-fan#integration-with-zigbee) | On/Off + Analog Output + On/Off + Multistate Output. Power, exact requested percentage (0–100), oscillation and forward/reverse direction are separate endpoints. Percentage is float32; direction is option 1=forward, 2=reverse. |
| [valve](https://github.com/rkubera/ArdPortal/wiki/Control-valve#integration-with-zigbee) | Two momentary On/Off actions + Analog Output + Multistate Input. Open/close actions are activated with ON, position is writable 0–100, and discrete state is reported: 1=closed, 2=opening, 3=open, 4=closing. |
| [humidifier](https://github.com/rkubera/ArdPortal/wiki/Control-humidifier#integration-with-zigbee) | On/Off + Analog Output + Multistate Output + Relative Humidity Measurement. Power, target humidity (0–100%), mode (1=normal, 2=eco in the preset), and measured current humidity are separate endpoints. |
| [water_heater](https://github.com/rkubera/ArdPortal/wiki/Control-water_heater#integration-with-zigbee) | Analog Output + Multistate Output + Temperature Measurement. Target temperature, selected operation mode and measured current temperature are separate endpoints. Preset mode indices: 1=off, 2=eco, 3=electric, 4=performance, 5=heat_pump, 6=high_demand. Temperatures are Celsius. |
| [alarm_control_panel](https://github.com/rkubera/ArdPortal/wiki/Control-alarm_control_panel#integration-with-zigbee) | Five momentary On/Off actions + Multistate Input. ON activates ARM_HOME, ARM_AWAY, ARM_NIGHT, DISARM or TRIGGER on its dedicated endpoint. Reported panel state: 1=disarmed, 2=armed_home, 3=armed_away, 4=armed_night, 5=pending, 6=arming, 7=triggered. |
| [vacuum](https://github.com/rkubera/ArdPortal/wiki/Control-vacuum#integration-with-zigbee) | Six momentary On/Off actions + Multistate Output + Multistate Input. ON activates start, stop, pause, return_to_base, locate or clean_spot. Fan speed is selected by ordinal (preset: 1=quiet, 2=normal, 3=turbo). Activity: 1=idle, 2=cleaning, 3=docked, 4=paused, 5=returning, 6=error. |
| [lawn_mower](https://github.com/rkubera/ArdPortal/wiki/Control-lawn_mower#integration-with-zigbee) | Three momentary On/Off actions + Multistate Input. ON invokes START, PAUSE or DOCK on its own endpoint. Activity: 1=docked, 2=mowing, 3=paused, 4=error. |
| [button](https://github.com/rkubera/ArdPortal/wiki/Control-button#integration-with-zigbee) | Momentary On/Off. Sending ON invokes the preset PRESS action through onAppConfigCommand. OFF does not execute the action. After processing, the endpoint reports OFF again. |
| [scene](https://github.com/rkubera/ArdPortal/wiki/Control-scene#integration-with-zigbee) | Momentary On/Off. Sending ON invokes the preset ON scene action through onAppConfigCommand. The endpoint returns to OFF after processing. |
| [device_tracker](https://github.com/rkubera/ArdPortal/wiki/Control-device_tracker#integration-with-zigbee) | Multistate Input. The preset presence state is read-only: 1=not_home, 2=home. The application updates it through the existing state setter. |

An action endpoint accepts ON and reports idle OFF after processing. It is not a
physical switch and OFF does not undo the action. Generic option/status values use
one-based indices; their names are listed on the control pages. Unsupported custom
text/JSON actions (for example vacuum’s custom command) are left unmapped.

Each endpoint carries its function in standard Basic ProductLabel, and generic
Analog/Multistate endpoints also have Description. Gateways may ignore those labels
or fail to expose those clusters. The lack of a supplied converter is intentional;
it does not guarantee that ZHA, Zigbee2MQTT or a commercial bridge automatically
shows every function. They remain parts of one physical Zigbee device.

Registration and publication advance one endpoint per loop, including composite
fields; retry/OOM behavior and selective invalidation are retained. Multiple
endpoints share one AppConfig value and callback route. The shared native endpoint
limit is 239, so composite fields use more slots than scalar fields.

### Portal pages and control order

Standard Zigbee descriptors expose endpoints, device types and clusters; they do
not transmit the ArdPortal navigation tree, dynamic forms or a UI sorting rule.
Current endpoint numbering follows **page/field registration order**, then the
normalized preset’s control order. This is not the portal’s page `order` sorting.
Read-only current-measurement and status endpoints follow the field’s controls.
Keep registration and control order stable to retain endpoint numbers.

ArdPortal publishes endpoint function labels; it does not encode page membership
or `order` as custom attributes. A gateway chooses grouping, naming and sorting.
Zigbee Groups group endpoints for addressing/multicast control, not navigation
pages. Recreating the portal’s exact pages/order would require explicit gateway
support or manually configuring its dashboard. [Zigbee endpoint model](https://docs.espressif.com/projects/esp-zigbee-sdk/en/release-v1.0/esp32c5/developing.html),
[Groups semantics](https://docs.espressif.com/projects/esp-zigbee-sdk/en/release-v1.0/esp32c5/user-guide/zcl_groups.html).

### Native feature limits

- Light supports RGB through XY and hue/saturation; RGB intensity is folded into the ZCL level. Incoming color/level commands normalize RGB to full intensity and use the brightness property for dimming. Color temperature and effects are not advertised.
- Climate supports representable Celsius values and standard modes listed in the field. Unknown fan modes are not translated into invented Zigbee modes.
- Cover percentages are inverted between the portal (100% open) and ZCL (0% open). Absolute encoder-position commands are not implemented.
- Lock supports lock/unlock; the portal's separate `OPEN` latch action and PIN-management functions are not exposed.
- Siren maps standard warning modes to the preset `alarm` tone. Its discrete volume is rounded to the preset's 0.1 step; the application must honor duration and report the resulting state.

## Possible mappings not yet implemented

The following is an **assessment of richer future mappings**. The composite
endpoints described above are implemented; the richer device-specific mappings
below are not. The cluster proposals are engineering
inferences from standard cluster semantics and the installed ESP Zigbee SDK.
Combining clusters can transport the properties without making the gateway
recognize one complete device automatically. A Zigbee2MQTT converter or equivalent
ZHA device handler may be needed to combine values, commands and events into the
intended UI. This does not change the current support status on each control page.

References to a converter in this future-extension table concern a richer combined
gateway UI or custom payload semantics. They are not a dependency of the currently
implemented generic endpoints listed above.

Repeated instances of the same cluster with the same role need separate endpoints;
a single endpoint cannot hold multiple independent Multistate Output instances.
Action transport must preserve repeated identical commands, rather than relying
only on a changed state attribute. Each field needs an explicit value/command
mapping and tests for the gateway role, reporting, storage and callback behavior.

| ArdPortal control | Feasibility | Proposed clusters/model | Remaining work / limits |
| --- | --- | --- | --- |
| [fan](https://github.com/rkubera/ArdPortal/wiki/Control-fan#integration-with-zigbee) | Current generic mapping; richer option feasible; composite + converter | Fan Control + On/Off + Analog Output + Binary Output + Multistate Output | Fan Control provides native off/low/medium/high/auto modes. Analog Output can preserve the exact requested percentage, Binary Output can carry oscillation, and Multistate Output can carry direction. Those properties need explicit gateway semantics; Fan Control alone cannot preserve the full ArdPortal fan. |
| [valve](https://github.com/rkubera/ArdPortal/wiki/Control-valve#integration-with-zigbee) | Current generic mapping; richer option feasible; composite + converter | On/Off + Analog Output + Multistate Input | On/Off can cover binary open/close, Analog Output can carry 0–100 position, and Multistate Input can report open/opening/closed/closing. A converter must combine them into a valve and keep action versus position-write behavior consistent. Level Control would usually suggest a dimmer, not automatically a valve. |
| [humidifier](https://github.com/rkubera/ArdPortal/wiki/Control-humidifier#integration-with-zigbee) | Current generic mapping; richer option feasible; composite + converter | On/Off + Relative Humidity Measurement + Analog Output + Multistate Output | These can carry power, current humidity, target humidity and normal/eco mode. The Dehumidification Control cluster describes removal of humidity; it should not be presented as a faithful native humidifier mapping. A gateway converter must combine the separate standard values into a humidifier. |
| [water_heater](https://github.com/rkubera/ArdPortal/wiki/Control-water_heater#integration-with-zigbee) | Current generic mapping; richer option feasible; composite + converter | Thermostat + Temperature Measurement + Multistate Output | Thermostat can provide a heating target/heat-off control, Temperature Measurement can report the reading, and Multistate Output can carry eco/electric/performance/heat-pump/high-demand selection. The ArdPortal modes need a documented mapping; the clusters do not automatically identify a complete HA water-heater entity. |
| [alarm_control_panel](https://github.com/rkubera/ArdPortal/wiki/Control-alarm_control_panel#integration-with-zigbee) | Current generic mapping; native extension; gateway role matters | IAS ACE; optionally IAS Zone and IAS Warning Device | IAS ACE supports disarm and day/home, night/sleep and all/away arming, panel status and emergency/panic/fire commands. A panel-side implementation needs the appropriate server commands, status responses/notifications and authorization behavior. IAS Zone/Warning Device are only needed when actual zones/sirens are exposed; they are not prerequisites for every panel. Do not assume keypad support in a gateway also supports an ArdPortal panel server. |
| [scene](https://github.com/rkubera/ArdPortal/wiki/Control-scene#integration-with-zigbee) | Current generic mapping; native extension; scene identity required | Scenes + Groups | Give each application scene a stable scene ID and group ID, handle Recall Scene and route it to the application callback. Sending scene activations from the device instead uses a client role/binding. Groups support multicast when used. Full scene-table add/store/remove behavior is extra work; a callback-only ArdPortal scene does not already implement it. |
| [button](https://github.com/rkubera/ArdPortal/wiki/Control-button#integration-with-zigbee) | Current momentary endpoint; richer command mapping possible | On/Off client commands or Scenes client/server commands | A device-originated press can send a standard bound command, but this differs from HA pressing a button to invoke a device action. The latter needs a documented incoming command/scene-ID mapping to PRESS and repeatable command handling. No combination automatically advertises a generic arbitrary-action button to every gateway. |
| [event](https://github.com/rkubera/ArdPortal/wiki/Control-event#integration-with-zigbee) | Partial standard mapping; custom gestures need converter | On/Off or Scenes client commands; optionally Multistate Input + event counter | Known actions can be sent as commands to bound peers/coordinator. Arbitrary press/double-press/long-press event names need a device converter and a repeat/event-sequence mechanism. A state attribute alone loses consecutive identical events; it is not equivalent to an event stream. |
| [device_trigger](https://github.com/rkubera/ArdPortal/wiki/Control-device_trigger#integration-with-zigbee) | Partial; host automation mapping required | On/Off or Scenes client commands | The device can emit standard commands; a gateway converter can expose them as automation triggers. HA MQTT device-automation type/subtype metadata cannot be sent unchanged as a standard Zigbee trigger definition, and all arbitrary triggers do not have native equivalents. |
| [device_tracker](https://github.com/rkubera/ArdPortal/wiki/Control-device_tracker#integration-with-zigbee) | Current generic mapping; further partial presence only; converter required | Binary Input or Multistate Input | Home/not-home or a fixed status list can be encoded, with the gateway assigning presence semantics. Occupancy Sensing is appropriate only when the field really measures occupancy, not arbitrary home/zone membership. Named locations, GPS coordinates and zone semantics need a custom model/converter; RSSI Location is not a drop-in HA device tracker. |
| [vacuum](https://github.com/rkubera/ArdPortal/wiki/Control-vacuum#integration-with-zigbee) | Current generic mapping; further partial composite; custom commands remain custom | Multistate Output + Multistate Input; optional On/Off and Power Configuration | Discrete action selection, fan-speed selection and reported activity can be carried on separate cluster instances/endpoints, with optional battery telemetry. Repeated actions need command/sequence semantics. The arbitrary JSON custom-command route and vacuum-specific behavior are not covered by those standard clusters; a device converter is necessary. |
| [lawn_mower](https://github.com/rkubera/ArdPortal/wiki/Control-lawn_mower#integration-with-zigbee) | Current generic endpoints; richer device UI/battery possible | Multistate Output + Multistate Input; optional Power Configuration | START/PAUSE/DOCK requests and reported activity can use discrete values, with repeat handling and separate endpoints as needed. A converter must create the mower meaning/UI. Standard generic clusters do not add navigation, zones, schedules or a universally recognized lawn-mower device. |
| [edit](https://github.com/rkubera/ArdPortal/wiki/Control-edit#integration-with-zigbee) | Custom attribute/protocol required | Writable ZCL character-string attribute | ZCL can carry a string, but there is no portable generic editable-text control from combining the existing On/Off/Analog/Multistate clusters. A manufacturer-specific attribute/model and gateway converter could preserve this value; bounds and read/write behavior must be defined. |
| [notify](https://github.com/rkubera/ArdPortal/wiki/Control-notify#integration-with-zigbee) | Custom command/protocol required | Character-string payload in a custom ZCL command | A notification message can be transported over Zigbee, but standard measurement/control clusters do not define the ArdPortal notify service. Delivery semantics, size limits, repeat handling and gateway presentation require a custom protocol/converter. |
| [infrared](https://github.com/rkubera/ArdPortal/wiki/Control-infrared#integration-with-zigbee) | Custom protocol required for the current JSON interface | Custom commands/attributes; optional tunneling in a compatible stack/gateway | Generic standard clusters cannot preserve the arbitrary IR-emitter JSON API or automatically make an IR controller. Predefined IR actions could be exposed as scenes/buttons, which is only a fixed subset. Full code transfer requires a documented device protocol, fragmentation limits and a gateway converter. |
| [date](https://github.com/rkubera/ArdPortal/wiki/Control-date#integration-with-zigbee) | Custom field model required | ZCL Date-typed attribute | Date is a standard ZCL data type, not a universally exposed calendar-setting cluster. A custom writable attribute can encode the calendar date with explicit conversion/range limits, but combining standard clusters does not recreate the existing date picker. Gateway support is required. |
| [time](https://github.com/rkubera/ArdPortal/wiki/Control-time#integration-with-zigbee) | Custom field model required | ZCL Time-of-Day-typed attribute | Time-of-day is a ZCL data type usable in a custom writable attribute. The Time cluster represents device/network time and is not an interchangeable container for arbitrary independent application time settings. There is no automatic generic time picker from a cluster combination. |
| [datetime](https://github.com/rkubera/ArdPortal/wiki/Control-datetime#integration-with-zigbee) | Partial for device clock; custom model for arbitrary fields | Time cluster for the device clock; custom UTC-time attribute for an application timestamp | The Time cluster can exchange actual device/network clock information. Independent AppConfig timestamps need their own attributes and a converter. ZCL UTC time has a fixed epoch/range and does not preserve the original offset or fractional-second representation. It cannot reproduce every accepted ArdPortal timestamp unchanged. |
| [tag](https://github.com/rkubera/ArdPortal/wiki/Control-tag#integration-with-zigbee) | Custom attribute/command required | Character/octet-string attribute or custom scan command | A tag identifier can be transmitted, but existing standard generic clusters do not describe arbitrary NFC/RFID scans as HA tags. Repeatable scans require event semantics; a converter must interpret the identifier and emit the host tag event. |
| Plain `text` | Custom read-only model only | Character-string attribute | No generic standard text-display control; this remains intentionally unmapped by default. |

The installed Fan Control model supplies discrete modes, not an exact percentage
or independent oscillation/direction fields; see [Fan Control](https://docs.espressif.com/projects/arduino-esp32/en/latest/zigbee/ep_fan_control.html).
The SDK exposes generic Analog/Multistate building blocks, but its Arduino
[Multistate documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/zigbee/ep_multistate.html)
explicitly warns about missing generic ZHA support. Native transport should not
be described as automatic UI compatibility.

The strongest direct standard candidates are [Scenes/Groups](https://docs.espressif.com/projects/esp-zigbee-sdk/en/release-v1.0/esp32c5/user-guide/zcl_scene.html)
and [IAS ACE](https://docs.espressif.com/projects/esp-zigbee-sdk/en/release-v1.0/esp32c5/api-reference/zcl/esp_zigbee_zcl_ias_ace.html).
Their client/server roles and command semantics still require implementation.
The [SDK cluster inventory](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/esp32/introduction.html)
is a protocol building-block reference, not proof that a gateway supports every
resulting ArdPortal device.

## Firmware updates over Zigbee

Only one `update` field can own the device's OTA client. This mapping also requires
`ARDPORTAL_ENABLE_OTA=1` and
`ARDPORTAL_ENABLE_CONTROL_UPDATE=1`. The coordinator must
serve a standard Zigbee OTA container containing a single **Upgrade Image** subelement
(tag 0) with the `.bin` compiled for the actual ESP32-C5 or ESP32-C6 target. A plain HTTP `.bin`/`.bin.gz` upload is a different flow.

Set these defines before including `ArdPortal.h` to match the coordinator's images:

```cpp
#define ARDPORTAL_ZIGBEE_OTA_MANUFACTURER 0xffff
#define ARDPORTAL_ZIGBEE_OTA_IMAGE_TYPE 1
#define ARDPORTAL_ZIGBEE_OTA_FILE_VERSION 2UL
```

Defaults are `0xffff`, `1` and `1UL`. The manufacturer value is an OTA image identifier;
use the value assigned to your product when applicable. Increment **the application's**
file version for each new image; it is independent of the ArdPortal library version.
The OTA client queries the coordinator at address 0, endpoint 1 after joining and sets
a ten-minute query interval. The `INSTALL` action requests a matching image through
this client when available, including when triggered from the portal.

The receiver handles fragmented subelement headers, checks length against the OTA
slot, validates firmware through ESP-IDF before selecting the new boot partition,
and aborts on errors or a 60-second receive timeout. Successful completion schedules
the usual `FirmwareUpdate` restart, including `onBeforeRestart()` and storage flushing.
The SDK callback does not call `ESP.restart()` directly. HTTP uploads and new AppConfig
writes are blocked while the native OTA receiver is active. The generic `update`
field's version strings are application metadata; they do not override these OTA
image identifiers. Native reception updates `in_progress` automatically in RAM,
without writing progress markers to ArdFS.

Physical pairing, command interoperability and OTA transfer still need validation
with a real coordinator; host tests and compilation do not establish gateway certification.

## Zigbee2MQTT converter download

Converter export is enabled by default (`ARDPORTAL_ENABLE_ZIGBEE_CONVERTER=1`).
Set it to `0` before including `ArdPortal.h` to disable it
([Defines](https://github.com/rkubera/ArdPortal/wiki/Defines)). The Zigbee page
then shows **Download Zigbee2MQTT converter** after endpoint registration and
stack initialization. The GET route is `/api/zigbee/converter.mjs`.

The download filename includes the layout signature, so converters for different
layouts can coexist in Zigbee2MQTT. The downloaded `.mjs` module is generated from the actual native endpoint
registry, one endpoint per HTTP fragment. The file is not retained in RAM.
Memory pressure postpones generation; normal HTTP timeout still applies.
The export includes control IDs, endpoint numbers, labels, numeric ranges,
select options, discrete states, climate modes and fan modes.

With this feature enabled, the Zigbee model identifier is `ArdPortal-` followed
by a deterministic 64-bit layout signature. The signature covers the exported
metadata and schema version, excluding live values and the device IEEE address.
All devices with identical exported layouts use the same model and converter,
including across supported chips and roles. Changed endpoint order, options,
ranges or labels produces a different model. Regenerate the converter and
refresh/re-interview an already paired device after changing its layout. The
signature is a layout identifier, not an authentication mechanism.

Install the downloaded module in Zigbee2MQTT's **Settings → Dev console →
External converters**, or in its `external_converters` directory. Current
Zigbee2MQTT installations may require `enable_external_js` to be enabled; see
[the official external converter instructions](https://www.zigbee2mqtt.io/advanced/more/external_converters.html).
ArdPortal does not install or change anything on the gateway. The generator
is tested with Zigbee2MQTT 2.14.2 / zigbee-herdsman-converters 26.115.1.

The converter adds named `select` options and Multistate status sensors,
respects climate ranges and modes, supports wind-speed measurements, and
scopes commands and standard state reports to each endpoint. Action endpoints
of the same AppConfig control are grouped into one command choice. Selecting
an option immediately sends the native command to its dedicated endpoint,
without an Execute button. The converter clears the transient selection after
sending, so the same action can be invoked again. WindFront may render short
choices as option buttons and longer choices as a dropdown; the converter does
not control this frontend layout. A standalone action remains a single-option
command (a button in HA Discovery). Composite
controls are still represented by separate standard features; this export
does not reconstruct dependent forms or promise a combined fan, vacuum, valve,
humidifier or water-heater entity. Plain text, unmapped controls and OTA image
management are not added by the converter. Climate still has one shared target
temperature for heating and cooling, matching ArdPortal's native mapping.

The download is refused while endpoint registration is incomplete or failed.
Adding fields after the Zigbee registry is frozen requires a firmware restart
before downloading a new converter.

## Memory behavior

Endpoint descriptors use compact report flags and retain labels only until the
SDK has copied them. Converter export reconstructs labels from field definitions,
so downloading after startup preserves the same metadata used for layout identity.
Composite definitions are normalized in place in their temporary parsed object;
registration and command handling do not retain a second deep copy.
Diagnostic history reuses its existing buffers and writes traffic records without
intermediate timestamp/message Strings. Queue sizes and supported controls are
unchanged. The common converter asset is compacted as a whole, independently of
the project's control selection.

## ZHA quirk download

A **quirk** is a Python device handler used by ZHA in Home Assistant. It adds
entity definitions and adjusts cluster interpretation; it is separate from a
Zigbee2MQTT JavaScript converter.

`ARDPORTAL_ENABLE_ZHA_QUIRK` defaults to **1** when Zigbee and dynamic pages
are available. The **Download ZHA quirk** button appears after registration
and stack startup. The GET route is `/api/zigbee/zha-quirk.py`; its filename
contains the layout signature. Set the define to **0** to omit this exporter.
The ZHA and Z2M exports use the same `ArdPortal-<signature>` model, including
when only one exporter is enabled. No IEEE address is embedded. One quirk
therefore matches all devices with identical exported control layouts.

Place the downloaded `.py` file in a custom quirks directory on Home Assistant,
configure ZHA's custom quirk loading, and restart Home Assistant. For example:

```yaml
zha:
  custom_quirks_path: /config/custom_zha_quirks
```

See the [official ZHA device handlers documentation](https://github.com/zigpy/zha-device-handlers).
The exporter uses the public `zhaquirks.builder` API and is tested with
**zha-quirks 2.3.0, zigpy 2.3.0 and zha 2.3.1**. It is not installed automatically.
Older API generations are not supported by this generated module. After a
layout change, download the matching quirk and refresh the device interview
so ZHA sees the new model and endpoints.

### Exported entities and limits

- Sliders become number entities with the application's minimum, maximum and step.
- Multistate Output becomes a select; Multistate Input becomes a named state sensor.
  ZHA displays underscores as spaces; colliding display names receive a numeric suffix,
  while writes still use the original one-based option number.
- Binary Input becomes a binary sensor. Action endpoints become command buttons;
  the application's action callback still determines whether an action is accepted.
- Typed text measurements become sensors in application units, with invalid wire
  readings converted to unavailable values. Ordinary text has no Zigbee endpoint.
- Standard switches, lights, covers, locks, sirens and thermostats use ZHA's native
  entities with the registered labels. Thermostat temperature limits are supplied
  locally by the quirk; ZHA chooses its temperature step and available modes from
  its native thermostat implementation. ArdPortal still has one shared heating/cooling target.
- Composite fan, valve, humidifier, alarm, vacuum, mower and water-heater controls
  remain separate entities. The quirk does not reconstruct pages or visibleWhen rules.
- Zigbee OTA image providers and unsupported controls are not added by this file.

The file is streamed one endpoint at a time using the shared export machinery,
without retaining a full Python program in device RAM.

### Names and frontend layout in Zigbee2MQTT

The converter labels standard-device child features with the registered control
name, because frontends may render these children without a parent heading.
A switch's state therefore uses its AppConfig name; climate properties use
`control name / property label`. Measurements and other simple features also
retain their AppConfig labels. Unique MQTT property suffixes and converter scopes retain the endpoint route.
The exported feature descriptions omit endpoint annotations, so the frontend
shows the control labels without `(Endpoint: ...)` suffixes.

A climate control is exported as one `climate` expose containing its properties.
WindFront renders these properties separately, even though HA Discovery can
create one climate entity. The converter cannot require a particular frontend
card layout. After updating an exporter, download and replace the `.mjs` file
in Z2M; updating firmware alone does not update the installed converter.
If the saved converter contains the new labels but the device still displays the
previous names, restart Zigbee2MQTT from **Settings → Tools → Restart Zigbee2MQTT**,
then reload its web interface. This was required when replacing the converter
with the same filename in Zigbee2MQTT 2.14.2. Removing or pairing the device again
is not required for a label-only converter update.


### Automatic time synchronization

ArdPortal adds a Time client cluster to management endpoint 1, without controls,
exposed functions or AppConfig fields. After connecting it probes the coordinator
(address 0, endpoint 1) for UTC time and TimeStatus. Only a successful standard
response from that endpoint with a master/synchronized status updates the system
clock. The Zigbee epoch is converted to Unix UTC; the existing configured time
zone continues to determine local time.

A successful synchronization is repeated after six hours. A missing, unsupported
or invalid response times out after ten seconds and is retried after one hour;
reconnecting triggers a fresh probe. Probing is asynchronous and does not block
HTTP, UART or control processing. The gateway must provide the standard Time
server on endpoint 1; pairing alone does not guarantee this support. Results
appear in diagnostics, without adding a visible Time control. Existing Wi-Fi
NTP synchronization remains enabled when configured.

### Export converters through the C++ API without WWW

The following asynchronous methods export the same converter formats as the
Zigbee download buttons, without HTTP, Wi-Fi or MQTT:

- `startZigbee2MqttConverterExport(writer, finished)` requires
  `ARDPORTAL_ENABLE_ZIGBEE_CONVERTER=1`.
- `startZhaQuirkExport(writer, finished)` requires
  `ARDPORTAL_ENABLE_ZHA_QUIRK=1`.
- `zigbeeConverterExportReady()` checks successful frozen endpoint registration
  and native stack readiness. Connection to a Zigbee network is not required.
- `zigbeeConverterExportBusy()` checks the API job, independently of HTTP downloads.
- `cancelZigbeeConverterExport()` requests cancellation; the result arrives from
  `loop()`.

Both export features require Zigbee and dynamic pages. Their API and shared
writer/completion types disappear if both features are disabled. HTTP download
routes, state and streaming methods additionally require WWW. The layout/model
signature remains identical with and without WWW and even when both exporters
are disabled, so a previously installed
converter still recognizes devices with the same layout.

`writer(const char* data, size_t bytes)` receives at most 256 bytes per loop pass.
The pointer is borrowed and valid only during that call. Return `true` after
accepting **all** bytes. Return `false` without consuming any bytes to retry the
same fragment later. For destinations supporting partial writes, copy a whole
fragment into your own queue and drain that queue separately; do not partially
write a fragment and return false. A blocked consumer or memory shortage lasting
30 seconds without progress fails the export. The library keeps one metadata row
and a bounded output fragment, not the entire file.

`finished(bool success, const String& error)` runs once for an accepted job.
A rejected start returns false and does not call it. Cancellation, layout changes,
restart/OTA admission conflicts and stalled output fail an active job. Discard any
partial file on failure. A successful callback means every byte was accepted by
your writer; it does not confirm your destination has flushed or durably saved it.
Continue calling `portal.loop()`, and keep callbacks nonblocking.

```cpp
// Call after zigbeeConverterExportReady(), for example from loop().
// This sink queues each whole fragment for your application's own transport.
bool started = portal.startZigbee2MqttConverterExport(
  [](const char* data, size_t bytes) -> bool {
    return enqueueWholeFragment(data, bytes); // Copies bytes; false consumes none.
  },
  [](bool success, const String& error) {
    // Finalize your .mjs output on success, or discard partial output on failure.
  }
);
```

Use `startZhaQuirkExport()` for Python `.py` output. The application owns the
file/transport, filenames and final flushing. This API does not mount an additional
filesystem or publish automatically through MQTT. `ARDPORTAL_ENABLE_WIFI=0` and
`ARDPORTAL_ENABLE_WWW=0` do not disable this export API.

On ESP32-H2, ArdPortal automatically disables Wi-Fi/AP, WWW and MQTT.
Both End Device and Router roles are supported. HTTP OTA requires WWW; Zigbee OTA can operate without WWW or Wi-Fi.
