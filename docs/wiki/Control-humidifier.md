# humidifier control

[Control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls) · [Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

![Portal humidifier control](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/control-humidifier.jpg)

**Minimal field JSON:**

```json
{
  "id": "humidifier",
  "type": "humidifier",
  "name": "Humidifier"
}
```

**Full field JSON:**

```json
{
  "id": "humidifier",
  "type": "humidifier",
  "name": "Humidifier",
  "default": {
    "state": "OFF",
    "humidity": 50,
    "current_humidity": null,
    "mode": "normal"
  },
  "controls": [
    {
      "key": "state",
      "type": "switch",
      "label": "ui_111",
      "command": "",
      "on": "ON",
      "off": "OFF"
    },
    {
      "key": "humidity",
      "type": "slider",
      "label": "ui_129",
      "command": "humidity",
      "min": 0,
      "max": 100,
      "step": 1
    },
    {
      "key": "mode",
      "type": "select",
      "label": "ui_103",
      "command": "mode",
      "options": [
        "normal",
        "eco"
      ]
    }
  ],
  "ha": {
    "command_topic": "$command",
    "state_topic": "$state",
    "state_value_template": "{{ value_json.state }}",
    "target_humidity_command_topic": "$command:humidity",
    "target_humidity_state_topic": "$state",
    "target_humidity_state_template": "{{ value_json.humidity }}",
    "current_humidity_topic": "$state",
    "current_humidity_template": "{{ value_json.current_humidity | default('', true) }}",
    "mode_command_topic": "$command:mode",
    "mode_state_topic": "$state",
    "mode_state_template": "{{ value_json.mode }}",
    "modes": [
      "normal",
      "eco"
    ]
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


The control uses a circular humidity dial (0–100%, step 1), power toggle and
mode selector. Its modes use rounded buttons like Climate, with the current mode also shown
inside the dial. Its active arc uses solid dark blue while on and gray while off. Its default state is included in the definition below.

The preset modes are `normal` and `eco`. Power uses base-topic `ON`/`OFF`, target
humidity uses `_humidity`, and mode uses `_mode`. Publish a complete value from
code when reporting measured humidity. To change the humidity range or mode
list, replace the appropriate `controls` entries and matching `ha` properties;
top-level climate `modes`/`fan_modes` do not configure this descriptor.

## Read and update values from C++

These examples use the field ID from the JSON above and an existing `ArdPortal portal`. Run them after page registration has finished and `portal.portalAndAppConfigReady()` is true. Call `portal.loop()` frequently. Register callbacks once in `setup()`; do not repeat setters on every loop iteration.

### Set the value with automatic ArdFS persistence

Set **`"persist": true`** on this field in its page JSON. It is optional for editable fields whose preset already persists settings, but required to override a RAM-only preset such as a sensor. Calling `setAppConfigValue()` sets the control value and ArdPortal automatically saves AppConfig through its integrated ArdFS storage. Do not create an ArdFS object or call a separate write/flush method. The automatic save is coalesced and advanced by subsequent `portal.loop()` calls.

```cpp
// Use this path when the value is a setting that should survive restart.
ArdJSON::JSONVar next = ArdJSON::JSON.parse(R"JSON({"state":"OFF","humidity":50,"current_humidity":null,"mode":"normal"})JSON");
if (!next.isValid() || !portal.setAppConfigValue("humidifier", next)) {
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
ArdJSON::JSONVar next = ArdJSON::JSON.parse(R"JSON({"state":"OFF","humidity":50,"current_humidity":null,"mode":"normal"})JSON");
if (!next.isValid() || !portal.setAppConfigStateValue("humidifier", next, false)) {
  Serial.println("Runtime value rejected");
}
```

The third argument `false` avoids queuing an immediate MQTT publication. Use `true` or omit it to queue changed state. RAM updates work regardless of `persist`. Alternatively, with **`"persist": false`**, `setAppConfigValue()` also follows the RAM-only path.

### Read the current value

```cpp
const ArdJSON::JSONVar current = portal.getAppConfigValue("humidifier");
if (!current.isUndefined() && current.isValid()) {
  Serial.println(ArdJSON::JSON.stringify(current));
}
```

The getter returns the effective field value: RAM override first, then saved AppConfig, then the definition default. A RAM-only update is lost on restart; the saved value/default becomes effective again. **Choose the persistent or RAM path for a given value.** A RAM override can mask a later persistent change for the same ID; there is no API to clear just that override. Prefer separate setting and telemetry IDs when both are needed.

[Back to control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls)
