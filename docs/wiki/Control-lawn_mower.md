# lawn_mower control

[Control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls) · [Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

![Portal lawn_mower control](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/control-lawn_mower.jpg)

**Minimal field JSON:**

```json
{
  "id": "lawn_mower",
  "type": "lawn_mower",
  "name": "Lawn mower"
}
```

**Full field JSON:**

```json
{
  "id": "lawn_mower",
  "type": "lawn_mower",
  "name": "Lawn mower",
  "default": "docked",
  "controls": [
    {
      "key": "",
      "type": "action",
      "label": "ui_139",
      "command": "start",
      "payload": "START"
    },
    {
      "key": "",
      "type": "action",
      "label": "ui_134",
      "command": "pause",
      "payload": "PAUSE"
    },
    {
      "key": "",
      "type": "action",
      "label": "ui_135",
      "command": "dock",
      "payload": "DOCK"
    }
  ],
  "ha": {
    "activity_state_topic": "$state",
    "start_mowing_command_topic": "$command:start",
    "pause_command_topic": "$command:pause",
    "dock_command_topic": "$command:dock"
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


The control provides Start mowing, Pause and Return to base buttons. Default
state is `"docked"`; command suffix/payload pairs are `_start`/`START`,
`_pause`/`PAUSE` and `_dock`/`DOCK`. Report `mowing`, `paused`, `returning` or
`docked` to update button selection. Raw state text is hidden. Scheduling,
navigation and safety interlocks belong to the application.

## Read and update values from C++

These examples use the field ID from the JSON above and an existing `ArdPortal portal`. Run them after page registration has finished and `portal.portalAndAppConfigReady()` is true. Call `portal.loop()` frequently. Register callbacks once in `setup()`; do not repeat setters on every loop iteration.

### Set the value with automatic ArdFS persistence

Set **`"persist": true`** on this field in its page JSON. It is optional for editable fields whose preset already persists settings, but required to override a RAM-only preset such as a sensor. Calling `setAppConfigValue()` sets the control value and ArdPortal automatically saves AppConfig through its integrated ArdFS storage. Do not create an ArdFS object or call a separate write/flush method. The automatic save is coalesced and advanced by subsequent `portal.loop()` calls.

```cpp
// Use this path when the value is a setting that should survive restart.
ArdJSON::JSONVar next = "docked";
if (!next.isValid() || !portal.setAppConfigValue("lawn_mower", next)) {
  Serial.println("Setting rejected or storage temporarily unavailable");
}
```

`true` from the setter means acceptance, not a completed flash write. Configure this callback once to observe the result:

```cpp
portal.onPortalAndAppConfigSaved([](bool ok, const String& error) {
  if (!ok) Serial.println(error);
});
```

### Update only RAM

```cpp
// Use this alternative for live readings/status; never writes ArdFS.
ArdJSON::JSONVar next = "docked";
if (!next.isValid() || !portal.setAppConfigStateValue("lawn_mower", next, false)) {
  Serial.println("Runtime value rejected");
}
```

The third argument `false` avoids queuing an immediate MQTT publication. Use `true` or omit it to queue changed state. RAM updates work regardless of `persist`. Alternatively, with **`"persist": false`**, `setAppConfigValue()` also follows the RAM-only path.

### Read the current value

```cpp
const ArdJSON::JSONVar current = portal.getAppConfigValue("lawn_mower");
if (!current.isUndefined() && current.isValid()) {
  Serial.println(ArdJSON::JSON.stringify(current));
}
```

The getter returns the effective field value: RAM override first, then saved AppConfig, then the definition default. A RAM-only update is lost on restart; the saved value/default becomes effective again. **Choose the persistent or RAM path for a given value.** A RAM override can mask a later persistent change for the same ID; there is no API to clear just that override. Prefer separate setting and telemetry IDs when both are needed.

[Back to control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls)
