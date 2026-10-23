# event control

[Control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls) · [Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

![Portal event control](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/control-event.jpg)

`class` is always optional. See [the shared class catalogue](Control-Classes) for valid values, defaults and protocol mapping limits.

**Minimal field JSON:**

```json
{
  "id": "event",
  "type": "event",
  "name": "Event"
}
```

**Full field JSON:**

```json
{
  "id": "event",
  "type": "event",
  "name": "Event",
  "default": {
    "event_type": "press"
  },
  "readonly": true,
  "controls": [],
  "event_types": ["press", "double_press", "long_press"],
  "ha": {"state_topic": "$state"}
}
```

**Properties in the full example:**

| Property | Required? | Meaning / default |
| --- | --- | --- |
| `id` | Required | Stable application key; unique across registered pages. |
| `type` | Required | Enabled field type. |
| `name` | Required title¹ | Display title; may be replaced by a nonempty `names` object. |
| `default` | **Optional** | Omit to use the built-in initial value. |
| `readonly` | **Optional** | Omit to use the preset editability policy. |
| `controls` | **Optional** | Omit to use the preset controls; supplying an array replaces them. |
| `ha` | **Optional** | Omit to use automatic HA discovery; supplied properties override defaults. |

¹ `name` can be replaced by `names`. Other shared optional properties are listed [in the catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls#required-and-optional-properties).


**Default / example:** `{"event_type":"press"}`; preset types `press`, `double_press`, `long_press`

Publish an object whose `event_type` belongs to `event_types`. Additional event data may be included. See the C++ publication example below.

For `event`, a longer C++ example avoids confusing JSON quoting:

```cpp
ArdJSON::JSONVar event = ArdJSON::JSONVar::object();
event["event_type"] = "double_press";
event["button"] = 1;
portal.emitAppConfigEvent("wall_button", event);
```

## Integration with Home Assistant

**MQTT component:** `event`. Requires the control’s feature flag, dynamic pages, MQTT and HA support to be enabled. ArdPortal registers MQTT Discovery and routes state/commands; the sketch implements device behavior. Action callbacks acknowledge acceptance, not physical completion.

**Supported by ArdPortal:** Transient event objects are published using `emitAppConfigEvent()`. The preset allows `press`, `double_press` and `long_press` in `event_type`; `event_types` can supply a matching custom list.

**Not provided by the built-in integration / limits:** Events are not retained configuration or editable HA state. ArdPortal does not detect hardware gestures or keep a durable event history; the application emits each occurrence.

Explicit `ha` properties can override Discovery metadata. They do not create additional hardware behavior, application value properties or command handlers by themselves. See [HA integration](https://github.com/rkubera/ArdPortal/wiki/Home-Assistant) for the shared setup.

## Integration with Zigbee

**Status: not mapped by ArdPortal.** Transient application events are not translated into Zigbee button/gesture events. Enabling Zigbee does not create an endpoint for this field. The field remains usable in the portal and through MQTT/HA when those features are enabled.

This describes the current ArdPortal implementation, not a claim that Zigbee cannot represent a similar device through a standard cluster or a gateway-specific converter.

**Possible extension, not implemented:** On/Off or Scenes client commands; optionally Multistate Input + event counter. Known actions can be sent as commands to bound peers/coordinator. Arbitrary press/double-press/long-press event names need a device converter and a repeat/event-sequence mechanism. A state attribute alone loses consecutive identical events; it is not equivalent to an event stream. See the [mapping feasibility table](https://github.com/rkubera/ArdPortal/wiki/Zigbee#possible-mappings-not-yet-implemented).

See [Zigbee integration](https://github.com/rkubera/ArdPortal/wiki/Zigbee) for pairing, endpoint lifecycle, gateway limitations and OTA requirements.

## Read and update values from C++

These examples use the field ID from the JSON above and an existing `ArdPortal portal`. Run them after page registration has finished and `portal.portalAndAppConfigReady()` is true. Call `portal.loop()` frequently. Register callbacks once in `setup()`; do not repeat setters on every loop iteration.

This field represents a transient action/event, not a durable setting. Do not persist its command and automatically replay it after restart. The state setters are not a substitute for action execution or event publication.

### Publish an occurrence and read the current local value

```cpp
ArdJSON::JSONVar occurrence = ArdJSON::JSON.parse(R"JSON({"event_type":"double_press"})JSON");
if (!occurrence.isValid() || !portal.emitAppConfigEvent("event", occurrence)) {
  Serial.println("Event rejected: MQTT disconnected, queue full or invalid value");
}
const ArdJSON::JSONVar current = portal.getAppConfigValue("event");
if (current.isValid()) Serial.println(ArdJSON::JSON.stringify(current));
```

An accepted event updates local RAM and, with MQTT enabled, queues a nonretained message. It does not write ArdFS. With MQTT enabled, a disconnected broker or full queue returns `false`; the occurrence is not stored for replay. Without MQTT, supported events update local RAM only.

### Automatic persistence

This control has no persistent setting value: actions and events are transient. Do not save its command as a setting or replay it after restart. Automatic ArdFS persistence applies to persistent setting controls through `setAppConfigValue()`, not to this action/event field. No separate storage key or manual ArdFS write is needed for using this control.

[Back to control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls)
