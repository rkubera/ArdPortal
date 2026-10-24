# Getting started

[Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home) · [All topics](https://github.com/rkubera/ArdPortal/wiki/Home#start-here) · [Source](https://github.com/rkubera/ArdPortal)

## Quick start

ArdPortal is a standalone Arduino library. Its complete distribution contains
`library.properties`, `src`, `examples`, `media`, this README and the MIT license.
The external `ArdUI.ino` demo is not part of the library.

Install ArdPortal through the Arduino Library Manager when the release is
available there, or download a release ZIP and use **Sketch → Include Library →
Add .ZIP Library**. A manual installation places `ArdPortal` in the sketchbook's
`libraries` directory. Avoid duplicate installed copies; Arduino's compilation
output identifies the actual library path.
For your first connection, open **File → Examples → ArdPortal → BasicPortal**
(`examples/BasicPortal/BasicPortal.ino`), then select the board,
physical flash size and partition layout with space for ArdFS and OTA.

```cpp
#include <ArdPortal.h>

ArdPortal portal;

void setup() {
  Serial.begin(115200);
  if (!portal.begin()) Serial.println("Invalid portal options");
}

void loop() {
  portal.loop();
  // Keep doing application work; do not wait for network or storage completion.
}
```

On a new board, ArdFS is mounted and automatically formatted if mounting
fails. With no saved Wi-Fi credentials, the device starts an AP named
`ArdUI-<chip ID>` with password `1234567890`. Connect to it and open
`http://192.168.4.1/`. Operating systems supporting captive portal detection may
open a configuration window automatically.

`begin()` starts initialization and returns immediately after validating options;
storage initialization and configuration loading continue through `loop()`.
Use `portalAndAppConfigReady()` or `onPortalAndAppConfigReady` before accessing saved settings.
Call `loop()` frequently. Methods and callbacks run in the Arduino task: do not
call `loop()` recursively or access the portal concurrently from other tasks.



## Examples

Examples are in `examples`:

| Example | Purpose |
| --- | --- |
| `ESP01-1MB` | ESP-01 portal with MQTT, OTA and HA; without TLS or Console |
| `DHT` | Configurable GPIO and DHT type, numeric °C/% readings, calibration, MQTT/HA and OTA; requires Adafruit DHT sensor library |
| `MinimalPortal` | Basic portal with OTA enabled; MQTT and dynamic forms disabled by sketch defines |
| `DynamicPagesWithDependencies` | Dynamic fields with visibility dependencies |
| `BasicPortal` | Minimal `begin()` / `loop()` portal with integrated storage |
| `DynamicPages` | Two JSON-defined pages with switch, slider, editable text, select and a change callback |
| `HomeAssistantEntities` | HA sensor/switch registration without dynamic panel pages, cooperative HA work settings |
| `ApplicationConfig` | Application JSON values and configuration callbacks |
| `AsyncStorage` | Standalone cooperative journal storage without a portal |
| `ZigbeeClimate` | Zigbee slider and thermostat, simulated temperature and shared AppConfig values; MQTT/HA disabled |
| `ZigbeePortal` | Native and composite Zigbee controls, callbacks and gateway converter export |
| `JsonBasics` | Standalone JSON objects, arrays, serialization and parsing |

The external `ArdUI.ino` demo additionally demonstrates MQTT subscriptions to device
commands/queries, publishing status and reacting to portal changes.

Progress and successful actions appear in green notification boxes. Failed
requests and connection attempts appear in red. Empty notifications are hidden;
the scan progress notification disappears after scanning completes. The Wi-Fi
connection progress notification disappears when the requested connection obtains
an IP address. Submitting unchanged Wi-Fi credentials starts a fresh attempt.

### DHT example

`examples/DHT/DHT.ino` uses Adafruit's DHT library and Unified Sensor dependency,
with a DHT22 on GPIO2, OTA, MQTT, HA and Console enabled. TLS, dependencies and
unused controls are excluded; slider, text and select are enabled. Use a pull-up
from DATA to 3.3 V and ensure GPIO2 is high during ESP8266 boot.

Home shows numeric temperature in Celsius and humidity in percent. Settings
select the GPIO, DHT sensor type, two calibration offsets and a persisted MQTT
rate: on reading change, every 10 seconds, minute, 10 minutes or hour. Corrected
values are rounded to one decimal place; humidity is clamped to 0–100%.
Changing calibration immediately recalculates the last sensor sample and queues
updated measurements for MQTT, bypassing the normal publishing interval without
an extra DHT transaction.

`class: "temperature"` and `class: "humidity"` provide units and HA measurement
metadata. Readings are numbers or `null` when unknown; no HA text-conversion
template is needed. HA handles display unit conversion. Live readings use
RAM-only setters and their own MQTT schedule; settings use ArdFS. Reconnect/get
snapshots and HA discovery remain independent of that schedule.

Samples are scheduled every two seconds, starting two seconds after configuration
readiness, and skipped during OTA. The DHT transaction itself is synchronous.
Three failed readings reinitialize the sensor with a two-second retry delay;
Serial diagnostics distinguish sensor failures from rejected portal values.
On-change mode publishes when the rounded displayed value changes, so changes
smaller than the displayed resolution do not necessarily send another packet.

---

[Back to documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

## First Zigbee connection

Use **File → Examples → ArdPortal → ZigbeeClimate** on ESP32-C5/C6/H2. Select Zigbee ED for the default End Device role, or set `ARDPORTAL_ZIGBEE_ROUTER=1` with ZCZR board mode. Preserve the example partition table and upload through USB for the initial flash layout. Enable joining on the coordinator; a new device attempts pairing automatically and retries after unsuccessful series.

On C5/C6, configure Wi-Fi through the normal portal if needed, then open **Zigbee** for network information and converter/quirk downloads. On H2, or with `ARDPORTAL_ENABLE_WIFI=0`, use the programming API and optional USB diagnostics; no web portal is available. Zigbee does not require the device's MQTT or HA module. Follow the [Zigbee guide](https://github.com/rkubera/ArdPortal/wiki/Zigbee) for gateway installation, OTA and control coverage.
