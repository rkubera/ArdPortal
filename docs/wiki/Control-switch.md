# switch control

[Control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls) · [Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

![Portal switch control](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/control-switch.jpg)

`class` is always optional. See [the shared class catalogue](Control-Classes) for valid values, defaults and protocol mapping limits.

**Minimal field JSON:**

```json
{
  "id": "enabled",
  "type": "switch",
  "name": "Enabled"
}
```

**Full field JSON:**

```json
{
  "id": "enabled",
  "type": "switch",
  "name": "Enabled",
  "default": false
}
```

**Properties in the full example:**

| Property | Required? | Meaning / default |
| --- | --- | --- |
| `id` | Required | Stable application key; unique across registered pages. |
| `type` | Required | Enabled field type. |
| `name` | Required title¹ | Display title; may be replaced by a nonempty `names` object. |
| `default` | **Optional** | Omit to use the built-in initial value. |

¹ `name` can be replaced by `names`. Other shared optional properties are listed [in the catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls#required-and-optional-properties).


**Appearance:** Graphical on/off switch

**Value:** Boolean

Code uses `true`/`false`; MQTT commands use unquoted `ON`/`OFF`.

## Integration with Home Assistant

**MQTT component:** `switch`. Requires the control’s feature flag, dynamic pages, MQTT and HA support to be enabled. ArdPortal registers MQTT Discovery and routes state/commands; the sketch implements device behavior. Action callbacks acknowledge acceptance, not physical completion.

**Supported by ArdPortal:** Boolean application state is published as `ON`/`OFF`; HA commands accept those two payloads. Discovery supplies state/command topics and a toggle icon unless explicitly overridden.

**Not provided by the built-in integration / limits:** No automatic GPIO/relay control or power/energy measurement. ArdPortal does not force an `optimistic` setting; an accepted command is not proof of the physical relay state.

Explicit `ha` properties can override Discovery metadata. They do not create additional hardware behavior, application value properties or command handlers by themselves. See [HA integration](https://github.com/rkubera/ArdPortal/wiki/Home-Assistant) for the shared setup.

## Integration with Zigbee

**Native mapping:** On/Off (`0x0006`). Requires ESP32-C5/C6/H2, `ARDPORTAL_ENABLE_ZIGBEE=1`, enabled dynamic pages and the control’s feature flag, with the matching End Device/Router board mode. MQTT/HA is not required. Register the initial pages before endpoint startup; later endpoint changes require restart. `persist` retains its AppConfig meaning.

**Supported by ArdPortal:** On/off reports and incoming on/off state changes. Boolean state is shared with the portal and HA.

**Not provided / limits:** No extra power/energy measurement, switch groups/scenes management or physical relay driver.

`visibleWhen` does not dynamically add/remove Zigbee endpoints: they remain registered, and Zigbee commands remain accepted regardless of field visibility. Gateway presentation depends on its support for the mapped clusters. ArdPortal can export a [Zigbee2MQTT converter or ZHA quirk](https://github.com/rkubera/ArdPortal/wiki/Zigbee#zigbee2mqtt-converter-download); see the [validation scope](https://github.com/rkubera/ArdPortal/wiki/Zigbee#troubleshooting).

See [Zigbee integration](https://github.com/rkubera/ArdPortal/wiki/Zigbee) for pairing, endpoint lifecycle, gateway limitations and OTA requirements.

## Integration with Matter

**Native mapping:** On/Off plug or On/Off light (On/Off cluster `0x0006`).

**Definition requirements:** An omitted `class`, `"switch"` or `"outlet"` creates a plug endpoint; `"light"` creates an On/Off light. `class` remains optional. Explicit `readonly: true` is unsupported.

**Supported by ArdPortal:** Reports the boolean application value and accepts controller On/Off commands. Portal/application changes and Matter commands share the same value.

**Not provided / limits:** No relay driver, energy/power measurements or brightness. Controller presentation may be a switch or a light according to its handling of the device type; the portal label does not force the HA entity domain.

**Setup:** Requires a supported ESP32 target, `ARDPORTAL_ENABLE_MATTER=1`, dynamic pages and the control’s feature flag. Select Matter Wi-Fi or Thread at compilation; MQTT/HA Discovery is not required. Register the initial pages before Matter endpoint startup. Native Zigbee and Matter are alternative firmware integrations.

The sketch supplies hardware behavior and updates the shared application value; Matter does not drive GPIOs or sensors. Call `portal.loop()` frequently. Incoming writable commands use normal validation/persistence and `ChangeSource::Matter`; acceptance does not confirm physical completion. `visibleWhen` only affects the portal UI and does not remove a Matter endpoint or disable its commands. Changes to the native endpoint layout may require a Matter reset and commissioning again; see [Matter integration](Matter#mappings).

## Read and update values from C++

These examples use the field ID from the JSON above and an existing `ArdPortal portal`. Run them after page registration has finished and `portal.portalAndAppConfigReady()` is true. Call `portal.loop()` frequently. Register callbacks once in `setup()`; do not repeat setters on every loop iteration.

### Set the value with automatic ArdFS persistence

Set **`"persist": true`** on this field in its page JSON. It is optional for editable fields whose preset already persists settings, but required to override a RAM-only preset such as a sensor. Calling `setAppConfigValue()` sets the control value and ArdPortal automatically saves AppConfig through its integrated ArdFS storage. Do not create an ArdFS object or call a separate write/flush method. The automatic save is coalesced and advanced by subsequent `portal.loop()` calls.

```cpp
// Use this path when the value is a setting that should survive restart.
ArdJSON::JSONVar next = false;
if (!next.isValid() || !portal.setAppConfigValue("enabled", next)) {
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
ArdJSON::JSONVar next = false;
if (!next.isValid() || !portal.setAppConfigStateValue("enabled", next, false)) {
  Serial.println("Runtime value rejected");
}
```

The third argument `false` avoids queuing an immediate MQTT publication. Use `true` or omit it to queue changed state. RAM updates work regardless of `persist`. Alternatively, with **`"persist": false`**, `setAppConfigValue()` also follows the RAM-only path.

### Read the current value

```cpp
const ArdJSON::JSONVar current = portal.getAppConfigValue("enabled");
if (!current.isUndefined() && current.isValid()) {
  Serial.println(ArdJSON::JSON.stringify(current));
}
```

The getter returns the effective field value: RAM override first, then saved AppConfig, then the definition default. A RAM-only update is lost on restart; the saved value/default becomes effective again. **Choose the persistent or RAM path for a given value.** A RAM override can mask a later persistent change for the same ID; there is no API to clear just that override. Prefer separate setting and telemetry IDs when both are needed.

[Back to control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls)
