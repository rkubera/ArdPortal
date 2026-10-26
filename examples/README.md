<!-- Author: Radoslaw Kubera (rkubera on GitHub). License: MIT. -->

# Examples

| Sketch | What it demonstrates |
| --- | --- |
| `DHT/DHT.ino` | Configurable GPIO and DHT11/12/21/22 type (default GPIO2 / DHT22), numeric °C/% readings, calibration, MQTT/HA, Console and OTA; no TLS. |
| `BasicPortal/BasicPortal.ino` | Minimal portal; configuration, JSON and ArdFS journal are managed by the library. The loop only calls `portal.loop()`. |
| `PortalStandby/PortalStandby.ino` | Suspend the Wi-Fi portal after Zigbee connects; a GPIO4 button reopens it for two minutes without stopping Zigbee. |
| `MatterPortal/MatterPortal.ino` | Matter plug, temperature and brightness endpoints; Wi-Fi/Thread selection, pairing UI and shared callbacks. |
| `ZigbeePortal/ZigbeePortal.ino` | Broad native/composite Zigbee control catalogue with portal callbacks and gateway converter/quirk export. |
| `ZigbeeClimate/ZigbeeClimate.ino` | Simulated temperature slider and thermostat over native Zigbee without MQTT. |
| `UsbDiagnostics/UsbDiagnostics.ino` | Messages over a sketch-owned USB/UART log callback, with WWW and its console histories disabled. |
| `PowerManagement/PowerManagement.ino` | Independent power policy/sleep type, GPIO/ISR events and software timers. |
| `ZigbeePowerManagement/ZigbeePowerManagement.ino` | Native Zigbee end-device radio sleep, optional CPU light sleep and periodic application events; Wi-Fi disabled. |
| `PowerManagementPeriodic/PowerManagementPeriodic.ino` | Scheduled application cycles, Periodic wake events and automatic sleep between cycles. |
| `PowerManagementDeepSleep/PowerManagementDeepSleep.ino` | Timed deep sleep and wake-cause diagnostics with Wi-Fi disabled; optional Zigbee end device via `ARDPORTAL_ENABLE_ZIGBEE=1`, ED board mode and the supplied optional partition layout. |
| `DynamicPages/DynamicPages.ino` | Two JSON-defined pages registered cooperatively in sequence; completion/error callbacks and startup values applied after both storage and registration are ready. |
| `DynamicPagesWithDependencies/DynamicPagesWithDependencies.ino` | A toggle controls slider and text-field visibility, including retained HA discovery removal/recreation. |
| `HomeAssistantEntities/HomeAssistantEntities.ino` | HA sensor and switch without panel pages; bounded HA work and shared AppConfig callbacks. |
| `ApplicationConfig/ApplicationConfig.ino` | Using getAppConfigValue/setAppConfigValue for application JSON values, queuing updates and handling configuration callbacks. |
| `AsyncStorage/AsyncStorage.ino` | Standalone asynchronous/cooperative journal read/write without starting a portal. |
| `JsonBasics/JsonBasics.ino` | JSON objects/arrays, serialization, parsing and field access; output goes to Serial at 115200 baud. |

Copy the complete `ArdPortal` library folder into your Arduino sketchbook’s
`libraries` directory, then
open the desired `.ino` file from this `examples` directory. Select a Wi-Fi
capable ESP8266 or ESP32 board. Storage uses ArdFS and JSON uses ArdJSON. Only the DHT example needs external libraries: Adafruit DHT sensor library and its Adafruit Unified Sensor dependency.

For ESP-12F, choose its actual flash size and a layout with filesystem/OTA space.
A typical 4 MB module can use 4 MB flash with a 1 MB filesystem. A failed
ArdFS mount triggers automatic formatting, which deletes filesystem files.

The default AP is `ArdUI-<chip ID>` with password `1234567890`. Connect to it;
captive portal detection may open the setup window automatically. Otherwise open
`http://192.168.4.1/`. After connecting Wi-Fi, use the LAN URL displayed by the
portal. The AP closes after all its clients disconnect.

In `DynamicPages`, open **Controls** (`/p/controls`) or **Settings** (`/p/settings`) in the second menu row.
Changes apply automatically and the library persists these two fields. Their IDs
(`enabled`, `level`) are keys for `getAppConfigValue` and `setAppConfigValue`.
`name` is the fallback label; `names` provides optional translations. Configure
MQTT in the portal to enable automatic Home Assistant discovery and state updates.

The portal language selector is visible when more than one language is compiled.
Translations are in `../languages/*.json`. To change/add a language, edit/copy a
JSON file, run `python3 ../tools/generate_languages.py` from this directory, then
recompile. Translation assets are built into firmware, not uploaded to ArdFS.

`begin()` starts storage initialization; call `loop()` repeatedly until
`portalAndAppConfigReady()` before reading saved values. Setters accept updates in
RAM; `onPortalAndAppConfigSaved` confirms durable completion. Application changes are merged
and identical writes skipped. See `ApplicationConfig` and the library README
for the full API and journal behavior.

MQTT, Home Assistant and OTA are serviced automatically by `portal.loop()` through
the owned `ArdMqtt`, `ArdHomeAssistant` and `ArdOta` components. The existing
portal methods remain available; no separate component initialization is needed.

Do not run a standalone `ArdFS` instance or independently mount/format
ArdFS while the portal uses it. The module distributes operations between
loop iterations, but mount/format, compaction and sync and physical flash writes remain
synchronous. It cannot guarantee zero processor stalls or hard real-time latency.

`BasicPortal` does not add application MQTT subscriptions/commands. The project's
main `ArdUI.ino` demonstrates cmnd/get subscriptions, stat/status publishing,
ready/change/save callbacks and connection state reporting.

Keep this guide and all example code comments in English when updating examples.

