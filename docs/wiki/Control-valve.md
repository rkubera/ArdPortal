# valve control

[Control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls) · [Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

![Portal valve control](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/control-valve.jpg)

**Minimal field JSON:**

```json
{
  "id": "valve",
  "type": "valve",
  "name": "Valve"
}
```

**Full field JSON:**

```json
{
  "id": "valve",
  "type": "valve",
  "name": "Valve",
  "default": {
    "state": "closed",
    "position": 0
  },
  "controls": [
    {
      "key": "",
      "type": "action",
      "label": "ui_117",
      "command": "",
      "payload": "100"
    },
    {
      "key": "",
      "type": "action",
      "label": "ui_118",
      "command": "",
      "payload": "0"
    },
    {
      "key": "position",
      "type": "slider",
      "label": "ui_120",
      "command": "",
      "min": 0,
      "max": 100,
      "step": 1
    }
  ],
  "ha": {
    "command_topic": "$command",
    "state_topic": "$state",
    "reports_position": true
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


The control provides Open/Close buttons and a position slider (0–100, step 1).
Its default is `{"state":"closed","position":0}`. The built-in Open and Close
buttons send action payloads `100` and `0`; intermediate numeric positions use
the same base command topic. Endpoint action payloads invoke `onAppConfigCommand`.
Report the resulting state/position separately, for example
`{"state":"open","position":100}`.

## Read and update values from C++

These examples use the field ID from the JSON above and an existing `ArdPortal portal`. Run them after page registration has finished and `portal.portalAndAppConfigReady()` is true. Call `portal.loop()` frequently. Register callbacks once in `setup()`; do not repeat setters on every loop iteration.

### Set the value with automatic ArdFS persistence

Set **`"persist": true`** on this field in its page JSON. It is optional for editable fields whose preset already persists settings, but required to override a RAM-only preset such as a sensor. Calling `setAppConfigValue()` sets the control value and ArdPortal automatically saves AppConfig through its integrated ArdFS storage. Do not create an ArdFS object or call a separate write/flush method. The automatic save is coalesced and advanced by subsequent `portal.loop()` calls.

```cpp
// Use this path when the value is a setting that should survive restart.
ArdJSON::JSONVar next = ArdJSON::JSON.parse(R"JSON({"state":"closed","position":0})JSON");
if (!next.isValid() || !portal.setAppConfigValue("valve", next)) {
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
ArdJSON::JSONVar next = ArdJSON::JSON.parse(R"JSON({"state":"closed","position":0})JSON");
if (!next.isValid() || !portal.setAppConfigStateValue("valve", next, false)) {
  Serial.println("Runtime value rejected");
}
```

The third argument `false` avoids queuing an immediate MQTT publication. Use `true` or omit it to queue changed state. RAM updates work regardless of `persist`. Alternatively, with **`"persist": false`**, `setAppConfigValue()` also follows the RAM-only path.

### Read the current value

```cpp
const ArdJSON::JSONVar current = portal.getAppConfigValue("valve");
if (!current.isUndefined() && current.isValid()) {
  Serial.println(ArdJSON::JSON.stringify(current));
}
```

The getter returns the effective field value: RAM override first, then saved AppConfig, then the definition default. A RAM-only update is lost on restart; the saved value/default becomes effective again. **Choose the persistent or RAM path for a given value.** A RAM override can mask a later persistent change for the same ID; there is no API to clear just that override. Prefer separate setting and telemetry IDs when both are needed.

[Back to control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls)
