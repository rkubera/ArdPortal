# climate control

[Control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls) · [Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

![Portal climate control](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/control-climate.jpg)

**Minimal field JSON:**

```json
{
  "id": "climate",
  "type": "climate",
  "name": "Climate",
  "min": 10,
  "max": 30
}
```

**Full field JSON:**

```json
{
  "id": "climate",
  "type": "climate",
  "name": "Climate",
  "min": 10,
  "max": 30,
  "step": 0.5,
  "modes": [
    "off",
    "heat",
    "cool",
    "auto"
  ],
  "fan_modes": [
    "auto",
    "low",
    "high"
  ],
  "default": {
    "temperature": 22,
    "current_temperature": 20.5,
    "mode": "heat",
    "fan_mode": "auto",
    "action": "heating"
  }
}
```

**Properties in the full example:**

| Property | Required? | Meaning / default |
| --- | --- | --- |
| `id` | Required | Stable application key; unique across registered pages. |
| `type` | Required | Enabled field type. |
| `name` | Required title¹ | Display title; may be replaced by a nonempty `names` object. |
| `min` | Required | Lower bound; required for this field type. |
| `max` | Required | Upper bound; must be greater than `min`. |
| `step` | **Optional** | Defaults to 1 for slider or 0.5 for climate. |
| `modes` | **Optional** | Omit to hide the climate mode selector. |
| `fan_modes` | **Optional** | Omit to hide the climate fan selector. |
| `default` | **Optional** | Omit to use the built-in initial value. |

¹ `name` can be replaced by `names`. Other shared optional properties are listed [in the catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls#required-and-optional-properties).


**Appearance:** Circular temperature dial, plus/minus, optional mode buttons and fan selector

**Value:** Object with target/current temperature and optional mode/fan state

Requires `min`/`max`; `step` defaults to 0.5. `modes` and `fan_modes` are independently optional. See Climate state and commands below.


### Climate state and commands

Its range and
optional mode/fan lists are top-level definition properties. A typical value is
`{"temperature":21,"current_temperature":20,"mode":"heat","fan_mode":"auto"}`.
Omit `mode` or `fan_mode` if its corresponding list is absent. Temperature
commands use `_temperature`; mode uses `_mode`; fan mode uses `_fan`.

## Read and update values from C++

These examples use the field ID from the JSON above and an existing `ArdPortal portal`. Run them after page registration has finished and `portal.portalAndAppConfigReady()` is true. Call `portal.loop()` frequently. Register callbacks once in `setup()`; do not repeat setters on every loop iteration.

### Set the value with automatic ArdFS persistence

Set **`"persist": true`** on this field in its page JSON. It is optional for editable fields whose preset already persists settings, but required to override a RAM-only preset such as a sensor. Calling `setAppConfigValue()` sets the control value and ArdPortal automatically saves AppConfig through its integrated ArdFS storage. Do not create an ArdFS object or call a separate write/flush method. The automatic save is coalesced and advanced by subsequent `portal.loop()` calls.

```cpp
// Use this path when the value is a setting that should survive restart.
ArdJSON::JSONVar next = ArdJSON::JSON.parse(R"JSON({"temperature":22,"current_temperature":20.5,"mode":"heat","fan_mode":"auto","action":"heating"})JSON");
if (!next.isValid() || !portal.setAppConfigValue("climate", next)) {
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
ArdJSON::JSONVar next = ArdJSON::JSON.parse(R"JSON({"temperature":22,"current_temperature":20.5,"mode":"heat","fan_mode":"auto","action":"heating"})JSON");
if (!next.isValid() || !portal.setAppConfigStateValue("climate", next, false)) {
  Serial.println("Runtime value rejected");
}
```

The third argument `false` avoids queuing an immediate MQTT publication. Use `true` or omit it to queue changed state. RAM updates work regardless of `persist`. Alternatively, with **`"persist": false`**, `setAppConfigValue()` also follows the RAM-only path.

### Read the current value

```cpp
const ArdJSON::JSONVar current = portal.getAppConfigValue("climate");
if (!current.isUndefined() && current.isValid()) {
  Serial.println(ArdJSON::JSON.stringify(current));
}
```

The getter returns the effective field value: RAM override first, then saved AppConfig, then the definition default. A RAM-only update is lost on restart; the saved value/default becomes effective again. **Choose the persistent or RAM path for a given value.** A RAM override can mask a later persistent change for the same ID; there is no API to clear just that override. Prefer separate setting and telemetry IDs when both are needed.

[Back to control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls)
