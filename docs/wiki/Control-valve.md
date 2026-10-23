# valve control

[Control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls) · [Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

![Portal valve control](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/control-valve.jpg)

`class` is always optional. See [the shared class catalogue](Control-Classes) for valid values, defaults and protocol mapping limits.

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

## Integration with Home Assistant

**MQTT component:** `valve`. Requires the control’s feature flag, dynamic pages, MQTT and HA support to be enabled. ArdPortal registers MQTT Discovery and routes state/commands; the sketch implements device behavior. Action callbacks acknowledge acceptance, not physical completion.

**Supported by ArdPortal:** Position-reporting valve with `reports_position: true`. Numeric commands use the base topic; 0/100 invoke close/open action callbacks, while intermediate positions update the position setting. JSON reports carry `state` and `position`.

**Not provided by the built-in integration / limits:** No STOP action in the built-in controls, flow/pressure telemetry, actuator calibration or physical valve driver. Position/state changes must be reported by the application; the preset does not supply a timer or safety interlock.

Explicit `ha` properties can override Discovery metadata. They do not create additional hardware behavior, application value properties or command handlers by themselves. See [HA integration](https://github.com/rkubera/ArdPortal/wiki/Home-Assistant) for the shared setup.

## Integration with Zigbee

**Implemented as a composite mapping.** The functions below are separate standard endpoints on the same physical Zigbee device. The optional generated Zigbee2MQTT converter and ZHA quirk add names and option/state semantics; a composite mapping does not guarantee one combined gateway entity. Requires ESP32-C5/C6/H2, Zigbee and dynamic pages enabled, and the field’s control flag enabled. MQTT/HA is not required.

**Standard mapping:** Two momentary On/Off actions + Analog Output + Multistate Input.

**Supported by ArdPortal:** Open/close actions are activated with ON, position is writable 0–100, and discrete state is reported: 1=closed, 2=opening, 3=open, 4=closing.

**Not provided / limits:** No STOP action, native valve device profile or actuator driver. Opening/closing status and physical position come from the application. Generic numeric/status endpoints are gateway-dependent.

Endpoint IDs follow registration order, then the preset’s `controls` order; read-only measurement/status endpoints follow the editable/action endpoints. Composite fields consume multiple slots in the shared 239-endpoint budget. Startup stages one endpoint per loop and reports one endpoint per loop. Values and action callbacks use the existing AppConfig routes and `persist`; accepting an action is not hardware completion.

Standard Basic ProductLabel contains `field name / property or action`, truncated to 63 UTF-8 bytes; Analog/Multistate endpoints also carry their Description. A gateway is not required to use these labels or expose all these clusters. Option/state names are documented here but are not currently sent as StateText. Unknown discrete readings set the standard status fault flag and leave the last representable ordinal unchanged.

`visibleWhen` does not remove endpoints or create a dynamic Zigbee form. Dependency rules do not affect Zigbee control visibility or command acceptance. Native endpoint definitions freeze at startup; changes need restart. Physical coordinator interoperability is still unverified.

See [Zigbee integration](https://github.com/rkubera/ArdPortal/wiki/Zigbee) and [page/order limitations](https://github.com/rkubera/ArdPortal/wiki/Zigbee#portal-pages-and-control-order).

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
