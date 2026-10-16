# siren control

[Control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls) · [Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

![Portal siren control](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/control-siren.jpg)

**Minimal field JSON:**

```json
{
  "id": "siren",
  "type": "siren",
  "name": "Siren"
}
```

**Full field JSON:**

```json
{
  "id": "siren",
  "type": "siren",
  "name": "Siren",
  "default": {
    "state": "OFF",
    "tone": "alarm",
    "duration": 10,
    "volume_level": 0.5
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
      "key": "tone",
      "type": "select",
      "label": "ui_130",
      "command": "",
      "options": [
        "alarm",
        "bell"
      ]
    },
    {
      "key": "duration",
      "type": "slider",
      "label": "ui_131",
      "command": "",
      "min": 1,
      "max": 300,
      "step": 1
    },
    {
      "key": "volume_level",
      "type": "slider",
      "label": "ui_132",
      "command": "",
      "min": 0,
      "max": 1,
      "step": 0.1
    }
  ],
  "ha": {
    "command_topic": "$command",
    "state_topic": "$state",
    "available_tones": [
      "alarm",
      "bell"
    ],
    "support_duration": true,
    "support_volume_set": true,
    "support_turn_on": true,
    "support_turn_off": true,
    "support_tones": true
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


The control provides power, tone, duration and volume controls. Its default state is included in the definition below.

Tones are `alarm`/`bell`; duration is 1–300 seconds in steps of 1; volume is 0–1
in steps of 0.1. Commands use JSON on the base topic, for example
`{"state":"ON","tone":"bell","duration":30,"volume_level":0.4}`. Partial JSON
commands preserve omitted properties. The library does not start a timer or
stop a physical siren after `duration`; implement that behavior in the application.

## Integration with Home Assistant

**MQTT component:** `siren`. Requires the control’s feature flag, dynamic pages, MQTT and HA support to be enabled. ArdPortal registers MQTT Discovery and routes state/commands; the sketch implements device behavior. Action callbacks acknowledge acceptance, not physical completion.

**Supported by ArdPortal:** JSON partial commands for on/off, tone (`alarm`/`bell`), duration (1–300 seconds) and volume (0–1, step 0.1). The preset advertises those capabilities and publishes the application state object.

**Not provided by the built-in integration / limits:** No waveform generation, amplifier driver, automatic timeout or hardware confirmation. The application must honor the requested duration and report the final OFF state; the library does not run a siren timer.

Explicit `ha` properties can override Discovery metadata. They do not create additional hardware behavior, application value properties or command handlers by themselves. See [HA integration](https://github.com/rkubera/ArdPortal/wiki/Home-Assistant) for the shared setup.

## Integration with Zigbee

**Native mapping:** IAS Warning Device (`0x0502`). Requires ESP32-C5/C6/H2, `ARDPORTAL_ENABLE_ZIGBEE=1`, enabled dynamic pages and the control’s feature flag, with the matching End Device/Router board mode. MQTT/HA is not required. Register the initial pages before endpoint startup; later endpoint changes require restart. `persist` retains its AppConfig meaning.

**Supported by ArdPortal:** Incoming Start Warning commands become on/off, duration and a four-level volume request. All nonzero warning modes map to the application preset’s `alarm` tone.

**Not provided / limits:** The `bell` tone and distinct Zigbee warning modes are not preserved. Volume is quantized and duration bounded to 1–300 seconds. Strobe/squawk controls are not mapped. There is no Zigbee report of the application siren state/tone/duration/volume; the current mapping is command-oriented. The application runs the duration timer.

`visibleWhen` does not dynamically add/remove Zigbee endpoints: they remain registered, and Zigbee commands remain accepted regardless of field visibility. Gateway presentation depends on its support for the mapped clusters. ArdPortal can export a [Zigbee2MQTT converter or ZHA quirk](https://github.com/rkubera/ArdPortal/wiki/Zigbee#zigbee2mqtt-converter-download); see the [validation scope](https://github.com/rkubera/ArdPortal/wiki/Zigbee#validation-and-troubleshooting).

See [Zigbee integration](https://github.com/rkubera/ArdPortal/wiki/Zigbee) for pairing, endpoint lifecycle, gateway limitations and OTA requirements.

## Read and update values from C++

These examples use the field ID from the JSON above and an existing `ArdPortal portal`. Run them after page registration has finished and `portal.portalAndAppConfigReady()` is true. Call `portal.loop()` frequently. Register callbacks once in `setup()`; do not repeat setters on every loop iteration.

### Set the value with automatic ArdFS persistence

Set **`"persist": true`** on this field in its page JSON. It is optional for editable fields whose preset already persists settings, but required to override a RAM-only preset such as a sensor. Calling `setAppConfigValue()` sets the control value and ArdPortal automatically saves AppConfig through its integrated ArdFS storage. Do not create an ArdFS object or call a separate write/flush method. The automatic save is coalesced and advanced by subsequent `portal.loop()` calls.

```cpp
// Use this path when the value is a setting that should survive restart.
ArdJSON::JSONVar next = ArdJSON::JSON.parse(R"JSON({"state":"OFF","tone":"alarm","duration":10,"volume_level":0.5})JSON");
if (!next.isValid() || !portal.setAppConfigValue("siren", next)) {
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
ArdJSON::JSONVar next = ArdJSON::JSON.parse(R"JSON({"state":"OFF","tone":"alarm","duration":10,"volume_level":0.5})JSON");
if (!next.isValid() || !portal.setAppConfigStateValue("siren", next, false)) {
  Serial.println("Runtime value rejected");
}
```

The third argument `false` avoids queuing an immediate MQTT publication. Use `true` or omit it to queue changed state. RAM updates work regardless of `persist`. Alternatively, with **`"persist": false`**, `setAppConfigValue()` also follows the RAM-only path.

### Read the current value

```cpp
const ArdJSON::JSONVar current = portal.getAppConfigValue("siren");
if (!current.isUndefined() && current.isValid()) {
  Serial.println(ArdJSON::JSON.stringify(current));
}
```

The getter returns the effective field value: RAM override first, then saved AppConfig, then the definition default. A RAM-only update is lost on restart; the saved value/default becomes effective again. **Choose the persistent or RAM path for a given value.** A RAM override can mask a later persistent change for the same ID; there is no API to clear just that override. Prefer separate setting and telemetry IDs when both are needed.

[Back to control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls)
