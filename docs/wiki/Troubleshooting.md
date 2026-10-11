# Troubleshooting

[Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home) · [All topics](https://github.com/rkubera/ArdPortal/wiki/Home#start-here) · [Source](https://github.com/rkubera/ArdPortal)

## On this page

- [Registration returns false or finishes with failure](#registration-returns-false-or-finishes-with-failure)
- [RAM/JSON limit or HTTP 503](#ramjson-limit-or-http-503)
- [Console reconnects or Messages disappear](#console-reconnects-or-messages-disappear)
- [MQTT repeatedly reports transport closed](#mqtt-repeatedly-reports-transport-closed)
- [HA control changes but hardware does not](#ha-control-changes-but-hardware-does-not)
- [HA shows two lightning buttons](#ha-shows-two-lightning-buttons)
- [Field D should depend on B and C](#field-d-should-depend-on-b-and-c)
- [A value reverts after restart](#a-value-reverts-after-restart)
- [OTA file cannot be selected or update fails](#ota-file-cannot-be-selected-or-update-fails)
- [Documentation and maintenance](#documentation-and-maintenance)

### Troubleshooting runtime failures

A `wdt reset` means servicing was interrupted long enough to trigger the watchdog;
it is not evidence that one particular callback or storage operation failed.
Keep application work bounded, do not use waiting loops for `publish()`/saves,
and call `portal.loop()` frequently. Built-in graph traversal and publication
use yields; flash, core network operations and application callbacks still have
synchronous costs. Check reset logs, free heap and a decoded exception trace.

Application field errors include a field ID and stage: `#0` invalid page/field/
index request, `#1` normalization, `#2` control index, `#3` command value, `#4`
composite state, `#5` persistence scheduling, `#6` hidden field. Check the complete
JSON value against the registered descriptor, especially required properties and
slider steps. A busy transaction may be retried; an invalid definition/value
needs correction. A reboot loses uncommitted RAM changes; `onPortalAndAppConfigSaved` is the
durable-save notification.

## Registration returns false or finishes with failure

Immediate `false` means the job was not accepted: no callback follows. Later
`onAppConfigPageRegistrationFinished(false)` means accepted work failed. Inspect
`stage`, `fieldId`, `reason`. Queue one page at a time. Definitions must remain
alive; use static PROGMEM for fixed schemas. A `bytes=0` definition often means a
String construction already failed before registration, not that the source
budget is too low. See [Dynamic pages](https://github.com/rkubera/ArdPortal/wiki/Dynamic-Pages).

## RAM/JSON limit or HTTP 503

Inspect heap8 free bytes and largest block, plus DMA on ESP32. The entire source
need not fit RAM, but its header and largest individual field/value do. Increasing
the total definition budget does not fix a fragmented heap. Preserve network
reserves; use flash definitions and avoid full-tree application copies.

## Console reconnects or Messages disappear

Only one console client and one events client are supported. Additional portal
browsers can replace a session. Messages needs its compile-time define. It has a
separate bounded history from MQTT; both use `/api/console`. No automatic Serial
capture exists. Use `portal.log` or `portal.logMessage`.

## MQTT repeatedly reports transport closed

Check duplicate device/client IDs first. Two devices using one ID can disconnect
each other through the broker. Check Wi-Fi stability, keepalive, slow loops and
broker logs when available. TLS and plaintext are configured explicitly; a
TLS-disabled build does not silently reinterpret saved TLS credentials.

## HA control changes but hardware does not

The library routes settings/actions; the sketch must implement hardware work.
Use `onAppConfigValueChanged` for editable state and `onAppConfigCommand` for
explicit actions. Report hardware results through the state API. Do not use a
stale telemetry value as immediate command confirmation. A retained command is
ignored deliberately. Invalid or hidden-field commands are rejected.

## HA shows two lightning buttons

Check explicit `ha.optimistic:true` and dashboard settings. The library's switch
icon is separate from HA's assumed-state behavior. A declared state topic allows
confirmed-state operation with HA defaults. A project-generated definition may
still explicitly override those defaults.

## Field D should depend on B and C

Define D once and use `visibleWhen.or` or `visibleWhen.and`. Duplicate IDs are
invalid. An invisible controller does not satisfy a leaf even if its stored value
matches. A page depending on its own field creates a dependency cycle.

## A value reverts after restart

`persist:false` and state setters are RAM-only. Accepted setters/visible updates
are not flash completion. Watch `onPortalAndAppConfigSaved`; a pending write lost
with power interruption cannot be restored. Startup readiness and registration
completion are independent. Schema changes can invalidate saved values and make
them fall back to defaults.

## OTA file cannot be selected or update fails

Use `.bin`, or `.bin.gz` on ESP8266 with a supporting core/bootloader. Bzip2 is
not supported; ESP32 rejects gzip. The raw image must match the board/layout and
application area; the compressed upload must fit staging space. URL updates are
browser downloads and need CORS/reachability. See [Console and OTA](https://github.com/rkubera/ArdPortal/wiki/Console-and-OTA).

## Reporting a problem

Verify the actual installed-library path shown by Arduino. Wiki screenshots illustrate the
DHT example and can differ from the current feature/profile selection. Report
reproducible library issues with version/core/board, heap metrics, feature flags
and the relevant JSON definition, without credentials.

---

[Back to documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)
