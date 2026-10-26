# Overview and main features

[Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home) · [All topics](https://github.com/rkubera/ArdPortal/wiki/Home#start-here) · [Source](https://github.com/rkubera/ArdPortal)

## On this page

- [Main features](#main-features)
- [Components](#components)
- [Responsibilities](#responsibilities)

ArdPortal combines optional Wi-Fi/AP/WWW, MQTT/Home Assistant and native Zigbee networking with a shared JSON control model, persistent settings and diagnostics. Wi-Fi features run on supported ESP8266/ESP32 boards; native Zigbee requires ESP32-C5/C6/H2. Disable unused transports at compile time, including Wi-Fi and WWW for a Zigbee-only device.

It uses the ESP Arduino core and its bundled Zigbee SDK, with no external web server, MQTT, JSON or filesystem library required by ArdPortal itself.

## Main features

- **Matter:** Wi-Fi or Thread transport, BLE commissioning, development/factory credential profiles, native shared-control mappings, status/QR/reset UI and independent traffic diagnostics. Thread can retain the Wi-Fi portal; see [Matter](Matter).
- **Portal standby:** suspend/resume Wi-Fi, WWW, WebSockets, DNS and MQTT while Zigbee or Matter over Thread keeps running. This is separate from CPU sleep and unavailable for Matter over Wi-Fi; see [Portal API](Portal-API#suspending-portal-networking).

- **Wi-Fi setup:** scan nearby networks, connect to a saved network and start a fallback AP when connection attempts time out. The portal remains accessible after connecting.
- **Captive portal:** configure the device from a browser, with captive DNS for supported operating systems.
- **MQTT:** configure the broker, optionally use verified TLS, publish application data and handle commands.
- **Home Assistant:** automatically publish discovery definitions and synchronize supported controls through MQTT.
- **Zigbee:** End Device/Router roles, automatic join/rejoin, Zigbee-only reset, standard and composite control endpoints, layout-specific Zigbee2MQTT/ZHA exports, time synchronization, OTA and a dedicated Console channel. See [setup and coverage](https://github.com/rkubera/ArdPortal/wiki/Zigbee).
- **Dynamic pages:** define forms in JSON, including a Home block, live controls and cascading visibility conditions for fields and pages.
- **Persistent configuration:** store portal and application settings as JSON using ArdFS, with a journal and delayed, coalesced writes to reduce flash wear.
- **Live updates:** synchronize portal controls through WebSockets and inspect or send MQTT messages in the console.
- **Firmware updates:** HTTP file/URL uploads, or standard Zigbee OTA through a compatible coordinator. Image validation and deferred restart include the application restart callback and storage flush.
- **Device management:** configure the device name and description, set manufacturer metadata from code, view system information, restart or restore factory settings.
- **Time and languages:** synchronize time through NTP and support developer-provided translations with automatic browser language selection.
- **Configurable builds:** disable optional modules and control types with compile-time flags to reduce firmware size.
- **Cooperative operation:** service network and storage work from `loop()`; remaining synchronous core calls are documented.
- **Self-contained library:** includes standalone ArdFS and ArdJSON modules and uses only the ESP Arduino core.

## Storage and JSON: ArdFS and ArdJSON

**[ArdFS](https://github.com/rkubera/ArdPortal/wiki/FS-API)** is the library's own flash filesystem and journaled JSON storage API. It keeps configuration across restarts, verifies saved records and advances queued reads/writes from `loop()`. It works without HTTP or MQTT.

**[ArdJSON](https://github.com/rkubera/ArdPortal/wiki/JSON-API)** parses JSON text into typed C++ values in RAM and serializes those values back to text. It interprets configuration, page definitions and messages; it does not save data by itself. Both modules can be used independently from ArdPortal.

A typical flow is: **application values → ArdJSON serialization → ArdFS write → flash**. Loading reverses the flow: **flash → ArdFS read → ArdJSON parsing → application values**. The integrated Portal API coordinates this for PortalConfig and AppConfig.

## Components

ArdPortal owns separate components, each with its own implementation
and state. Its existing public methods delegate to them, so sketches still call
`portal.begin()` and `portal.loop()`.

Access the owned instances with `portal.mqtt()`, `portal.homeAssistant()` and
`portal.ota()`. For example, `portal.mqtt().connected()` reports the broker
connection, and `portal.ota().active()`, `received()` and `expected()` report
firmware upload progress. MQTT offers `publish()`, `subscribe()`, `mqttTopic()`,
`validMqttTopic()`, `state()` and `onMessage()`; they have the same semantics
as the corresponding portal methods. A component callback replaces the same
callback registered through the portal; there is only one message callback.

These are integrated components with a reference to the owning portal, not
standalone clients. The portal supplies configuration, storage, HTTP responses
and application callbacks. Do not construct additional components for the same
portal or call a second service loop. Portal/component objects cannot be copied
or moved. Each component has one owner.

## Responsibilities

ArdPortal validates inputs, presents controls, maintains network connectivity,
stores settings and exchanges MQTT messages and Zigbee commands/reports. The application reads sensors,
operates actuators and reports hardware results. Accepting a command, confirming
a flash write and confirming physical hardware are separate events.

`PortalConfig` contains device, Wi-Fi, AP and broker settings. `AppConfig` contains
application values. Field/page definitions are schemas, not stored value copies.
RAM-only telemetry is separate from persistent application configuration.

Application callbacks and cooperative portal work run through `loop()`. The native Zigbee stack runs in a separate SDK task and queues application work for that loop. The
library is cooperative: it splits registration, HTTP, Discovery and storage into
steps, but a core DNS/TCP/TLS/flash operation or application callback can still
block. Keep application callbacks and UART work bounded.

---

[Back to documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)
