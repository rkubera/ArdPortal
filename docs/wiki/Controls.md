# All controls

[Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home) · [All topics](https://github.com/rkubera/ArdPortal/wiki/Home#start-here) · [Source](https://github.com/rkubera/ArdPortal)

## Choose a control

| Control | Portal widget |
| --- | --- |
| [slider](https://github.com/rkubera/ArdPortal/wiki/Control-slider) | Numeric slider |
| [text](https://github.com/rkubera/ArdPortal/wiki/Control-text) | Read-only measurement or text |
| [select](https://github.com/rkubera/ArdPortal/wiki/Control-select) | Dropdown selection |
| [edit](https://github.com/rkubera/ArdPortal/wiki/Control-edit) | Editable text |
| [switch](https://github.com/rkubera/ArdPortal/wiki/Control-switch) | On/off toggle |
| [climate](https://github.com/rkubera/ArdPortal/wiki/Control-climate) | Thermostat dial |
| [binary_sensor](https://github.com/rkubera/ArdPortal/wiki/Control-binary_sensor) | Binary status indicator |
| [date](https://github.com/rkubera/ArdPortal/wiki/Control-date) | Date input |
| [time](https://github.com/rkubera/ArdPortal/wiki/Control-time) | Time input |
| [datetime](https://github.com/rkubera/ArdPortal/wiki/Control-datetime) | Date and time input |
| [button](https://github.com/rkubera/ArdPortal/wiki/Control-button) | Single action |
| [scene](https://github.com/rkubera/ArdPortal/wiki/Control-scene) | Scene action |
| [notify](https://github.com/rkubera/ArdPortal/wiki/Control-notify) | Text notification action |
| [infrared](https://github.com/rkubera/ArdPortal/wiki/Control-infrared) | JSON infrared action |
| [event](https://github.com/rkubera/ArdPortal/wiki/Control-event) | Event publication |
| [device_trigger](https://github.com/rkubera/ArdPortal/wiki/Control-device_trigger) | Automation trigger |
| [tag](https://github.com/rkubera/ArdPortal/wiki/Control-tag) | Scanned tag publication |
| [device_tracker](https://github.com/rkubera/ArdPortal/wiki/Control-device_tracker) | Presence state |
| [light](https://github.com/rkubera/ArdPortal/wiki/Control-light) | Power, brightness and RGB |
| [fan](https://github.com/rkubera/ArdPortal/wiki/Control-fan) | Power, speed and oscillation |
| [cover](https://github.com/rkubera/ArdPortal/wiki/Control-cover) | Opening, position and tilt |
| [valve](https://github.com/rkubera/ArdPortal/wiki/Control-valve) | Opening and position |
| [lock](https://github.com/rkubera/ArdPortal/wiki/Control-lock) | Lock actions |
| [alarm_control_panel](https://github.com/rkubera/ArdPortal/wiki/Control-alarm_control_panel) | Alarm actions |
| [humidifier](https://github.com/rkubera/ArdPortal/wiki/Control-humidifier) | Humidity dial and modes |
| [siren](https://github.com/rkubera/ArdPortal/wiki/Control-siren) | Power, tone, duration and volume |
| [vacuum](https://github.com/rkubera/ArdPortal/wiki/Control-vacuum) | Vacuum actions and speed |
| [lawn_mower](https://github.com/rkubera/ArdPortal/wiki/Control-lawn_mower) | Mower actions |
| [water_heater](https://github.com/rkubera/ArdPortal/wiki/Control-water_heater) | Water temperature and modes |
| [update](https://github.com/rkubera/ArdPortal/wiki/Control-update) | Firmware installation action |


The [complete current Zigbee coverage table](https://github.com/rkubera/ArdPortal/wiki/Zigbee#complete-current-control-coverage)
marks which fields are native, composite (several separate endpoints), generic
single-endpoint controls, or unmapped. All 30 types remain documented individually.

## Control catalogue

Control types can be **enabled or disabled at compile time with defines**. `ARDPORTAL_ENABLE_CONTROLS` selects the default for optional types; an individual flag such as `ARDPORTAL_ENABLE_CONTROL_LIGHT` overrides that type. See [Defines → Optional field types](https://github.com/rkubera/ArdPortal/wiki/Defines#optional-field-types) for every flag and its default. Set the flags before `#include <ArdPortal.h>` in the main sketch, then rebuild/upload the firmware. Dynamic portal pages also require `ARDPORTAL_ENABLE_DYNAMIC_PAGES`.

There are **30 field types**, presented in one catalogue below. Each screenshot shows the actual library portal renderer with illustrative sample values, captured in a local browser preview. These are portal screenshots, not Home Assistant cards or evidence of connected hardware.
The examples below describe the built-in presets shipped with this library,
not every capability that Home Assistant may support. Register a field with its
`id`, `type` and `name`; add only the options you need. Field IDs are stable
application keys, not C++ variable declarations.

Each control page also describes **Integration with Home Assistant** and
**Integration with Zigbee**, including the behavior ArdPortal implements and the
remaining gaps. Possible future cluster combinations are clearly marked as
unimplemented; see the [Zigbee feasibility table](https://github.com/rkubera/ArdPortal/wiki/Zigbee#possible-mappings-not-yet-implemented).

Every linked control page includes **minimal JSON** and **full JSON**, both ready to copy into a page's `fields` array. Minimal JSON contains only the properties needed to register the field. Full JSON makes the example configuration explicit, including defaults and the built-in `controls` and `ha` settings where applicable; it is not a requirement to supply every optional property. The property table on each control page identifies what can be omitted. `$state`, `$command` and `$command:<suffix>` are topic placeholders expanded by the library. Internal normalization properties are not needed in your JSON.

A page containing one or more of these objects has this structure:

```json
{
  "id": "settings",
  "name": "Settings",
  "order": 10,
  "fields": [
    {"id": "enabled", "type": "switch", "name": "Enabled", "default": false}
  ]
}
```

Each field ID must be unique across registered pages. See [Dynamic pages](https://github.com/rkubera/ArdPortal/wiki/Dynamic-Pages) for page registration.


## Required and optional properties

`id` and `type` are required for every field. A display title is also required: the examples use `name`; you may replace it with a nonempty `names` translation object. `names` is optional when `name` is present. The `slider` and `climate` fields also require `min` and `max`; `select` requires `options`. Other types obtain their default state and control/discovery definitions from the library presets.

Optional properties available across the catalogue:

| Property | Required? | Meaning when supplied / omitted |
| --- | --- | --- |
| `names` | Optional with `name` | Translated titles; see [Languages](https://github.com/rkubera/ArdPortal/wiki/Languages#multilingual-names-in-page-json). Without it, the default title is used. |
| `default` | Optional | Initial value. Omission uses the type's default; it does not remove the field. |
| `persist` | Optional | Storage policy. Omission uses the type policy; `false` keeps edits in RAM. See [Persistence and callbacks](#persistence-and-callbacks). |
| `visibleWhen` | Optional | Visibility rule; without it, there is no field-specific condition. Page conditions still apply. See [Dependent fields](https://github.com/rkubera/ArdPortal/wiki/Dependent-Fields). |
| `icon` | Optional | HA icon override. The preset/HA default is used when omitted. |
| `ha` | Optional | Overrides only supplied discovery properties. Omission uses automatic HA configuration, including topics and templates. |
| `unit_of_measurement`, `device_class`, `state_class`, `entity_category` | Optional, where supported by the HA component | Measurement/classification metadata. Choose values appropriate for that component. |

For fields with preset `controls`, the entire array is optional. If supplied, it **replaces** the preset array. Its entries must contain the properties required by their control type, such as a slider's range or an action's payload; see [Custom controls](#custom-controls). Optional `ha` overrides must remain consistent with those controls. For `select`, the `options` array is required, and each option requires `value` plus a title (`name` or `names`). For grouped scenes, `group` is optional; when supplied it needs `id` and a title. These are requirements inside an optional customization, not additional requirements for minimal field JSON.

## Shared state and command behavior

Some field types use built-in control descriptors. Unlike a `select` field, descriptor-based
select controls store their allowed values as an array of strings in `controls`.

Composite values must include all properties present in their declared `default`.
Changing one property through the portal preserves the remaining properties.
Additional telemetry properties are allowed, within the 1024-byte extended
composite-state limit. The application is responsible for operating hardware.

`button`, `scene`, `notify` and `infrared` keep `null` as their state/default. They invoke
`onAppConfigCommand`; they are not persistent switches and are not periodically replayed.
Action submission requires an installed `onAppConfigCommand` callback. Commands submitted
from the portal execute locally even when MQTT is disconnected. A callback returning
`false` rejects the command; returning `true` acknowledges acceptance, not physical
completion. This behavior also applies to action buttons in composite devices.
All use the base command topic.

Action-text and JSON inputs require an explicit Send. This avoids transmitting
an action on every keystroke. Action JSON is bounded to 1500 serialized bytes
with at most 512 nodes; full MQTT packet limits still apply.

`event`, `device_trigger` and `tag` are read-only in the portal. Use `emitAppConfigEvent` for an occurrence,
not a persistent setter: events are nonretained, queued only while MQTT is
connected, and are not replayed on reconnect or during periodic refresh.

## Custom controls

`ha` overrides discovery properties; `controls` replaces the entire built-in
control array rather than merging entries by key. `default` describes the complete
initial state. Keep all three consistent. The `slider` field declares its range directly; composite ranges and mode lists
need explicit control overrides.
Reserved discovery identity/name/device/availability properties are managed by
the library. `$state`, `$command` and `$command:<suffix>` are expanded in direct
string-valued `ha` properties, not recursively inside arbitrary nested JSON.

A control's `key` selects a property in a composite state; empty `key` selects the
whole scalar value. `command` is a suffix, without the leading underscore; empty
`command` uses the base field topic. `name` and optional `names` customize portal
labels; `label` can reference a catalog UI key. The available control descriptors
are:

| Control `type` | Additional properties | Behavior |
| --- | --- | --- |
| `slider` | `min`, `max`, positive `step` | Numeric range with live readout; values must match the range and step. |
| `switch` | Optional string `on`/`off` | Boolean by default, or the configured string values; rendered as a graphical switch. |
| `select` | `options: ["eco","normal"]` | String dropdown; this descriptor syntax differs from a `select` field's translated option objects. |
| `color` | None | RGB object with integer `r`, `g`, `b` channels, each 0–255. |
| `edit` | None | Editable string. |
| `date`, `time`, `datetime` | None | Strings following the formats listed above. |
| `action` | `payload` | Button invoking `onAppConfigCommand` with the declared payload. |
| `action_text` | None | Text input and explicit Send; invokes `onAppConfigCommand`. |
| `action_json` | None | JSON input and explicit Send; invokes `onAppConfigCommand`. |

For example, a humidifier with a narrower target range and a custom mode:

```json
{
  "id":"room_humidifier","type":"humidifier","name":"Room humidifier",
  "default":{"state":"OFF","humidity":45,"current_humidity":null,"mode":"quiet"},
  "ha":{"min_humidity":30,"max_humidity":70,"modes":["quiet","normal"]},
  "controls":[
    {"key":"state","type":"switch","name":"Power","command":"","on":"ON","off":"OFF"},
    {"key":"humidity","type":"slider","name":"Humidity","command":"humidity","min":30,"max":70,"step":1},
    {"key":"mode","type":"select","name":"Mode","command":"mode","options":["quiet","normal"]}
  ]
}
```

Changing a preset without matching its HA topics/templates can produce a portal
that works locally but does not communicate correctly with HA. The full demo in
the external `ArdUI.ino` demo and `examples/DynamicPages` are starting points; use actual hardware
feedback in production. The library does not enforce a whitelist of physical
operating states for action-only devices; the state examples above identify the
values recognized by the shipped portal button highlighting.

### Widget appearance and state feedback

The portal renders `humidifier` with the same circular dial as `climate`, using
percent humidity instead of temperature. Drag the dial, use the plus/minus
buttons or the arrow keys to change its target. Power and mode controls remain
available; updates from MQTT immediately refresh the widget. `water_heater` uses
the same dial for target temperature, displays current temperature and retains
its mode selector. All three dials mark the current measurement with a dot:
the arc is light below the lower value and darker between current and target
when the target is higher. Above the target the arc is gray; a current
measurement above the target is marked with a gray dot. The current marker is hidden when no measurement is known. In off mode, the
whole arc, target handle, current marker and selected off button are gray.
Cooling reverses the arc: gray below the target, dark blue from the target to
a higher current temperature, then light blue up to the maximum.
Both widgets respect the range and step of their slider
control definitions.

Action controls for `cover`, `valve`, `lock`, `alarm_control_panel`, `vacuum`
and `lawn_mower` use rounded buttons in a wrapping row. A green button reflects
the reported device state; clicking an action does not assume it succeeded.
Lock, Alarm Control Panel, Vacuum and Lawn Mower hide raw state strings; their selected buttons show
the reported state. The portal refreshes values after command completion as well
as on WebSocket updates. The main sketch simulates reported Lock/alarm/robot states;
hardware integrations must report the actual result with `setAppConfigStateValue`.
Sliders, selectors and custom-command inputs remain available below the row.

`binary_sensor` is displayed as a read-only status badge with a colored dot and
a localized Active/Inactive label. Missing measurements show Unknown state.
MQTT updates refresh the indicator immediately.

The field types cover the supported MQTT discovery entity
categories, including `device_automation` through `device_trigger`. Hardware
operations remain the application's responsibility. The built-in presets provide
common capabilities; advanced capabilities can be configured through `ha` and
`controls`. HA topic values can use `$state`, `$command` or `$command:<suffix>`;
these expand to the device's topics. Custom controls must match those options,
command suffixes and the declared state/default schema. Sensor metadata accepts
`unit_of_measurement`, `device_class`, `state_class` and `entity_category`.

Consecutive portal edits update the visible state immediately and coalesce into
the pending snapshot before flash writing begins.

### Persistence and callbacks

Editable extended fields default to durable storage (`persist: true`), so their
settings survive restart. Read-only telemetry and transient actions/events
default to RAM (`persist: false`) to avoid writing measurements to flash.
`setAppConfigValue` follows this policy for registered extended fields; an explicit
`persist: false` keeps a field volatile. This applies to form controls, composite controls, MQTT commands and `setAppConfigValue()`. A form containing both kinds saves only persistent fields; volatile changes remain in RAM. Forms containing only volatile fields do not start a storage transaction. Editable fields without `persist` retain their default durable behavior. `setAppConfigStateValue` always updates RAM only. Its optional third argument defaults
to `true` (publish changed state to MQTT). Pass `false` for local live updates
without queuing MQTT; use `queueAppConfigStatePublish(key)` to queue the latest value
through the cooperative publisher, even if unchanged. This returns queue success,
not broker confirmation. Periodic refresh and reconnect/get snapshots still apply;
set `Options::appStateIntervalMs=0` when managing your own refresh schedule. All
registered fields are read through `getAppConfigValue`.

```cpp
portal.setAppConfigStateValue("temperature", 22.5); // Telemetry: no flash write.
portal.onAppConfigCommand([](const String& field, const String& command,
                       const ArdJSON::JSONVar& value, ArdPortal::ChangeSource source) {
  // Accept/queue a nonblocking hardware action; return false if rejected.
  return true;
});
portal.emitAppConfigEvent("tag_reader", "tag-id"); // With MQTT enabled, connection required.
```

Actions require `onAppConfigCommand`. When MQTT is connected, accepted actions attempt to
queue a nonretained acceptance message containing `accepted`, `command` and `value`;
stateful entities also attempt to queue their reported state. Disconnection or a
full emission queue does not reject an action accepted by the callback.
Report actual hardware results with `setAppConfigStateValue`.
With MQTT enabled, events and action replies share an eight-message FIFO; emission
returns false when full or disconnected. Without MQTT, supported events update
local RAM state/revision only. `emitAppConfigEvent()` does not invoke the application
value-change callback. Events/actions are never retained or replayed periodically.
Binary console payloads are
summarized by byte count; they are not rendered as text.

See the official [MQTT discovery documentation](https://www.home-assistant.io/integrations/mqtt/)
and [MQTT climate documentation](https://www.home-assistant.io/integrations/climate.mqtt/).
Discovery tests use a simulated broker; physical device/HA interoperability still
requires testing with your installation. Generating/parsing JSON, callbacks and
existing core flash/TCP/TLS operations execute synchronously; network sends,
discovery refresh and storage work are scheduled incrementally by `loop()`.

## HA component mapping

| ArdPortal field | HA component |
| --- | --- |
| `slider` | `number` |
| `text` | `sensor` |
| `select` | `select` |
| `edit` | `text` |
| `switch`, `climate` | Same component name |
| `device_trigger` | `device_automation` |
| Other descriptor-based types | Matching component name |

The portal widget and the HA widget are separate UIs. A `light` advertises RGB
and brightness, so HA can show its native light color picker. It is not a
standalone HA `color` entity. A switch's `mdi:toggle-switch` icon does not change
HA's assumed-state behavior: an explicit `ha.optimistic:true` can still make HA
show two action buttons instead of one state toggle.
