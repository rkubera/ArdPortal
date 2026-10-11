# scene control

[Control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls) · [Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

![Portal scene control](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/control-scene.jpg)

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
  ],
  "ha": {
    "command_topic": "$command",
    "payload_on": "ON"
  }
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
| `ha` | **Optional** | Omit to use automatic HA discovery; supplied properties override defaults. |

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
