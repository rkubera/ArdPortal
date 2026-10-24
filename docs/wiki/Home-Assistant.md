# Home Assistant integration

[Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home) · [All topics](https://github.com/rkubera/ArdPortal/wiki/Home#start-here) · [Source](https://github.com/rkubera/ArdPortal)

## On this page

- [Setup](#setup)
- [State, availability and actions](#state-availability-and-actions)
- [Switch and light display](#switch-and-light-display)
- [Discovery inspection](#discovery-inspection)

## Setup

Enable MQTT and HA in the build (both default to enabled), configure the broker
in the portal, and configure HA's MQTT integration for the same broker. Register
pages or HA-only entities on each boot and keep calling `portal.loop()`.

The library publishes Discovery itself. Application code supplies physical
hardware behavior and state updates; no handwritten HA entity YAML is required
for supported presets. Dynamic pages are not HA dashboard blocks: their entities
share one device, and a custom dashboard layout remains a HA concern.

All controls accept optional `icon: "mdi:..."`. Basic switches default to
`mdi:toggle-switch`; other omitted icons use HA defaults. For select fields, HA displays option labels chosen from `name`/`names`;
discovery templates translate those labels to/from stable `value` strings on
MQTT topics. The portal uses the selected language. Descriptor-based selects
use their declared string options directly.

With at least one registered field, the library automatically subscribes to
`cmnd/<device>/+`, `get/<device>/+` and `homeassistant/status`. Field states are
retained at `stat/<device>/<fieldId>`; ordinary commands use
`cmnd/<device>/<fieldId>`. Climate commands use `<fieldId>_temperature`,
`<fieldId>_mode` and `<fieldId>_fan`; climate state is a JSON object. A message to
`get/<device>/<fieldId>` requests a fresh state. Retained commands are ignored.
Commands received while flash is busy are coalesced by field and retried.
After every successfully committed command, the corresponding retained `stat`
is queued, including commands that repeat the current value. This lets HA and
Node-RED observe completion. Climate replies contain the complete JSON state at
`stat/<device>/<fieldId>`. Multiple commands to the same field during a write may
coalesce into one final-state acknowledgement. Invalid commands and failed writes
are not acknowledged as successful.
Invalid values are rejected. The existing `onMqttMessage` callback still receives
device messages. The console can continue publishing arbitrary topics.

Discovery is retained at `homeassistant/<component>/ardui_<chipId>/<fieldId>/config`.
Entities have stable chip/field identifiers and share one HA device. Current
application values are also republished every ten minutes
(`Options::appStateIntervalMs`, default 600000; zero disables periodic state
refresh), independently of the discovery interval. Definitions
and current states are sent after connection, after HA's `homeassistant/status`
`online` birth message. Periodic discovery is disabled by default
(`Options::discoveryIntervalMs = 0`); a nonzero interval explicitly enables it.
Discovery is retained, so HA does not require periodic configuration messages
to keep entities. Dependency visibility changes can still remove/recreate an
entity. After publishing its Discovery, the current state is queued again so HA
can restore the control promptly. MQTT availability
uses a retained `online` state and retained `offline` Last Will. State updates
have priority over periodic discovery and are queued as soon as a value commits,
then sent in the next available MQTT slot. Only one HA publication is queued per
`loop()` pass. After its temporary JSON objects are destroyed, `yield()` gives
the core scheduler time before another publication; no delay or drain loop is used. QoS 0 provides no delivery acknowledgement;
reconnect sends current states again. The library does not wait for HA to appear.

Persistent application setters retain their debounce/wear-saving behavior;
`flushPortalAndAppConfig()` bypasses that delay. Basic dynamic controls update live state and
MQTT immediately, with a debounced journal write afterward. Their change callback
runs on live application; `onPortalAndAppConfigSaved` reports durable completion. Ordinary
persistent code changes invoke the callback after commit. Accepted MQTT control
commands update live state and invoke the callback on receipt, independently
of flash activity, just like portal controls. Persistence remains debounced and
coalesces commands while storage is busy. A previously reported hardware state cannot hide the
requested value; later hardware confirmation remains the application’s responsibility.
Save completion does not repeat the callback. Unchanged effective values do not
invoke the change callback. Startup restoration uses
`onPortalAndAppConfigReady`, not change callbacks. Keep callbacks brief.

Descriptor-based fields use built-in presets with optional `ha` and `controls`
overrides. The complete reference below describes every supported type.

### Home Assistant entities without panel pages

Use `addAppConfigEntity()` for telemetry or controls that belong in Home Assistant
but do not need a dynamic page. Pass a single field object, not a page containing
`fields`. All supported controls use the same validation,
MQTT topic namespace, callbacks, state setters and persistence rules as panel
fields. Entity IDs must be unique across the entire application. HA-only entities
have their own source registry, consume no slots from the 16 panel pages, and do
not appear in `/api/pages` or the portal menu. Their current values remain available
through `getAppConfigValue()` and the complete `/api/app` values endpoint. The 1024-field and configurable source
budgets cover both registries together. `ARDPORTAL_ENABLE_HA` and
`ARDPORTAL_ENABLE_DYNAMIC_PAGES` must be enabled.

```cpp
const char TEMPERATURE_ENTITY[] PROGMEM = R"JSON({
  "id":"air_temperature", "name":"Air temperature", "type":"text",
  "default":0, "class":"temperature",
  "state_class":"measurement"
})JSON";

// During setup; check registration failures.
if (!portal.addAppConfigEntity(FPSTR(TEMPERATURE_ENTITY)))
  Serial.println("Invalid HA entity");

// After portalAndAppConfigReady(), report sensor readings in RAM.
portal.setAppConfigStateValue("air_temperature", 22.5);
```

For writable entities, register `onAppConfigValueChanged()` or
`onAppConfigCommand()` exactly as for panel controls. Reporting a sensor value
with `setAppConfigStateValue()` does not write flash. See
`examples/HomeAssistantEntities/HomeAssistantEntities.ino` for an HA sensor and
switch with no dynamic panel pages.

### Bounded Home Assistant work

```cpp
ArdPortal::Options options;
options.haWorkBudgetMs = 2;
options.haMaxOperationsPerLoop = 1;
portal.begin(options);
```

`haWorkBudgetMs` limits how long HA may start additional work during a `loop()` call; `haMaxOperationsPerLoop` limits the number of operations. Both must be nonzero. Keep the defaults unless measurements show that your application needs a different balance between HA updates, HTTP and UART responsiveness.

The time budget is cooperative: parsing/normalizing one field, serializing a
payload or a core transport call cannot be interrupted and may exceed it.
Application callbacks and MQTT connection/TLS handshakes have their own timing;
this budget is not a hard deadline for the entire portal loop.

Adding a page does not restart discovery of already registered entities. Broker reconnects and Home Assistant's `online` birth message request rediscovery; periodic discovery is disabled by default. PROGMEM definitions must remain immutable and available for the portal lifetime.

## State, availability and actions

`stat/<device>/availability` is retained `online` while connected and retained
`offline` through the MQTT Last Will when the connection is lost. This is broker
connectivity, not proof that an actuator accepted its last command.

For editable values, `onAppConfigValueChanged` delivers requested values from
Portal/HA/MQTT. For explicit buttons/actions, `onAppConfigCommand` accepts or
rejects the action. Report the actual resulting state with
`setAppConfigStateValue`. Avoid immediately republishing an old hardware sample
as though it were confirmation of the new request.

For dependencies, false conditions remove retained Discovery and true conditions
restore it. This intentionally affects entity existence; it does not set HA card
visibility. See [Dependent fields](https://github.com/rkubera/ArdPortal/wiki/Dependent-Fields).

## Switch and light display

Basic switches declare their state topic and default icon `mdi:toggle-switch`.
ArdPortal does not inject `optimistic:false` or overwrite an explicit optimistic
option. HA's default confirmed-state behavior applies when a state topic exists;
`ha.optimistic:true` opts into assumed state. Two lightning action buttons in HA
can therefore originate in the supplied definition, not in the icon.

The built-in `light` uses JSON, RGB and brightness with a state topic. HA offers
on/off, brightness and color. Color temperature, effects, white channels and
other light capabilities need consistent custom Discovery, controls and hardware
handling; the built-in RGB preset does not implement them automatically.

## Discovery inspection

```cpp
String error;
const auto field = ArdJSON::JSON.parse(
    "{\"id\":\"temperature\",\"type\":\"text\",\"name\":\"Temperature\",\"default\":0}", &error);
String discovery = portal.homeAssistant().discoveryConfig(field, &error);
// This builds a document for inspection; it does not register or publish it.
```

Reserved identity/name/device/availability fields are managed by ArdPortal.
Direct topic properties in `ha` support `$state`, `$command`,
`$command:<suffix>` and `$device` as described in [MQTT messages](https://github.com/rkubera/ArdPortal/wiki/MQTT-Messages).

Official references: [MQTT integration](https://www.home-assistant.io/integrations/mqtt/),
[MQTT Light](https://www.home-assistant.io/integrations/light.mqtt/) and
[MQTT Switch](https://www.home-assistant.io/integrations/switch.mqtt/).

---

[Back to documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)
