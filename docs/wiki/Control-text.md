# text control

[Control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls) · [Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

![Portal text control](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/control-text.jpg)

**Minimal field JSON:**

```json
{
  "id": "room_temperature",
  "type": "text",
  "name": "Room temperature",
  "textType": "temperature"
}
```

**Full field JSON:**

```json
{
  "id": "room_temperature",
  "type": "text",
  "name": "Room temperature",
  "textType": "temperature",
  "default": 20.5,
  "persist": false
}
```

**Properties in the full example:**

| Property | Required? | Meaning / default |
| --- | --- | --- |
| `id` | Required | Stable application key; unique across registered pages. |
| `type` | Required | Enabled field type. |
| `name` | Required title¹ | Display title; may be replaced by a nonempty `names` object. |
| `textType` | **Optional** | Semantic subtype; defaults to `"text"`. See the table below. |
| `default` | **Optional** | Plain text starts as `""`; numeric subtypes start as `null` (unknown). |
| `persist` | **Optional** | Defaults to false for this read-only control. |
| `unit_of_measurement` | **Optional** | Optional unit for the measurement. |
| `device_class` | **Optional** | Optional HA measurement classification. |
| `state_class` | **Optional** | Optional HA statistical classification. |

¹ `name` can be replaced by `names`. Other shared optional properties are listed [in the catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls#required-and-optional-properties).


**Appearance:** Read-only value

**Value:** Plain text accepts a string (up to 512 bytes) or a number. Measurement subtypes accept JSON numbers or `null`, never numeric strings.

Update with a code setter; portal/MQTT commands cannot edit it. String values such as `"Ready"` use the same type.

## Text subtypes and automatic patterns

Omitting `textType`, or specifying `"textType": "text"`, creates ordinary read-only
text. It appears in the portal and as an MQTT sensor in HA, but **does not create a
Zigbee endpoint**, even if its value happens to be a number. The subtype is explicit;
`device_class` alone does not select a Zigbee measurement.

```json
{"id":"message","type":"text","name":"Message","default":"Ready"}
```

All other supported subtypes create a native Zigbee measurement when Zigbee is
enabled. MQTT/HA support works independently of Zigbee. HA Discovery automatically
applies the subtype pattern: unit, device class, `state_class: "measurement"`, and a
numeric value template that represents `null` as an unknown reading. You do not need
to duplicate these properties in each control JSON.

| `textType` | Application unit | HA device class | Zigbee cluster / encoding |
| --- | --- | --- | --- |
| `text` (default) | None | None | No endpoint |
| `number` | User-defined or none | None | Analog Input, float32 |
| `temperature` | °C | `temperature` | Temperature Measurement, signed hundredths |
| `humidity` | % | `humidity` | Relative Humidity Measurement, hundredths |
| `pressure` | hPa | `pressure` | Pressure Measurement, rounded integer hPa |
| `illuminance` | lx | `illuminance` | Illuminance Measurement, standard logarithmic encoding; zero means darkness |
| `flow` | m³/h | `volume_flow_rate` | Flow Measurement, tenths |
| `carbon_dioxide` | ppm | `carbon_dioxide` | CO₂ Measurement, float fraction (ppm / 1,000,000) |
| `pm25` | µg/m³ | `pm25` | PM2.5 Measurement, float32 |
| `wind_speed` | m/s | `wind_speed` | Wind Speed Measurement, hundredths |
| `voltage` | V (AC RMS) | `voltage` | Electrical Measurement, tenths |
| `current` | A (AC RMS) | `current` | Electrical Measurement, thousandths |
| `power` | W (AC active power) | `power` | Electrical Measurement, signed integer |

Names are case-sensitive. Unknown subtype names fail page registration with
`stage=field`, the field ID, and `reason=unsupported textType`. Values always use
the application units listed above. Do not label Celsius as Fahrenheit without
converting the actual value. Explicit top-level HA metadata and properties in `ha`
can override automatic defaults; a custom unit requires a matching conversion
in `ha.value_template` for MQTT/HA. Zigbee always uses the units and encoding above.

Use `null` when a measurement is unavailable. Out-of-range wire readings become
Zigbee's unknown sentinel instead of overflowing: humidity 0–100%, temperature
−273.15–327.67 °C, pressure/power −32767–32767, flow 0–6553.4 m³/h, wind
0–655.34 m/s, voltage 0–6553.4 V, and current 0–65.534 A. Negative light,
CO₂ and PM2.5 readings are unknown. Generic `number` signals an Analog Input fault
when no finite reading is available. A standard cluster does not guarantee that
every gateway automatically creates a UI; see [Zigbee](https://github.com/rkubera/ArdPortal/wiki/Zigbee).

The automatic HA classifications follow [Home Assistant sensor device classes](https://www.home-assistant.io/integrations/sensor/).

## Integration with Home Assistant

**MQTT component:** `sensor`. Requires the control’s feature flag, dynamic pages, MQTT and HA support to be enabled. ArdPortal registers MQTT Discovery and routes state/commands; the sketch implements device behavior. Action callbacks acknowledge acceptance, not physical completion.

**Supported by ArdPortal:** Read-only strings or numbers. `textType` defaults to `text`; numeric subtypes automatically add unit/device-class/state-class patterns and a numeric template handling unknown readings. See the subtype table above for exact units and mappings. Explicit `ha` metadata can override discovery defaults.

**Not provided by the built-in integration / limits:** No incoming edit command, sensor driver or automatic conversion of the application value to another unit. Numeric subtypes require a JSON number or null, not a numeric string. Plain text has no automatic numeric statistics pattern. A custom HA template only affects MQTT/HA presentation, not Zigbee encoding.

Explicit `ha` properties can override Discovery metadata. They do not create additional hardware behavior, application value properties or command handlers by themselves. See [HA integration](https://github.com/rkubera/ArdPortal/wiki/Home-Assistant) for the shared setup.

## Integration with Zigbee

**Status:** Plain `text` (omitted `textType`, or `textType: "text"`) creates **no Zigbee endpoint**, including when the displayed value is numeric. Supported non-text subtypes create read-only native measurements.

Requires ESP32-C5/C6, `ARDPORTAL_ENABLE_ZIGBEE=1`, enabled dynamic pages and the control’s feature flag, with the matching End Device/Router board mode. MQTT/HA is not required. Register the initial pages before endpoint startup; later endpoint changes require restart. `persist` retains its AppConfig meaning.

**Supported by ArdPortal:** `number`, `temperature`, `humidity`, `pressure`, `illuminance`, `flow`, `carbon_dioxide`, `pm25`, `wind_speed`, `voltage`, `current` and `power`. The subtype table above lists the exact cluster, application unit and wire encoding. Unknown readings use null and the corresponding Zigbee invalid value (or Analog Input fault flag).

**Limits:** No editable readings, unit selection/conversion, calibration logic or physical sensor driver. Pressure/power round to integers, other integer clusters quantize to their scales, and float clusters use float32. Out-of-range readings become unknown; illuminance uses a logarithmic encoding. Voltage/current are AC RMS, and power is AC active power; DC/reactive/apparent/energy readings are not implemented. Generic Analog Input may require a converter. HA metadata never changes the Zigbee subtype or its units.

`visibleWhen` does not dynamically add/remove Zigbee endpoints: they remain registered, and Zigbee commands remain accepted regardless of field visibility. Gateway UI support may require a converter; physical coordinator interoperability remains to be validated.

See [Zigbee integration](https://github.com/rkubera/ArdPortal/wiki/Zigbee) for pairing, endpoint lifecycle, gateway limitations and OTA requirements.

## Read and update values from C++

These examples use the field ID from the JSON above and an existing `ArdPortal portal`. Run them after page registration has finished and `portal.portalAndAppConfigReady()` is true. Call `portal.loop()` frequently. Register callbacks once in `setup()`; do not repeat setters on every loop iteration.

### Set the value with automatic ArdFS persistence

Set **`"persist": true`** on this field in its page JSON. It is optional for editable fields whose preset already persists settings, but required to override a RAM-only preset such as a sensor. Calling `setAppConfigValue()` sets the control value and ArdPortal automatically saves AppConfig through its integrated ArdFS storage. Do not create an ArdFS object or call a separate write/flush method. The automatic save is coalesced and advanced by subsequent `portal.loop()` calls.

```cpp
// Use this path when the value is a setting that should survive restart.
ArdJSON::JSONVar next = 20.5;
if (!next.isValid() || !portal.setAppConfigValue("room_temperature", next)) {
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
ArdJSON::JSONVar next = 20.5;
if (!next.isValid() || !portal.setAppConfigStateValue("room_temperature", next, false)) {
  Serial.println("Runtime value rejected");
}
```

The third argument `false` avoids queuing an immediate MQTT publication. Use `true` or omit it to queue changed state. RAM updates work regardless of `persist`. Alternatively, with **`"persist": false`**, `setAppConfigValue()` also follows the RAM-only path.

### Read the current value

```cpp
const ArdJSON::JSONVar current = portal.getAppConfigValue("room_temperature");
if (!current.isUndefined() && current.isValid()) {
  Serial.println(ArdJSON::JSON.stringify(current));
}
```

The getter returns the effective field value: RAM override first, then saved AppConfig, then the definition default. A RAM-only update is lost on restart; the saved value/default becomes effective again. **Choose the persistent or RAM path for a given value.** A RAM override can mask a later persistent change for the same ID; there is no API to clear just that override. Prefer separate setting and telemetry IDs when both are needed.

[Back to control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls)

Numeric `textType` support can be compiled out with
`ARDPORTAL_ENABLE_TEXT_TYPES=0` (default: `1`); plain text remains available.
See [Defines](Defines#typed-text-measurements). Unsupported subtype definitions
are rejected rather than silently converted to plain text.
