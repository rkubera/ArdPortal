# climate control

[Control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls) · [Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

![Portal climate control](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/control-climate.jpg)

`class` is always optional. See [the shared class catalogue](Control-Classes) for valid values, defaults and protocol mapping limits.

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

## Integration with Home Assistant

**MQTT component:** `climate`. Requires the control’s feature flag, dynamic pages, MQTT and HA support to be enabled. ArdPortal registers MQTT Discovery and routes state/commands; the sketch implements device behavior. Action callbacks acknowledge acceptance, not physical completion.

**Supported by ArdPortal:** Single target or independent heating/cooling targets in `heat_cool`, current Celsius temperature, configured `min`/`max`/`step`, optional mode selection and optional fan-mode selection. Commands use `_temperature`, `_mode` and `_fan`. Supported selectable modes are `off`, `heat`, `cool`, `auto`, `heat_cool`, `dry`, `fan_only`; fan names come from the field definition.

**Not provided by the built-in integration / limits:** The automatic discovery does not emit `action_topic`/`action_template`: storing `action` in AppConfig alone does not expose HVAC activity to HA. You can explicitly add `ha.action_topic: "$state"` and `ha.action_template: "{{ value_json.action }}"` and report HA-supported action names. There are no built-in presets, target humidity, swing control, emergency heating, precooling or sleep modes. Arbitrary extra properties in the climate value are rejected; `ha` metadata alone cannot add those state/command routes.

Explicit `ha` properties can override Discovery metadata. They do not create additional hardware behavior, application value properties or command handlers by themselves. See [HA integration](https://github.com/rkubera/ArdPortal/wiki/Home-Assistant) for the shared setup.

## Integration with Zigbee

**Native mapping:** Thermostat (`0x0201`), optional Fan Control (`0x0202`). Requires ESP32-C5/C6/H2, `ARDPORTAL_ENABLE_ZIGBEE=1`, enabled dynamic pages and the control’s feature flag, with the matching End Device/Router board mode. MQTT/HA is not required. Register the initial pages before endpoint startup; later endpoint changes require restart. `persist` retains its AppConfig meaning.

**Supported by ArdPortal:** Current and target Celsius temperature, supported standard system modes, fan modes and heating/cooling relay bits. For a thermostat supporting both heat and cool, the active setpoint follows the application target. In `cool`, the inactive heating setpoint is parked 2.5 °C below `min`; in `heat`, `off` and `auto`, the inactive cooling setpoint is parked 2.5 °C above `max`. This preserves the native thermostat deadband while allowing the active target to move across its entire configured range. Parking the inactive setpoint next to the current target would cause the SDK to reject larger remote changes before ArdPortal could process them. Outside `heat_cool`, a write to the inactive setpoint is converted back using the same offset. In `heat_cool`, `temperature_low` and `temperature_high` supply independent heating/cooling targets, and Zigbee Auto maps to `heat_cool`. Commands for setpoints, system mode and fan mode route to AppConfig.

**Not provided / limits:** No presets, schedules, occupancy setpoints, emergency heating, precooling or sleep selection. Fan modes translate only standard names: off/low/medium/high/on/auto/smart. RunningState encodes heating or cooling only: fan and additional-stage bits/combinations are absent. idle/off/unknown and other actions become zero relay bits; these are not equivalent application states. A distinct RunningMode report is not supplied.

`visibleWhen` does not dynamically add/remove Zigbee endpoints: they remain registered, and Zigbee commands remain accepted regardless of field visibility. Gateway presentation depends on its support for the mapped clusters. ArdPortal can export a [Zigbee2MQTT converter or ZHA quirk](https://github.com/rkubera/ArdPortal/wiki/Zigbee#zigbee2mqtt-converter-download); see the [validation scope](https://github.com/rkubera/ArdPortal/wiki/Zigbee#troubleshooting).

See [Zigbee integration](https://github.com/rkubera/ArdPortal/wiki/Zigbee) for pairing, endpoint lifecycle, gateway limitations and OTA requirements.

## Integration with Matter

**Native mapping:** Thermostat (Thermostat cluster `0x0201`).

**Definition requirements:** Declare heating and/or cooling in `modes`; only `off`, `heat`, `cool` and `heat_cool` are supported. At least heating or cooling is required. When both are declared, include `heat_cool`. The whole field range must fit heat 7–30 °C, cool 16–32 °C, or their intersection 16–30 °C for dual capability. Explicit `readonly: true` is unsupported.

**Supported by ArdPortal:** Reports `mode`, `current_temperature` and setpoints in Celsius. Single-capability presets use `temperature`; definitions with `heat_cool` use `temperature_low`/`temperature_high`. Accepts supported mode and setpoint commands through the shared model. Unknown current temperature is reported as Matter null.

**Not provided / limits:** Dual heating/cooling setpoints must retain at least a 2.5 °C deadband, including while off. Thermostat fan modes and action labels are not native attributes in this adapter. No heating/cooling hardware or control algorithm is supplied.

**Setup:** Requires a supported ESP32 target, `ARDPORTAL_ENABLE_MATTER=1`, dynamic pages and the control’s feature flag. Select Matter Wi-Fi or Thread at compilation; MQTT/HA Discovery is not required. Register the initial pages before Matter endpoint startup. Native Zigbee and Matter are alternative firmware integrations.

The sketch supplies hardware behavior and updates the shared application value; Matter does not drive GPIOs or sensors. Call `portal.loop()` frequently. Incoming writable commands use normal validation/persistence and `ChangeSource::Matter`; acceptance does not confirm physical completion. `visibleWhen` only affects the portal UI and does not remove a Matter endpoint or disable its commands. Changes to the native endpoint layout may require a Matter reset and commissioning again; see [Matter integration](Matter#mappings).

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

## Heating and cooling range

Add `"heat_cool"` to `modes` to enable two independent setpoints. In this mode the Portal shows one ring with independent orange heating and blue cooling handles, with both targets in its center. Select a handle or its temperature to choose which target the plus/minus buttons change. `cool` alone still uses a single target.

```json
{"id":"room","type":"climate","name":"Room","min":10,"max":35,"step":0.5,"modes":["off","heat","cool","heat_cool"],"default":{"temperature":22,"temperature_low":20,"temperature_high":25,"current_temperature":null,"mode":"heat_cool"}}
```

`temperature_low` and `temperature_high` are optional outside `heat_cool` and required in `heat_cool`. They must respect min/max/step and low must not exceed high. HA Discovery exposes both MQTT command/state topics (`<id>_temperature_low`, `<id>_temperature_high`). Zigbee maps them to Occupied Heating Setpoint and Occupied Cooling Setpoint. With Zigbee enabled, validation requires a minimum 2.5 °C separation in `heat_cool`, matching the native thermostat deadband. Zigbee2MQTT calls the combined mode `auto`; the generated converter exposes both setpoints.
