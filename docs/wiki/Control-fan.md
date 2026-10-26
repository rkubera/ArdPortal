# fan control

[Control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls) · [Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

![Portal fan control](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/control-fan.jpg)

`class` is always optional. See [the shared class catalogue](Control-Classes) for valid values, defaults and protocol mapping limits.

**Minimal field JSON:**

```json
{
  "id": "fan",
  "type": "fan",
  "name": "Fan"
}
```

**Full field JSON:**

```json
{
  "id": "fan",
  "type": "fan",
  "name": "Fan",
  "default": {
    "state": "OFF",
    "percentage": 0,
    "oscillation": "oscillate_off",
    "direction": "forward"
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
      "key": "percentage",
      "type": "slider",
      "label": "ui_114",
      "command": "percentage",
      "min": 0,
      "max": 100,
      "step": 1
    },
    {
      "key": "oscillation",
      "type": "switch",
      "label": "ui_115",
      "command": "oscillation",
      "on": "oscillate_on",
      "off": "oscillate_off"
    },
    {
      "key": "direction",
      "type": "select",
      "label": "ui_116",
      "command": "direction",
      "options": [
        "forward",
        "reverse"
      ]
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


The control displays power, speed (0–100%), oscillation and direction. Its default state is included in the definition below.

Oscillation accepts `oscillate_on`/`oscillate_off`; direction accepts
`forward`/`reverse`. Power uses the base command topic with `ON`/`OFF`; numeric
speed uses `_percentage`, oscillation `_oscillation`, and direction `_direction`.
Changing speed does not automatically turn on the fan.

## Integration with Home Assistant

**MQTT component:** `fan`. Requires the control’s feature flag, dynamic pages, MQTT and HA support to be enabled. ArdPortal registers MQTT Discovery and routes state/commands; the sketch implements device behavior. Action callbacks acknowledge acceptance, not physical completion.

**Supported by ArdPortal:** On/off, speed percentage 0–100, oscillation on/off and forward/reverse direction. The preset provides separate percentage/oscillation/direction topics and JSON state templates.

**Not provided by the built-in integration / limits:** No built-in fan presets, timed operation, RPM measurement, acceleration curve or motor driver. The application applies the requested settings and reports actual state.

Explicit `ha` properties can override Discovery metadata. They do not create additional hardware behavior, application value properties or command handlers by themselves. See [HA integration](https://github.com/rkubera/ArdPortal/wiki/Home-Assistant) for the shared setup.

## Integration with Zigbee

**Implemented as a composite mapping.** The functions below are separate standard endpoints on the same physical Zigbee device. The optional generated Zigbee2MQTT converter and ZHA quirk add names and option/state semantics; a composite mapping does not guarantee one combined gateway entity. Requires ESP32-C5/C6/H2, Zigbee and dynamic pages enabled, and the field’s control flag enabled. MQTT/HA is not required.

**Standard mapping:** On/Off + Analog Output + On/Off + Multistate Output.

**Supported by ArdPortal:** Power, exact requested percentage (0–100), oscillation and forward/reverse direction are separate endpoints. Percentage is float32; direction is option 1=forward, 2=reverse.

**Not provided / limits:** No native Fan Control endpoint or unified fan UI is advertised. A gateway may display separate switches/numeric controls, may show direction as an ordinal, or may omit generic Analog/Multistate functions.

Endpoint IDs follow registration order, then the preset’s `controls` order; read-only measurement/status endpoints follow the editable/action endpoints. Composite fields consume multiple slots in the shared 239-endpoint budget. Startup stages one endpoint per loop and reports one endpoint per loop. Values and action callbacks use the existing AppConfig routes and `persist`; accepting an action is not hardware completion.

Standard Basic ProductLabel contains `field name / property or action`, truncated to 63 UTF-8 bytes; Analog/Multistate endpoints also carry their Description. A gateway is not required to use these labels or expose all these clusters. Option/state names are documented here but are not currently sent as StateText. Unknown discrete readings set the standard status fault flag and leave the last representable ordinal unchanged.

`visibleWhen` does not remove endpoints or create a dynamic Zigbee form. Dependency rules do not affect Zigbee control visibility or command acceptance. Native endpoint definitions freeze at startup; changes need restart. Physical coordinator interoperability is still unverified.

See [Zigbee integration](https://github.com/rkubera/ArdPortal/wiki/Zigbee) and [page/order limitations](https://github.com/rkubera/ArdPortal/wiki/Zigbee#portal-pages-and-control-order).

## Integration with Matter

**Native mapping:** Fan (Fan Control cluster `0x0202`).

**Definition requirements:** The definition must expose both power and percentage/speed capabilities. The built-in preset supplies them. Explicit `readonly: true` is unsupported.

**Supported by ArdPortal:** Reports `state` and `percentage`; accepts speed and supported fan mode/power commands, updating the shared application value.

**Not provided / limits:** Direction and oscillation remain available to the portal/other supported integrations but are not transported by this Matter adapter. It does not create a motor driver or arbitrary fan-mode mapping.

**Setup:** Requires a supported ESP32 target, `ARDPORTAL_ENABLE_MATTER=1`, dynamic pages and the control’s feature flag. Select Matter Wi-Fi or Thread at compilation; MQTT/HA Discovery is not required. Register the initial pages before Matter endpoint startup. Native Zigbee and Matter are alternative firmware integrations.

The sketch supplies hardware behavior and updates the shared application value; Matter does not drive GPIOs or sensors. Call `portal.loop()` frequently. Incoming writable commands use normal validation/persistence and `ChangeSource::Matter`; acceptance does not confirm physical completion. `visibleWhen` only affects the portal UI and does not remove a Matter endpoint or disable its commands. Changes to the native endpoint layout may require a Matter reset and commissioning again; see [Matter integration](Matter#mappings).

## Read and update values from C++

These examples use the field ID from the JSON above and an existing `ArdPortal portal`. Run them after page registration has finished and `portal.portalAndAppConfigReady()` is true. Call `portal.loop()` frequently. Register callbacks once in `setup()`; do not repeat setters on every loop iteration.

### Set the value with automatic ArdFS persistence

Set **`"persist": true`** on this field in its page JSON. It is optional for editable fields whose preset already persists settings, but required to override a RAM-only preset such as a sensor. Calling `setAppConfigValue()` sets the control value and ArdPortal automatically saves AppConfig through its integrated ArdFS storage. Do not create an ArdFS object or call a separate write/flush method. The automatic save is coalesced and advanced by subsequent `portal.loop()` calls.

```cpp
// Use this path when the value is a setting that should survive restart.
ArdJSON::JSONVar next = ArdJSON::JSON.parse(R"JSON({"state":"OFF","percentage":0,"oscillation":"oscillate_off","direction":"forward"})JSON");
if (!next.isValid() || !portal.setAppConfigValue("fan", next)) {
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
ArdJSON::JSONVar next = ArdJSON::JSON.parse(R"JSON({"state":"OFF","percentage":0,"oscillation":"oscillate_off","direction":"forward"})JSON");
if (!next.isValid() || !portal.setAppConfigStateValue("fan", next, false)) {
  Serial.println("Runtime value rejected");
}
```

The third argument `false` avoids queuing an immediate MQTT publication. Use `true` or omit it to queue changed state. RAM updates work regardless of `persist`. Alternatively, with **`"persist": false`**, `setAppConfigValue()` also follows the RAM-only path.

### Read the current value

```cpp
const ArdJSON::JSONVar current = portal.getAppConfigValue("fan");
if (!current.isUndefined() && current.isValid()) {
  Serial.println(ArdJSON::JSON.stringify(current));
}
```

The getter returns the effective field value: RAM override first, then saved AppConfig, then the definition default. A RAM-only update is lost on restart; the saved value/default becomes effective again. **Choose the persistent or RAM path for a given value.** A RAM override can mask a later persistent change for the same ID; there is no API to clear just that override. Prefer separate setting and telemetry IDs when both are needed.

[Back to control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls)
