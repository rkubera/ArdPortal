<!-- Author: Radoslaw Kubera (rkubera on GitHub). License: MIT. -->

# Examples

| Sketch | What it demonstrates |
| --- | --- |
| `ESP01-1MB/ESP01-1MB.ino` | ESP-01 with MQTT, OTA and HA enabled; TLS and Console disabled in the sketch. Use Generic ESP8266 Module with the actual flash size and an ArdFS area. |
| `DHT/DHT.ino` | DHT22 on GPIO2: temperature/humidity, C/F selection, calibration, MQTT/HA and OTA; three control types, no TLS/Console. |
| `MinimalPortal/MinimalPortal.ino` | Portal with OTA enabled and MQTT disabled; HA and Console are disabled automatically. |
| `BasicPortal/BasicPortal.ino` | Minimal portal; configuration, JSON and ArdFS journal are managed by the library. The loop only calls `portal.loop()`. |
| `DynamicPages/DynamicPages.ino` | Two JSON-defined pages registered cooperatively in sequence; completion/error callbacks and startup values applied after both storage and registration are ready. |
| `DynamicPagesWithDependencies/DynamicPagesWithDependencies.ino` | A toggle controls slider and text-field visibility, including retained HA discovery removal/recreation. |
| `HomeAssistantEntities/HomeAssistantEntities.ino` | HA sensor and switch without panel pages; bounded HA work and shared AppConfig callbacks. |
| `ApplicationConfig/ApplicationConfig.ino` | Using getAppConfigValue/setAppConfigValue for application JSON values, queuing updates and handling configuration callbacks. |
| `AsyncStorage/AsyncStorage.ino` | Standalone asynchronous/cooperative journal read/write without starting a portal. |
| `JsonBasics/JsonBasics.ino` | JSON objects/arrays, serialization, parsing and field access; output goes to Serial at 115200 baud. |

Copy the complete `ArdPortal` library folder into your Arduino sketchbook’s
`libraries` directory, then
open the desired `.ino` file from this `examples` directory. Select a Wi-Fi
capable ESP8266 or ESP32 board. Storage uses ArdFS and JSON uses ArdJSON. Only the DHT11 example needs external libraries: Adafruit DHT sensor library and its Adafruit Unified Sensor dependency.

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

`ESP01-1MB` compiles only `slider`, `text`, `select` and `edit`. Its main `.ino`
sets `ARDPORTAL_ENABLE_CONTROLS` to `0` and enables these four individual flags.
`MinimalPortal` instead sets `ARDPORTAL_ENABLE_DYNAMIC_PAGES` to `0`: no dynamic
forms or control types are included, and HA/dependencies are forced off. Generic
application configuration methods and ArdFS persistence remain available. Other examples use the default of all 30 field types.
Each type can be switched separately with `ARDPORTAL_ENABLE_CONTROL_<TYPE>`.
Definitions of excluded types are rejected when registered. Exclusive validation,
HA schemas, web code/styles and captions are omitted from the firmware.

Use `switch` for graphical on/off fields, `slider` for numeric settings and `text`
for read-only numeric or text values. HA still receives switch, number and sensor
entities respectively. Shared primitives remain inside an enabled composite
control such as a light or fan.

## ESP-01 MQTT build

`ESP01-1MB` enables MQTT, OTA and HA; disables MQTT TLS, Console and field
dependencies through defines before `ArdPortal.h`. Only slider, text, select and
edit fields are included. ESP8266 core 3.1.2, Generic ESP8266 Module, flash layout
`1MB (FS:64KB OTA:~470KB)` produces a 374,368-byte firmware binary and uses
36,740 bytes of static RAM. It fits a 1 MB ESP-01. The remaining sector-aligned
OTA staging space is 581,632 bytes, enough for this image. Initial upload is by
serial; subsequent uploads can use the Update page. Hardware OTA upload has not
been tested. ArdFS uses the filesystem reservation. Static RAM excludes runtime
heap allocations.

The preceding build with all control types was 504,608 bytes / 37,440 static RAM
bytes. Selecting four controls saves 21,376 firmware bytes and 648 static RAM
bytes. The earlier OTA-disabled `MinimalPortal` on the same board settings built to 345,968 bytes with
31,052 static RAM bytes. Selecting four controls originally saved 18,640 firmware
bytes and 424 static RAM bytes there. The corrected four-control gzip profile
saves an additional 2,112 bytes in MinimalPortal and 2,240 bytes in ESP01-1MB,
with unchanged static RAM.

An earlier OTA-enabled check of the same MQTT sketch builds to 570,784 bytes
and uses 38,428 bytes of static RAM. It fits as a serial-uploaded application,
but the 1 MB / 64 KB storage layout leaves only 389,120 bytes for
the incoming update, too little for another raw firmware image of this size.
The portal currently accepts raw `.bin` images only.

