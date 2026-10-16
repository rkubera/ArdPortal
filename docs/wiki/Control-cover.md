# cover control

[Control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls) · [Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

![Portal cover control](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/control-cover.jpg)

**Minimal field JSON:**

```json
{
  "id": "cover",
  "type": "cover",
  "name": "Cover"
}
```

**Full field JSON:**

```json
{
  "id": "cover",
  "type": "cover",
  "name": "Cover",
  "default": {
    "state": "closed",
    "position": 0,
    "tilt": 0
  },
  "controls": [
    {
      "key": "",
      "type": "action",
      "label": "ui_117",
      "command": "",
      "payload": "OPEN"
    },
    {
      "key": "",
      "type": "action",
      "label": "ui_118",
      "command": "",
      "payload": "CLOSE"
    },
    {
      "key": "",
      "type": "action",
      "label": "ui_119",
      "command": "",
      "payload": "STOP"
    },
    {
      "key": "position",
      "type": "slider",
      "label": "ui_120",
      "command": "position",
      "min": 0,
      "max": 100,
      "step": 1
    },
    {
      "key": "tilt",
      "type": "slider",
      "label": "ui_121",
      "command": "tilt",
      "min": 0,
      "max": 100,
      "step": 1
    }
  ],
  "ha": {
    "command_topic": "$command",
    "state_topic": "$state",
    "value_template": "{{ value_json.state }}",
    "position_topic": "$state",
    "position_template": "{{ value_json.position }}",
    "set_position_topic": "$command:position",
    "tilt_command_topic": "$command:tilt",
    "tilt_status_topic": "$state",
    "tilt_status_template": "{{ value_json.tilt }}"
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


The control provides Open, Close and Stop buttons plus position and tilt sliders,
both 0–100 in steps of 1. Its default state is
`{"state":"closed","position":0,"tilt":0}`. Action payloads are `OPEN`, `CLOSE`
and `STOP` on the base command topic; numeric position and tilt use `_position`
and `_tilt`. Report `open`, `opening`, `closed`, `closing` or `stopped` to highlight
the matching button. Action acceptance alone does not change reported position.

## Integration with Home Assistant

**MQTT component:** `cover`. Requires the control’s feature flag, dynamic pages, MQTT and HA support to be enabled. ArdPortal registers MQTT Discovery and routes state/commands; the sketch implements device behavior. Action callbacks acknowledge acceptance, not physical completion.

**Supported by ArdPortal:** OPEN/CLOSE/STOP actions plus position and tilt commands, both 0–100. JSON state templates expose `state`, `position` and `tilt`; the application reports open/opening/closed/closing/stopped as appropriate.

**Not provided by the built-in integration / limits:** No motor driver, end-stop handling, position calibration, obstacle detection or automatic movement-state tracking. Accepting an action does not update the physical position; report it separately.

Explicit `ha` properties can override Discovery metadata. They do not create additional hardware behavior, application value properties or command handlers by themselves. See [HA integration](https://github.com/rkubera/ArdPortal/wiki/Home-Assistant) for the shared setup.

## Integration with Zigbee

**Native mapping:** Window Covering (`0x0102`). Requires ESP32-C5/C6/H2, `ARDPORTAL_ENABLE_ZIGBEE=1`, enabled dynamic pages and the control’s feature flag, with the matching End Device/Router board mode. MQTT/HA is not required. Register the initial pages before endpoint startup; later endpoint changes require restart. `persist` retains its AppConfig meaning.

**Supported by ArdPortal:** OPEN/CLOSE/STOP and lift/tilt percentage commands; reported position/tilt. ArdPortal converts 100% open to the ZCL 0% open convention.

**Not provided / limits:** No absolute encoder-position commands, calibration/end-stop configuration or separate application `state` report for opening/closing/stopped. The application drives motion and reports position.

`visibleWhen` does not dynamically add/remove Zigbee endpoints: they remain registered, and Zigbee commands remain accepted regardless of field visibility. Gateway presentation depends on its support for the mapped clusters. ArdPortal can export a [Zigbee2MQTT converter or ZHA quirk](https://github.com/rkubera/ArdPortal/wiki/Zigbee#zigbee2mqtt-converter-download); see the [validation scope](https://github.com/rkubera/ArdPortal/wiki/Zigbee#validation-and-troubleshooting).

See [Zigbee integration](https://github.com/rkubera/ArdPortal/wiki/Zigbee) for pairing, endpoint lifecycle, gateway limitations and OTA requirements.

## Read and update values from C++

These examples use the field ID from the JSON above and an existing `ArdPortal portal`. Run them after page registration has finished and `portal.portalAndAppConfigReady()` is true. Call `portal.loop()` frequently. Register callbacks once in `setup()`; do not repeat setters on every loop iteration.

### Set the value with automatic ArdFS persistence

Set **`"persist": true`** on this field in its page JSON. It is optional for editable fields whose preset already persists settings, but required to override a RAM-only preset such as a sensor. Calling `setAppConfigValue()` sets the control value and ArdPortal automatically saves AppConfig through its integrated ArdFS storage. Do not create an ArdFS object or call a separate write/flush method. The automatic save is coalesced and advanced by subsequent `portal.loop()` calls.

```cpp
// Use this path when the value is a setting that should survive restart.
ArdJSON::JSONVar next = ArdJSON::JSON.parse(R"JSON({"state":"closed","position":0,"tilt":0})JSON");
if (!next.isValid() || !portal.setAppConfigValue("cover", next)) {
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
ArdJSON::JSONVar next = ArdJSON::JSON.parse(R"JSON({"state":"closed","position":0,"tilt":0})JSON");
if (!next.isValid() || !portal.setAppConfigStateValue("cover", next, false)) {
  Serial.println("Runtime value rejected");
}
```

The third argument `false` avoids queuing an immediate MQTT publication. Use `true` or omit it to queue changed state. RAM updates work regardless of `persist`. Alternatively, with **`"persist": false`**, `setAppConfigValue()` also follows the RAM-only path.

### Read the current value

```cpp
const ArdJSON::JSONVar current = portal.getAppConfigValue("cover");
if (!current.isUndefined() && current.isValid()) {
  Serial.println(ArdJSON::JSON.stringify(current));
}
```

The getter returns the effective field value: RAM override first, then saved AppConfig, then the definition default. A RAM-only update is lost on restart; the saved value/default becomes effective again. **Choose the persistent or RAM path for a given value.** A RAM override can mask a later persistent change for the same ID; there is no API to clear just that override. Prefer separate setting and telemetry IDs when both are needed.

[Back to control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls)
