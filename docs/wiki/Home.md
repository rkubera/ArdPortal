# ArdPortal documentation

[Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home) · [All topics](https://github.com/rkubera/ArdPortal/wiki/Home#start-here) · [Source](https://github.com/rkubera/ArdPortal)

## On this page

- [Start here](#start-here)
- [First connection](#first-connection)
- [Project links](#project-links)

A Wi-Fi and MQTT configuration portal for Wi-Fi equipped ESP8266 and ESP32 boards,
with storage and JSON implementations written in this project. The library
requires only the board’s Arduino core; no ESPAsyncTCP, AsyncTCP,
ESPAsyncWebServer or external MQTT library is required.

This wiki is the developer and user reference for ArdPortal, ArdFS and ArdJSON.
It covers Wi-Fi equipped ESP8266 and ESP32 boards and the current 0.7.7 API.
Documentation reviewed on **10 October 2026**. Features awaiting the next source
release are marked explicitly.

## Start here

| Topic | What you will find |
| --- | --- |
| [Overview](https://github.com/rkubera/ArdPortal/wiki/Overview) | Architecture, main features and hardware responsibilities |
| [Getting started](https://github.com/rkubera/ArdPortal/wiki/Getting-Started) | Installation, first boot, a minimal sketch and examples |
| [All defines](https://github.com/rkubera/ArdPortal/wiki/Defines) | Every supported compile-time flag, defaults and dependencies |
| [Portal API](https://github.com/rkubera/ArdPortal/wiki/Portal-API) | Methods, callbacks, configuration fields, options and lifecycle |
| [FS API](https://github.com/rkubera/ArdPortal/wiki/FS-API) | ArdFS operations, results, streaming writes and recovery |
| [JSON API](https://github.com/rkubera/ArdPortal/wiki/JSON-API) | Types, checked access, parsing, serialization and object patches |
| [All controls](https://github.com/rkubera/ArdPortal/wiki/Controls) | All 30 field types, their values, options and commands |
| [Dynamic pages](https://github.com/rkubera/ArdPortal/wiki/Dynamic-Pages) | JSON schemas, registration, limits, flash sources and HTTP loading |
| [Dependent fields](https://github.com/rkubera/ArdPortal/wiki/Dependent-Fields) | Single conditions, AND/OR, page conditions and cascades |
| [Home Assistant](https://github.com/rkubera/ArdPortal/wiki/Home-Assistant) | Discovery, mapping, availability, state and HA-only entities |
| [MQTT messages](https://github.com/rkubera/ArdPortal/wiki/MQTT-Messages) | Topic construction, payload examples, retain and custom commands |
| [Wi-Fi and device identity](https://github.com/rkubera/ArdPortal/wiki/Wi-Fi-and-Device-Identity) | Station/AP operation and normalized names |
| [Console and OTA](https://github.com/rkubera/ArdPortal/wiki/Console-and-OTA) | Messages, WebSockets, firmware uploads and restart handling |
| [Memory and limits](https://github.com/rkubera/ArdPortal/wiki/Memory-and-Limits) | Actual heap pools, reserves, limits and allocation failure |
| [Languages](https://github.com/rkubera/ArdPortal/wiki/Languages) | Translation catalogs and localized field labels |
| [Troubleshooting](https://github.com/rkubera/ArdPortal/wiki/Troubleshooting) | Registration, memory, networking, HA and persistence failures |
| [Screenshots](https://github.com/rkubera/ArdPortal/wiki/Screenshots) | Eight annotated views of the DHT example portal |

## First connection

First upload the **BasicPortal** example: in Arduino IDE open **File → Examples → ArdPortal → BasicPortal**, select your ESP board and flash/partition layout, then upload it over USB/serial. The sketch is [`examples/BasicPortal/BasicPortal.ino`](https://github.com/rkubera/ArdPortal/blob/main/examples/BasicPortal/BasicPortal.ino). The library alone does not start a portal until a sketch using it has been uploaded.

Connect to `ArdUI-<chip ID>` with the default AP password **`1234567890`** and open
`http://192.168.4.1/`. Call `portal.loop()` frequently to advance startup, storage
and network work. The library requires the ESP Arduino core; only examples with
external sensors require their sensor libraries.

![DHT example Home page](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/Demo1.png)

## Project links

- [Source and examples](https://github.com/rkubera/ArdPortal)
- [Releases](https://github.com/rkubera/ArdPortal/releases)
- [Changelog](https://github.com/rkubera/ArdPortal/blob/main/CHANGELOG.md)
- [Issues](https://github.com/rkubera/ArdPortal/issues)
- [MIT license](https://github.com/rkubera/ArdPortal/blob/main/LICENSE)

---

[Back to documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

- [Zigbee](https://github.com/rkubera/ArdPortal/wiki/Zigbee)