ESP01-1MB sets `ARDPORTAL_ENABLE_DEPENDENCIES` to `0` before including
`ArdPortal.h`. MinimalPortal disables dependencies automatically by excluding
all dynamic forms. In ESP01-1MB, dynamic fields remain available, but `visibleWhen` definitions are
rejected. Disabling this feature saves 1,040 firmware bytes in MinimalPortal and
1,168 bytes in ESP01-1MB, plus 36 static RAM bytes in each, compared with the
same four-control builds with dependencies enabled. Other examples retain the
default enabled setting, including DynamicPagesWithDependencies.

Excluding all dynamic forms saves an additional 13,568 firmware bytes and
816 static RAM bytes in MinimalPortal versus its preceding dependency-disabled,
four-control build (363,264 bytes / 31,932 static RAM bytes). That build
was 349,696 bytes / 31,116 static RAM bytes, using ESP8266 core 3.1.2 and the
same Generic ESP8266 1 MB / FS 64 KB settings.

Recompiling the same current code with dynamic forms enabled and the four basic
control flags produces 363,328 bytes. Thus the new define alone saves 13,632
firmware bytes; the comparison to the preceding revision above includes a
64-byte change in the shared configuration adapter and web helper ordering.

The static HTML shell is now gzip-compressed when dynamic forms are disabled.
MinimalPortal builds to 347,520 bytes / 31,116 static RAM bytes, saving another
2,176 firmware bytes versus 349,696 bytes with the uncompressed HTML shell.
The ESP streams compressed bytes directly from flash; only the browser inflates
them. No extra decompression buffer is allocated on the device.

Numeric native-message IDs further reduce ESP01-1MB to 477,904 firmware bytes
and 36,680 static RAM bytes: 1,968 firmware / 76 RAM bytes saved versus 479,872
/ 36,756. Changing only `ARDPORTAL_ENABLE_HA` to `1` builds to 481,232 firmware
bytes and 37,012 static RAM bytes, saving 1,952 firmware / 84 RAM bytes versus
the preceding HA-enabled build. Both profiles keep OTA, Console and dependencies
disabled and enable slider, text, select and edit.

With numeric native-message IDs, the earlier OTA-disabled MinimalPortal built to 345,968 firmware
bytes and 31,052 static RAM bytes; the gzip-only measurement above precedes
this message-catalog optimization.

The current MinimalPortal enables OTA while keeping MQTT and dynamic forms off.
With ESP8266 core 3.1.2 and Generic 1 MB / FS 64 KB settings, its firmware
is 346,928 bytes and static RAM is 31,144 bytes. Shared HTTP response construction
saved 1,536 bytes, followed by 2,416 bytes from compact language messages.
The preceding OTA-enabled build was 350,880 bytes.

## DHT

Install Adafruit's **DHT sensor library** (the Library Manager also installs
**Adafruit Unified Sensor**). Connect VCC to 3.3 V, GND to GND, and DATA to
**GPIO2**, with a 4.7–10 kΩ pull-up to 3.3 V if the module lacks one. On
ESP8266 this is GPIO2, not the NodeMCU D2 label (GPIO4); GPIO2 must be high
during boot. Use a board/flash layout with space for ArdFS and OTA.

The Home card on Start has two read-only measurements: temperature and humidity.
Info appears below it in a separate card. The Settings page has a Celsius/
Fahrenheit selector, a temperature offset (-10…10 °C, step 0.1), and a humidity
offset (-20…20 percentage points, step 1), and MQTT update rate. Temperature correction is applied
in Celsius before display conversion. Corrected humidity is clamped to 0…100%.
Offsets, units and MQTT rate survive restart; measurements stay in RAM without flash writes.
Changing units/calibration immediately updates the cached readings. Failed reads
show Unavailable. Readings are sampled every two seconds and skipped during OTA.

MQTT and HA discovery include all six fields. The portal/MQTT temperature
contains the chosen unit; the HA template converts it to a numeric Celsius
sensor, which Home Assistant can display in its own chosen unit. No extra unit
or status field is needed. The three enabled control types are slider, text
and select; MQTT TLS, dependencies and other types are excluded; Console is enabled.
No special asset compression profile is added for this example.

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

DHT22 temperature and humidity are rounded to one decimal place after calibration
and unit conversion, before portal updates and MQTT publishing.

The DHT example displays temperature and humidity in a separate Home card above
Info. Its Settings page contains the four settings. Both measurements use fixed
one-decimal MQTT text payloads; HA interprets humidity as a numeric sensor.

The DHT example currently selects DHT22 on GPIO2. It waits two seconds after
configuration readiness before sampling, and reinitializes the sensor after
three failed transactions with a two-second retry delay. Serial diagnostics
distinguish failed sensor reads from rejected portal values.
