# datetime control

[Control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls) · [Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

![Portal datetime control](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/control-datetime.jpg)

`class` is always optional. See [the shared class catalogue](Control-Classes) for valid values, defaults and protocol mapping limits.

**Minimal field JSON:**

```json
{
  "id": "datetime",
  "type": "datetime",
  "name": "Datetime"
}
```

**Full field JSON:**

```json
{
  "id": "datetime",
  "type": "datetime",
  "name": "Datetime",
  "default": "2000-01-01T00:00:00Z",
  "controls": [
    {
      "key": "",
      "type": "datetime",
      "label": "ui_049",
      "command": ""
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


**Default / example:** `"2000-01-01T00:00:00Z"`

Date/time input with an ISO-style string containing a timezone: for example `2026-10-02T12:30:00Z` or `2026-10-02T14:30:00+02:00`. Fractional seconds are accepted. The browser control sends a UTC `Z` value.

## Integration with Home Assistant

**MQTT component:** `datetime`. Requires the control’s feature flag, dynamic pages, MQTT and HA support to be enabled. ArdPortal registers MQTT Discovery and routes state/commands; the sketch implements device behavior. Action callbacks acknowledge acceptance, not physical completion.

**Supported by ArdPortal:** Read/write timestamp state and commands with a calendar date, time and explicit `Z` or UTC offset. The validator accepts optional fractional seconds (up to six digits).

**Not provided by the built-in integration / limits:** No timezone database, recurring schedule, automatic clock-setting or conversion of the stored timestamp. The application owns interpretation and execution.

Explicit `ha` properties can override Discovery metadata. They do not create additional hardware behavior, application value properties or command handlers by themselves. See [HA integration](https://github.com/rkubera/ArdPortal/wiki/Home-Assistant) for the shared setup.

## Integration with Zigbee

**Status: not mapped by ArdPortal.** Timestamp values and commands are not mapped; there is no automatic Zigbee Time endpoint. Enabling Zigbee does not create an endpoint for this field. The field remains usable in the portal and through MQTT/HA when those features are enabled.

This describes the current ArdPortal implementation, not a claim that Zigbee cannot represent a similar device through a standard cluster or a gateway-specific converter.

**Possible extension, not implemented:** Time cluster for the device clock; custom UTC-time attribute for an application timestamp. The Time cluster can exchange actual device/network clock information. Independent AppConfig timestamps need their own attributes and a converter. ZCL UTC time has a fixed epoch/range and does not preserve the original offset or fractional-second representation. It cannot reproduce every accepted ArdPortal timestamp unchanged. See the [mapping feasibility table](https://github.com/rkubera/ArdPortal/wiki/Zigbee#possible-mappings-not-yet-implemented).

See [Zigbee integration](https://github.com/rkubera/ArdPortal/wiki/Zigbee) for pairing, endpoint lifecycle, gateway limitations and OTA requirements.

## Integration with Matter

**Native mapping:** Not supported by the current ArdPortal Matter adapter.

**Supported by ArdPortal:** This control remains available in the portal and through its existing integrations described above; it does not create a Matter endpoint.

**Not provided / limits:** Matter does not transport date/time values for this control. Adding `class` or `ha` metadata does not enable it, and MQTT Discovery settings do not configure Matter. This is an ArdPortal adapter limitation, not a statement that the Matter standard has no related device type. See [Matter control mappings](Matter#mappings) for supported alternatives.

## Read and update values from C++

These examples use the field ID from the JSON above and an existing `ArdPortal portal`. Run them after page registration has finished and `portal.portalAndAppConfigReady()` is true. Call `portal.loop()` frequently. Register callbacks once in `setup()`; do not repeat setters on every loop iteration.

### Set the value with automatic ArdFS persistence

Set **`"persist": true`** on this field in its page JSON. It is optional for editable fields whose preset already persists settings, but required to override a RAM-only preset such as a sensor. Calling `setAppConfigValue()` sets the control value and ArdPortal automatically saves AppConfig through its integrated ArdFS storage. Do not create an ArdFS object or call a separate write/flush method. The automatic save is coalesced and advanced by subsequent `portal.loop()` calls.

```cpp
// Use this path when the value is a setting that should survive restart.
ArdJSON::JSONVar next = "2000-01-01T00:00:00Z";
if (!next.isValid() || !portal.setAppConfigValue("datetime", next)) {
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
ArdJSON::JSONVar next = "2000-01-01T00:00:00Z";
if (!next.isValid() || !portal.setAppConfigStateValue("datetime", next, false)) {
  Serial.println("Runtime value rejected");
}
```

The third argument `false` avoids queuing an immediate MQTT publication. Use `true` or omit it to queue changed state. RAM updates work regardless of `persist`. Alternatively, with **`"persist": false`**, `setAppConfigValue()` also follows the RAM-only path.

### Read the current value

```cpp
const ArdJSON::JSONVar current = portal.getAppConfigValue("datetime");
if (!current.isUndefined() && current.isValid()) {
  Serial.println(ArdJSON::JSON.stringify(current));
}
```

The getter returns the effective field value: RAM override first, then saved AppConfig, then the definition default. A RAM-only update is lost on restart; the saved value/default becomes effective again. **Choose the persistent or RAM path for a given value.** A RAM override can mask a later persistent change for the same ID; there is no API to clear just that override. Prefer separate setting and telemetry IDs when both are needed.

[Back to control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls)
