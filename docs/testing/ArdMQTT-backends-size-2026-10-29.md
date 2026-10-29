# ArdMQTT extraction and backend sizes — updated 2026-10-30

Sketch: `examples/DynamicPages`, unchanged between builds. Baseline:
`7068a2bc6234651cffe1efb6e73ea39dc2e8201d`. Arduino CLI 1.5.1,
ESP8266 core 3.1.2, ESP32 core 3.3.12, optional PubSubClient 2.8.

## ESP8266 generic

| Backend | Firmware .bin bytes | IROM bytes | Static RAM bytes |
| --- | ---: | ---: | ---: |
| Before extraction | 593056 | 551652 | 43756 |
| Standalone ArdMQTT (default) | 592928 | 551716 | 43580 |
| PubSubClient | 594960 | 553732 | 41592 |

The native firmware is **128 bytes smaller than before extraction**, with
176 bytes less static RAM. The earlier +704-byte result was superseded after
removing the additional type-erased callback and writer, duplicate validation,
and temporary String conversions while building protocol fields. The portal
uses a compile-time transport binding; there is no runtime backend selector.
Inlining the single-call protocol entry points lets the compiler fold the
adapter into its caller.

PubSubClient adds 2032 firmware bytes relative to native ArdMQTT. Its static
RAM is 1988 bytes lower because its 2048-byte MQTT buffer lives on the heap;
this is not an equivalent reduction in total runtime RAM. Both backends retain
the existing 2048-byte packet limit. No second receive buffer is kept in the
portal adapter.

`xtensa-lx106-elf-nm -C` confirmed 0 `ArdMQTTClient<...>` symbols in the PubSubClient ELF,
and 0 `PubSubClient::` symbols in the native ELF. Compiler dependency tests also
verify that the unselected implementation header is absent, and disabling MQTT
excludes both headers.

## ESP32 generic

| Backend | Program bytes | Firmware .bin bytes | Static RAM bytes |
| --- | ---: | ---: | ---: |
| Before extraction | 1282926 | 1283168 | 57732 |
| ArdMQTT | 1282926 | 1283168 | 57748 |
| PubSubClient | 1285366 | 1285616 | 55756 |

The native program and firmware binary have **exactly the baseline size**.
Static RAM increased by 16 bytes. These measurements use identical sketches,
board settings, core and compiler versions; other profiles may produce different
alignment and inlining results.

These are build/link checks, without broker or hardware testing.

## Reproduction

```sh
arduino-cli compile --fqbn esp8266:esp8266:generic --library . \
  --build-path /tmp/ardmqtt-native examples/DynamicPages
arduino-cli compile --fqbn esp8266:esp8266:generic --library . \
  --library /path/to/PubSubClient \
  --build-property compiler.cpp.extra_flags=-DARDPORTAL_USE_PUBSUBCLIENT=1 \
  --build-path /tmp/ardmqtt-pubsub examples/DynamicPages
```

Use `esp32:esp32:esp32` for ESP32. Baseline was built from an independent
`git archive` checkout with the same sketch, board and default options.
The optional dependency is passed explicitly; it is not added to the default
library dependencies.

## Verification

Both portal backends compiled successfully on ESP8266 and ESP32. The standalone
`examples/ArdMQTT` sketch also compiled on ESP8266 without the portal. The
ASan/UBSan client, backend selection, packet framing, API, state topic, network,
power integration and complete `run-config-memory.sh` suites passed.

## MQTT disabled while logging is enabled

An ESP32 sketch with `ARDPORTAL_ENABLE_LOG_CALLBACK 1`,
`ARDPORTAL_ENABLE_MQTT 0` and `ARDPORTAL_ENABLE_DYNAMIC_PAGES 0`, all defined
**before** `#include <ArdPortal.h>`, compiled to 1024870 program bytes with
51484 bytes static RAM. Its ELF has zero `ArdMqtt::`, `ArdMQTTClient<...>` and
`PubSubClient::` symbols. This probe omits M5 hardware initialization.

The general web console remains enabled by the log callback's default diagnostic
settings. Set `ARDPORTAL_ENABLE_CONSOLE 0` before the include to hide it while
keeping callbacks. Defining feature flags after the include cannot remove
already included declarations, implementations or UI sections. Backend tests
cover both console choices with MQTT disabled and callbacks enabled.
