# light control

[Control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls) · [Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

![Portal light control](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/control-light.jpg)

**Minimal field JSON:**

```json
{
  "id": "backlight",
  "type": "light",
  "name": "Backlight"
}
```

**Full field JSON:**

```json
{
  "id": "backlight",
  "type": "light",
  "name": "Backlight",
  "default": {
    "state": "OFF",
    "brightness": 128,
    "color": {
      "r": 255,
      "g": 255,
      "b": 255
    },
    "color_mode": "rgb"
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
      "key": "brightness",
      "type": "slider",
      "label": "ui_112",
      "command": "",
      "min": 0,
      "max": 255,
      "step": 1
    },
    {
      "key": "color",
      "type": "color",
      "label": "ui_113",
      "command": ""
    }
  ],
  "ha": {
    "schema": "json",
    "command_topic": "$command",
    "state_topic": "$state",
    "brightness": true,
    "supported_color_modes": [
      "rgb"
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


The control displays a power toggle, brightness slider (0–255, step 1) and a rounded
color swatch opening the browser color picker. Its preset uses RGB and JSON MQTT
commands. The default state is included in the definition below.

The color swatch has no visible label by default. Set optional `colorLabel` on
the light field to display a dedicated label above it. Optional `colorLabels`
provides translations using the same language keys as `names`. These properties
affect the portal only; they do not rename the HA entity.

```json
{"id":"backlight","type":"light","name":"Backlight","colorLabel":"Color","colorLabels":{"en":"Color","pl":"Kolor"}}
```

`state` is `ON` or `OFF`; each RGB channel is an integer from 0 to 255. HA commands
can contain only changed properties, for example `{"state":"ON","brightness":200}`
or `{"color":{"r":255,"g":80,"b":0}}`, on the base command topic. The library
merges supported properties into the existing state. The preset does not provide
color-temperature or effect controls; these require matching custom discovery,
controls and application behavior.

Available since ArdPortal 0.7.8.

## Read and update values from C++

These examples use the field ID from the JSON above and an existing `ArdPortal portal`. Run them after page registration has finished and `portal.portalAndAppConfigReady()` is true. Call `portal.loop()` frequently. Register callbacks once in `setup()`; do not repeat setters on every loop iteration.

### Set the value with automatic ArdFS persistence

Set **`"persist": true`** on this field in its page JSON. It is optional for editable fields whose preset already persists settings, but required to override a RAM-only preset such as a sensor. Calling `setAppConfigValue()` sets the control value and ArdPortal automatically saves AppConfig through its integrated ArdFS storage. Do not create an ArdFS object or call a separate write/flush method. The automatic save is coalesced and advanced by subsequent `portal.loop()` calls.

```cpp
// Use this path when the value is a setting that should survive restart.
ArdJSON::JSONVar next = ArdJSON::JSON.parse(R"JSON({"state":"OFF","brightness":128,"color":{"r":255,"g":255,"b":255},"color_mode":"rgb"})JSON");
if (!next.isValid() || !portal.setAppConfigValue("backlight", next)) {
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
ArdJSON::JSONVar next = ArdJSON::JSON.parse(R"JSON({"state":"OFF","brightness":128,"color":{"r":255,"g":255,"b":255},"color_mode":"rgb"})JSON");
if (!next.isValid() || !portal.setAppConfigStateValue("backlight", next, false)) {
  Serial.println("Runtime value rejected");
}
```

The third argument `false` avoids queuing an immediate MQTT publication. Use `true` or omit it to queue changed state. RAM updates work regardless of `persist`. Alternatively, with **`"persist": false`**, `setAppConfigValue()` also follows the RAM-only path.

### Read the current value

```cpp
const ArdJSON::JSONVar current = portal.getAppConfigValue("backlight");
if (!current.isUndefined() && current.isValid()) {
  Serial.println(ArdJSON::JSON.stringify(current));
}
```

The getter returns the effective field value: RAM override first, then saved AppConfig, then the definition default. A RAM-only update is lost on restart; the saved value/default becomes effective again. **Choose the persistent or RAM path for a given value.** A RAM override can mask a later persistent change for the same ID; there is no API to clear just that override. Prefer separate setting and telemetry IDs when both are needed.

[Back to control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls)
