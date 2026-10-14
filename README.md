<!-- Author: Radoslaw Kubera (rkubera on GitHub). License: MIT. -->

# ArdPortal

ArdPortal is an Arduino library for ESP8266 and ESP32 that provides a browser-based device configuration portal. It uses only the ESP Arduino core; no external web server or MQTT library is required.

- Wi-Fi configuration, fallback access point and captive portal.
- MQTT with optional verified TLS and automatic Home Assistant discovery.
- JSON-defined pages with 30 control types, visibility conditions and live updates.
- Persistent settings through **ArdFS**, the included flash filesystem and journaled storage API.
- JSON parsing and serialization through the standalone **ArdJSON** module.
- OTA firmware upload, diagnostics console, device information and restart controls.
- Optional ESP32-C5/C6 Zigbee End Device/Router, native control mapping, OTA and dedicated diagnostics.
- Compile-time defines to include only the required features.

**[Full documentation and API reference → GitHub Wiki](https://github.com/rkubera/ArdPortal/wiki)**

## Portal preview

Home page from the DHT example, with live temperature and humidity readings:

![ArdPortal Home with temperature, humidity and device information](media/Demo1.png)

[See more portal screenshots](https://github.com/rkubera/ArdPortal/wiki/Screenshots) · [Explore all 30 controls](https://github.com/rkubera/ArdPortal/wiki/Controls)

## Install and connect

Install **ArdPortal** through Arduino Library Manager, or download a [release ZIP](https://github.com/rkubera/ArdPortal/releases) and use **Sketch → Include Library → Add .ZIP Library**. Install the ESP8266 or ESP32 Arduino board package and select a board/flash layout with room for ArdFS and, if needed, OTA.

For the first connection, upload **File → Examples → ArdPortal → BasicPortal** ([sketch](examples/BasicPortal/BasicPortal.ino)). On a device without saved Wi-Fi credentials, connect to **`ArdUI-<chip ID>`**, using password **`1234567890`**, then open **http://192.168.4.1/** to configure it.

## Minimal sketch

```cpp
#include <ArdPortal.h>

ArdPortal portal;

void setup() {
  portal.begin();
}

void loop() {
  portal.loop();
  // Run nonblocking application work here.
}
```

Call `loop()` frequently: initialization, networking and storage advance cooperatively. Configuration loading completes asynchronously; see the [Portal API](https://github.com/rkubera/ArdPortal/wiki/Portal-API) before reading saved settings. Sensor examples may require their sensor libraries.

## Documentation

| Topic | Wiki |
| --- | --- |
| Installation, first connection and examples | [Getting started](https://github.com/rkubera/ArdPortal/wiki/Getting-Started) |
| Modules and control feature flags | [Defines](https://github.com/rkubera/ArdPortal/wiki/Defines) |
| Configuration methods and callbacks | [Portal API](https://github.com/rkubera/ArdPortal/wiki/Portal-API) |
| Controls, screenshots, minimal/full JSON | [Control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls) |
| Page definitions and conditional visibility | [Dynamic pages](https://github.com/rkubera/ArdPortal/wiki/Dynamic-Pages) · [Dependencies](https://github.com/rkubera/ArdPortal/wiki/Dependent-Fields) |
| Zigbee pairing, rejoin and diagnostics | [Zigbee](https://github.com/rkubera/ArdPortal/wiki/Zigbee) |
| Home Assistant and MQTT commands/state | [HA integration](https://github.com/rkubera/ArdPortal/wiki/Home-Assistant) · [MQTT messages](https://github.com/rkubera/ArdPortal/wiki/MQTT-Messages) |
| Standalone flash storage and JSON | [ArdFS API](https://github.com/rkubera/ArdPortal/wiki/FS-API) · [ArdJSON API](https://github.com/rkubera/ArdPortal/wiki/JSON-API) |
| Diagnostics, updates, memory and languages | [Console/OTA](https://github.com/rkubera/ArdPortal/wiki/Console-and-OTA) · [Memory limits](https://github.com/rkubera/ArdPortal/wiki/Memory-and-Limits) · [Languages](https://github.com/rkubera/ArdPortal/wiki/Languages) |

See [portal screenshots](https://github.com/rkubera/ArdPortal/wiki/Screenshots), [troubleshooting](https://github.com/rkubera/ArdPortal/wiki/Troubleshooting), [examples](examples), [changelog](CHANGELOG.md) and [releases](https://github.com/rkubera/ArdPortal/releases).

## License

[MIT](LICENSE). Copyright Radoslaw Kubera (rkubera on GitHub).

Zigbee support includes native controls and composite functions exposed as separate standard endpoints. See the [control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls) for HA/Zigbee coverage and [Zigbee](https://github.com/rkubera/ArdPortal/wiki/Zigbee) for gateway and page-order limitations.