The project root `ArdUI.ino` also demonstrates JSON-defined dynamic pages, all supported MQTT entity/control types, optional climate modes/fan modes/icons, Home Assistant MQTT discovery
and `onAppConfigValueChanged`/`onAppConfigCommand` callbacks. Dynamic controls apply
changes automatically and refresh through WebSocket notifications. See the library README for the definition
schema. Minimal examples remain focused on their individual features.

The DynamicPages example keeps its static definition in `PROGMEM` and registers
it with `FPSTR()`. The portal retains only an index in RAM and parses the page
on demand. Definitions passed as a String instead retain an owned JSON source.

## Small control profiles

To compile only selected controls, set `ARDPORTAL_ENABLE_CONTROLS` to `0`
and enable the required individual `ARDPORTAL_ENABLE_CONTROL_*` flags.
Setting `ARDPORTAL_ENABLE_DYNAMIC_PAGES` to `0` excludes all dynamic fields: no dynamic
forms or control types are included, and HA/dependencies are forced off. Generic
application configuration methods and ArdFS persistence remain available. Other examples use the default of all 30 field types.
Each type can be switched separately with `ARDPORTAL_ENABLE_CONTROL_<TYPE>`.
Definitions of excluded types are rejected when registered. Exclusive validation,
HA schemas, web code/styles and captions are omitted from the firmware.

Use `switch` for graphical on/off fields, `slider` for numeric settings and `text`
for read-only numeric or text values. HA still receives switch, number and sensor
entities respectively. Shared primitives remain inside an enabled composite
control such as a light or fan.

## DHT

Install Adafruit's **DHT sensor library** (the Library Manager also installs
**Adafruit Unified Sensor**). Connect VCC to 3.3 V, GND to GND, and DATA to
**GPIO2**, with a 4.7–10 kΩ pull-up to 3.3 V if the module lacks one. On
ESP8266 this is GPIO2, not the NodeMCU D2 label (GPIO4); GPIO2 must be high
during boot. Use a board/flash layout with space for ArdFS and OTA.

The Home card on Start has two read-only numeric measurements: temperature in
Celsius and humidity in percent. Info appears below it in a separate card.
Settings select the GPIO and DHT sensor type, temperature offset (-10…10 °C,
step 0.1), humidity offset (-20…20 percentage points, step 1), and MQTT update
rate. Corrected humidity is clamped to 0…100%. Settings survive restart;
measurements stay in RAM without flash writes. Changing calibration immediately
updates the cached readings. Failed reads publish `null` (unknown). Readings are
sampled every two seconds and skipped during OTA.

MQTT and HA discovery include the measurements and settings. Temperature and
humidity use `class: "temperature"` and `class: "humidity"`; their unit and HA
measurement metadata are inferred. No `ha` block or text-conversion template is
needed. HA can display Celsius readings in the user's chosen unit. The three
enabled control types are slider, text and select; MQTT TLS, dependencies and
other types are excluded; Console is enabled. No special asset compression
profile is added for this example.

The sketch does not wait between samples. The DHT22 transaction itself is
synchronous; sampling, MQTT publication and storage are scheduled through loop.
Physical sensor reads and OTA still require hardware verification. Build size
depends on selected flags, core/library versions and layout; measure the generated
firmware for your board rather than relying on another profile's size.

The **MQTT updates** selector offers On reading change (default), Every 10 seconds,
Every minute, Every 10 minutes and Every hour. It schedules temperature/humidity
only, with retained state packets sent cooperatively. Fixed intervals repeat the
latest readings even if unchanged. Portal readings still update every two seconds;
settings acknowledge immediately in MQTT. Connecting/reconnecting and explicit
`get` requests also return the current state. HA discovery is independent.

DHT temperature and humidity are rounded to one decimal place after calibration,
before portal updates and MQTT publishing.

The DHT example displays temperature and humidity in a separate Home card above
Info. Both measurements use numeric values, rounded to one decimal place;
HA receives numeric measurements or an unknown reading.

The DHT example currently selects DHT22 on GPIO2. It waits two seconds after
configuration readiness before sampling, and reinitializes the sensor after
three failed transactions with a two-second retry delay. Serial diagnostics
distinguish failed sensor reads from rejected portal values.

In `DHT`, choose **DHT data pin (GPIO)** and **DHT sensor type** under **Settings**.
Changes are saved and applied without restarting; readings resume after two seconds.
Use chip GPIO numbers rather than board labels (for example, ESP8266 D2 is GPIO4).
The GPIO list excludes ESP8266 flash pins and ESP32 pins without output support;
check your board pinout to avoid pins used by flash, PSRAM, USB or other peripherals.

On ESP8266, the serial bootloader also outputs UART TX on GPIO2 during flashing.
If a DHT connected there stops responding after a serial upload and restarting
the ESP does not help, power-cycle both the ESP and sensor. `dht.begin()` only
reinitializes the host driver and data pin; it does not reset the sensor's power.
For boards with spare pins, consider a non-boot GPIO such as GPIO4 or GPIO5.
See [Espressif boot mode documentation](https://docs.espressif.com/projects/esptool/en/latest/esp8266/advanced-topics/boot-mode-selection.html).

DHT diagnostics are available remotely in **Console > Messages**, including the
selected GPIO/type, read failures, driver reinitialization and the first valid
reading after startup or recovery. The same messages are printed to Serial at
115200 baud. Console history is bounded and stored in RAM, so it clears on restart.

- `MatterPortal`: ESP32-C6, optional Matter over Wi-Fi or Thread; use the included single-app partition table and read [Matter](../docs/wiki/Matter.md).

- `PortalStandby`: Zigbee connection followed by Wi-Fi/WWW suspension; a debounced GPIO button restores the portal for two minutes.
