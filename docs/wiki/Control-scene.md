# scene control

[Control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls) · [Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

![Portal scene control](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/control-scene.jpg)

`class` is always optional. See [the shared class catalogue](Control-Classes) for valid values, defaults and protocol mapping limits.

**Minimal field JSON:**

```json
{
  "id": "scene",
  "type": "scene",
  "name": "Scene"
}
```

**Full field JSON:**

```json
{
  "id": "scene",
  "type": "scene",
  "name": "Scene",
  "default": null,
  "controls": [
    {
      "key": "",
      "type": "action",
      "label": "ui_108",
      "command": "",
      "payload": "ON"
    }
  ]
}
```

**Properties in the full example:**

| Property | Required? | Meaning / default |
| --- | --- | --- |
| `id` | Required | Stable application key; unique across registered pages. |
| `type` | Required | Enabled field type. |
| `name` | Required title¹ | Display title; may be replaced by a nonempty `names` object. |
| `default` | **Optional** | Omit to use the built-in initial value. |
| `controls` | **Optional** | Omit to use the preset controls; supplying an array replaces them. |

¹ `name` can be replaced by `names`. Other shared optional properties are listed [in the catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls#required-and-optional-properties).


**Appearance:** Gray rounded button with the scene name

**Value:** Literal string `ON`; activate a predefined application scene.

The main sketch groups two scene buttons in one Scenes field on the Actions
page: Morning (`demo_scene`)
and Evening (`demo_scene_evening`). Both receive `ON` with an empty command
suffix. Their callback reports the demo light as on, using bright white
(brightness 200) for Morning and dim warm orange (brightness 40) for Evening.
Replace that state simulation with nonblocking hardware control in your program.
Scene fields with the same optional `group.id` share one portal fieldset.
`group.name` and optional `group.names` label that group. Each scene keeps its own
field ID, MQTT topic and HA discovery entity.

## Integration with Home Assistant

**MQTT component:** `scene`. Requires the control’s feature flag, dynamic pages, MQTT and HA support to be enabled. ArdPortal registers MQTT Discovery and routes state/commands; the sketch implements device behavior. Action callbacks acknowledge acceptance, not physical completion.

**Supported by ArdPortal:** Activating the HA scene sends `ON` and invokes `onAppConfigCommand`. The application can implement a scene involving its own outputs/settings.

**Not provided by the built-in integration / limits:** No built-in scene recording, snapshot/restore, HA entity orchestration, or retained active/inactive state. Portal grouping does not create a Zigbee or HA scene table.

Explicit `ha` properties can override Discovery metadata. They do not create additional hardware behavior, application value properties or command handlers by themselves. See [HA integration](https://github.com/rkubera/ArdPortal/wiki/Home-Assistant) for the shared setup.

## Integration with Zigbee

**Implemented as a composite mapping.** The functions below are separate standard endpoints on the same physical Zigbee device. The optional generated Zigbee2MQTT converter and ZHA quirk add names and option/state semantics; a composite mapping does not guarantee one combined gateway entity. Requires ESP32-C5/C6/H2, Zigbee and dynamic pages enabled, and the field’s control flag enabled. MQTT/HA is not required.

**Standard mapping:** Momentary On/Off.

**Supported by ArdPortal:** Sending ON invokes the preset ON scene action through onAppConfigCommand. The endpoint returns to OFF after processing.

**Not provided / limits:** It can appear as a switch, not a scene. There is no Scenes/Groups scene table or standard Recall Scene mapping for this field. The application implements the scene; idle OFF does not mean a scene was undone.

Endpoint IDs follow registration order, then the preset’s `controls` order; read-only measurement/status endpoints follow the editable/action endpoints. Composite fields consume multiple slots in the shared 239-endpoint budget. Startup stages one endpoint per loop and reports one endpoint per loop. Values and action callbacks use the existing AppConfig routes and `persist`; accepting an action is not hardware completion.

Standard Basic ProductLabel contains `field name / property or action`, truncated to 63 UTF-8 bytes; Analog/Multistate endpoints also carry their Description. A gateway is not required to use these labels or expose all these clusters. Option/state names are documented here but are not currently sent as StateText. Unknown discrete readings set the standard status fault flag and leave the last representable ordinal unchanged.

`visibleWhen` does not remove endpoints or create a dynamic Zigbee form. Dependency rules do not affect Zigbee control visibility or command acceptance. Native endpoint definitions freeze at startup; changes need restart. Physical coordinator interoperability is still unverified.

See [Zigbee integration](https://github.com/rkubera/ArdPortal/wiki/Zigbee) and [page/order limitations](https://github.com/rkubera/ArdPortal/wiki/Zigbee#portal-pages-and-control-order).

## Integration with Matter

**Native mapping:** Not supported by the current ArdPortal Matter adapter.

**Supported by ArdPortal:** This control remains available in the portal and through its existing integrations described above; it does not create a Matter endpoint.

**Not provided / limits:** Matter does not transport scene activation actions for this control. Adding `class` or `ha` metadata does not enable it, and MQTT Discovery settings do not configure Matter. This is an ArdPortal adapter limitation, not a statement that the Matter standard has no related device type. See [Matter control mappings](Matter#mappings) for supported alternatives.

## Read and update values from C++

These examples use the field ID from the JSON above and an existing `ArdPortal portal`. Run them after page registration has finished and `portal.portalAndAppConfigReady()` is true. Call `portal.loop()` frequently. Register callbacks once in `setup()`; do not repeat setters on every loop iteration.

This field represents a transient action/event, not a durable setting. Do not persist its command and automatically replay it after restart. The state setters are not a substitute for action execution or event publication.

### Handle an action without saving it

```cpp
portal.onAppConfigCommand([](const String& field, const String& command,
                            const ArdJSON::JSONVar& value,
                            ArdPortal::ChangeSource source) {
  if (field != "scene") return false; // Add branches for other action IDs.
  Serial.println(ArdJSON::JSON.stringify(value));
  // Queue the hardware/application action here; keep this callback nonblocking.
  return true; // Acceptance, not confirmation of physical completion.
});
```

Portal and MQTT actions reach this callback. The action payload is available as `value`; `getAppConfigValue("scene")` returns the field's null state, not a command history. Keep a separate application variable if the last action needs to remain available in RAM.

### Automatic persistence

This control has no persistent setting value: actions and events are transient. Do not save its command as a setting or replay it after restart. Automatic ArdFS persistence applies to persistent setting controls through `setAppConfigValue()`, not to this action/event field. No separate storage key or manual ArdFS write is needed for using this control.

[Back to control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls)
