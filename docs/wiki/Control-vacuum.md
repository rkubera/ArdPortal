# vacuum control

[Control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls) · [Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

![Portal vacuum control](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/control-vacuum.jpg)

**Minimal field JSON:**

```json
{
  "id": "vacuum",
  "type": "vacuum",
  "name": "Vacuum"
}
```

**Full field JSON:**

```json
{
  "id": "vacuum",
  "type": "vacuum",
  "name": "Vacuum",
  "default": {
    "state": "docked",
    "fan_speed": "normal"
  },
  "controls": [
    {
      "key": "",
      "type": "action",
      "label": "ui_133",
      "command": "",
      "payload": "start"
    },
    {
      "key": "",
      "type": "action",
      "label": "ui_119",
      "command": "",
      "payload": "stop"
    },
    {
      "key": "",
      "type": "action",
      "label": "ui_134",
      "command": "",
      "payload": "pause"
    },
    {
      "key": "",
      "type": "action",
      "label": "ui_135",
      "command": "",
      "payload": "return_to_base"
    },
    {
      "key": "",
      "type": "action",
      "label": "ui_136",
      "command": "",
      "payload": "locate"
    },
    {
      "key": "",
      "type": "action",
      "label": "ui_137",
      "command": "",
      "payload": "clean_spot"
    },
    {
      "key": "fan_speed",
      "type": "select",
      "label": "ui_104",
      "command": "fan",
      "options": [
        "quiet",
        "normal",
        "turbo"
      ]
    },
    {
      "key": "",
      "type": "action_json",
      "label": "ui_138",
      "command": "custom"
    }
  ],
  "ha": {
    "command_topic": "$command",
    "state_topic": "$state",
    "supported_features": [
      "start",
      "stop",
      "pause",
      "return_home",
      "status",
      "locate",
      "clean_spot",
      "fan_speed",
      "send_command"
    ],
    "set_fan_speed_topic": "$command:fan",
    "fan_speed_list": [
      "quiet",
      "normal",
      "turbo"
    ],
    "send_command_topic": "$command:custom"
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


The control provides Clean, Stop, Pause, Return to base, Locate and Clean spot
buttons, a fan-speed selector, and a JSON custom-command input. Its default state is
`{"state":"docked","fan_speed":"normal"}`. Base-topic action payloads are
`start`, `stop`, `pause`, `return_to_base`, `locate` and `clean_spot`.
Fan speed accepts `quiet`, `normal` or `turbo` on `_fan`; custom JSON uses `_custom`.
States such as `cleaning`, `idle`, `paused`, `returning` and `docked` highlight
matching operating buttons. The program can additionally report an optional
`action` property (`start`, `stop`, `pause`, `return_to_base`, `locate`, or
`clean_spot`) using `setAppConfigStateValue`. When present, this property selects the
active button, including Locate and Clean spot, until the program updates it.
There is no automatic timeout or local selection based on a click. The demo
callback reports both operating state and the selected action. Raw state text is hidden.

## Read and update values from C++

These examples use the field ID from the JSON above and an existing `ArdPortal portal`. Run them after page registration has finished and `portal.portalAndAppConfigReady()` is true. Call `portal.loop()` frequently. Register callbacks once in `setup()`; do not repeat setters on every loop iteration.

### Set the value with automatic ArdFS persistence

Set **`"persist": true`** on this field in its page JSON. It is optional for editable fields whose preset already persists settings, but required to override a RAM-only preset such as a sensor. Calling `setAppConfigValue()` sets the control value and ArdPortal automatically saves AppConfig through its integrated ArdFS storage. Do not create an ArdFS object or call a separate write/flush method. The automatic save is coalesced and advanced by subsequent `portal.loop()` calls.

```cpp
// Use this path when the value is a setting that should survive restart.
ArdJSON::JSONVar next = ArdJSON::JSON.parse(R"JSON({"state":"docked","fan_speed":"normal"})JSON");
if (!next.isValid() || !portal.setAppConfigValue("vacuum", next)) {
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
ArdJSON::JSONVar next = ArdJSON::JSON.parse(R"JSON({"state":"docked","fan_speed":"normal"})JSON");
if (!next.isValid() || !portal.setAppConfigStateValue("vacuum", next, false)) {
  Serial.println("Runtime value rejected");
}
```

The third argument `false` avoids queuing an immediate MQTT publication. Use `true` or omit it to queue changed state. RAM updates work regardless of `persist`. Alternatively, with **`"persist": false`**, `setAppConfigValue()` also follows the RAM-only path.

### Read the current value

```cpp
const ArdJSON::JSONVar current = portal.getAppConfigValue("vacuum");
if (!current.isUndefined() && current.isValid()) {
  Serial.println(ArdJSON::JSON.stringify(current));
}
```

The getter returns the effective field value: RAM override first, then saved AppConfig, then the definition default. A RAM-only update is lost on restart; the saved value/default becomes effective again. **Choose the persistent or RAM path for a given value.** A RAM override can mask a later persistent change for the same ID; there is no API to clear just that override. Prefer separate setting and telemetry IDs when both are needed.

[Back to control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls)
