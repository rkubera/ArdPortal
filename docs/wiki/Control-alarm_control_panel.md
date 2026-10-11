# alarm_control_panel control

[Control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls) · [Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

![Portal alarm_control_panel control](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/control-alarm_control_panel.jpg)

**Minimal field JSON:**

```json
{
  "id": "alarm_control_panel",
  "type": "alarm_control_panel",
  "name": "Alarm control panel"
}
```

**Full field JSON:**

```json
{
  "id": "alarm_control_panel",
  "type": "alarm_control_panel",
  "name": "Alarm control panel",
  "default": "disarmed",
  "controls": [
    {
      "key": "",
      "type": "action",
      "label": "ui_124",
      "command": "",
      "payload": "ARM_HOME"
    },
    {
      "key": "",
      "type": "action",
      "label": "ui_125",
      "command": "",
      "payload": "ARM_AWAY"
    },
    {
      "key": "",
      "type": "action",
      "label": "ui_126",
      "command": "",
      "payload": "ARM_NIGHT"
    },
    {
      "key": "",
      "type": "action",
      "label": "ui_127",
      "command": "",
      "payload": "DISARM"
    },
    {
      "key": "",
      "type": "action",
      "label": "ui_128",
      "command": "",
      "payload": "TRIGGER"
    }
  ],
  "ha": {
    "command_topic": "$command",
    "state_topic": "$state",
    "code_arm_required": false,
    "code_disarm_required": false,
    "code_trigger_required": false
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


The control provides Arm home, Arm away, Arm night, Disarm and Trigger
buttons. Its default state is `"disarmed"`; commands are `ARM_HOME`, `ARM_AWAY`,
`ARM_NIGHT`, `DISARM` and `TRIGGER` on the base topic. Matching reported states
are `armed_home`, `armed_away`, `armed_night`, `disarmed` and `triggered`.
The preset does not require a code and does not offer a PIN-entry field.
Changing HA code-related properties alone does not implement code handling in
the portal or firmware. The raw state string is hidden; selected buttons reflect
reported state.

## Read and update values from C++

These examples use the field ID from the JSON above and an existing `ArdPortal portal`. Run them after page registration has finished and `portal.portalAndAppConfigReady()` is true. Call `portal.loop()` frequently. Register callbacks once in `setup()`; do not repeat setters on every loop iteration.

### Set the value with automatic ArdFS persistence

Set **`"persist": true`** on this field in its page JSON. It is optional for editable fields whose preset already persists settings, but required to override a RAM-only preset such as a sensor. Calling `setAppConfigValue()` sets the control value and ArdPortal automatically saves AppConfig through its integrated ArdFS storage. Do not create an ArdFS object or call a separate write/flush method. The automatic save is coalesced and advanced by subsequent `portal.loop()` calls.

```cpp
// Use this path when the value is a setting that should survive restart.
ArdJSON::JSONVar next = "disarmed";
if (!next.isValid() || !portal.setAppConfigValue("alarm_control_panel", next)) {
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
ArdJSON::JSONVar next = "disarmed";
if (!next.isValid() || !portal.setAppConfigStateValue("alarm_control_panel", next, false)) {
  Serial.println("Runtime value rejected");
}
```

The third argument `false` avoids queuing an immediate MQTT publication. Use `true` or omit it to queue changed state. RAM updates work regardless of `persist`. Alternatively, with **`"persist": false`**, `setAppConfigValue()` also follows the RAM-only path.

### Read the current value

```cpp
const ArdJSON::JSONVar current = portal.getAppConfigValue("alarm_control_panel");
if (!current.isUndefined() && current.isValid()) {
  Serial.println(ArdJSON::JSON.stringify(current));
}
```

The getter returns the effective field value: RAM override first, then saved AppConfig, then the definition default. A RAM-only update is lost on restart; the saved value/default becomes effective again. **Choose the persistent or RAM path for a given value.** A RAM override can mask a later persistent change for the same ID; there is no API to clear just that override. Prefer separate setting and telemetry IDs when both are needed.

[Back to control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls)
