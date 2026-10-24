# update control

[Control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls) · [Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

![Portal update control](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/control-update.jpg)

`class` is always optional. See [the shared class catalogue](Control-Classes) for valid values, defaults and protocol mapping limits.

**Minimal field JSON:**

```json
{
  "id": "update",
  "type": "update",
  "name": "Update"
}
```

**Full field JSON:**

```json
{
  "id": "update",
  "type": "update",
  "name": "Update",
  "default": {
    "installed_version": "0.0.0",
    "latest_version": "0.0.0",
    "in_progress": false
  },
  "controls": [
    {
      "key": "",
      "type": "action",
      "label": "ui_140",
      "command": "",
      "payload": "INSTALL"
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


The control carries installed/latest firmware versions and an installation action. The portal preset displays the Install button; version metadata is used by the HA update entity.
Its default state is
`{"installed_version":"0.0.0","latest_version":"0.0.0","in_progress":false}`.
The Install button sends `INSTALL` on the base command topic and invokes
`onAppConfigCommand`. This entity does not automatically fetch firmware or invoke the
portal's OTA upload flow. Implement the update process and report version/progress
state from code. The separate built-in Update portal page handles uploaded images.

## Integration with Home Assistant

**MQTT component:** `update`. Requires the control’s feature flag, dynamic pages, MQTT and HA support to be enabled. ArdPortal registers MQTT Discovery and routes state/commands; the sketch implements device behavior. Action callbacks acknowledge acceptance, not physical completion.

**Supported by ArdPortal:** Installed/latest version strings and `in_progress` in the JSON state, plus an INSTALL action. The application owns version metadata and the ordinary MQTT-triggered update process; a configured native Zigbee OTA client can intercept INSTALL to query its coordinator.

**Not provided by the built-in integration / limits:** No automatic HTTP firmware download, GitHub release checking or automatic synchronization of version strings with Zigbee OTA image versions. The built-in HTTP upload page is a separate mechanism. The application must report version metadata; native Zigbee reception only updates `in_progress` automatically.

Explicit `ha` properties can override Discovery metadata. They do not create additional hardware behavior, application value properties or command handlers by themselves. See [HA integration](https://github.com/rkubera/ArdPortal/wiki/Home-Assistant) for the shared setup.

## Integration with Zigbee

**Native mapping:** OTA Upgrade client (`0x0019`). Requires ESP32-C5/C6/H2, `ARDPORTAL_ENABLE_ZIGBEE=1`, enabled dynamic pages and the control’s feature flag, with the matching End Device/Router board mode. MQTT/HA is not required. Register the initial pages before endpoint startup; later endpoint changes require restart. `persist` retains its AppConfig meaning.

**Supported by ArdPortal:** One field per device can own the native OTA client. It queries a coordinator for an image matching manufacturer/image-type/file-version, receives a standard container with one Upgrade Image subelement, validates firmware and requests the portal-managed restart. `in_progress` is mirrored to AppConfig in RAM.

**Not provided / limits:** Requires the OTA/control feature flags and adequate OTA partitions. It does not transfer HTTP `.bin.gz` files, publish release descriptions/URLs or synchronize installed/latest version strings. Numeric Zigbee OTA identity/version comes from compile-time defines, independently of the application version strings. The coordinator must provide the matching image.

`visibleWhen` does not dynamically add/remove Zigbee endpoints: they remain registered, and Zigbee commands remain accepted regardless of field visibility. Gateway presentation depends on its support for the mapped clusters. ArdPortal can export a [Zigbee2MQTT converter or ZHA quirk](https://github.com/rkubera/ArdPortal/wiki/Zigbee#zigbee2mqtt-converter-download); see the [validation scope](https://github.com/rkubera/ArdPortal/wiki/Zigbee#validation-and-troubleshooting).

See [Zigbee integration](https://github.com/rkubera/ArdPortal/wiki/Zigbee) for pairing, endpoint lifecycle, gateway limitations and OTA requirements.

## Read and update values from C++

These examples use the field ID from the JSON above and an existing `ArdPortal portal`. Run them after page registration has finished and `portal.portalAndAppConfigReady()` is true. Call `portal.loop()` frequently. Register callbacks once in `setup()`; do not repeat setters on every loop iteration.

### Set the value with automatic ArdFS persistence

Set **`"persist": true`** on this field in its page JSON. It is optional for editable fields whose preset already persists settings, but required to override a RAM-only preset such as a sensor. Calling `setAppConfigValue()` sets the control value and ArdPortal automatically saves AppConfig through its integrated ArdFS storage. Do not create an ArdFS object or call a separate write/flush method. The automatic save is coalesced and advanced by subsequent `portal.loop()` calls.

```cpp
// Use this path when the value is a setting that should survive restart.
ArdJSON::JSONVar next = ArdJSON::JSON.parse(R"JSON({"installed_version":"0.0.0","latest_version":"0.0.0","in_progress":false})JSON");
if (!next.isValid() || !portal.setAppConfigValue("update", next)) {
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
ArdJSON::JSONVar next = ArdJSON::JSON.parse(R"JSON({"installed_version":"0.0.0","latest_version":"0.0.0","in_progress":false})JSON");
if (!next.isValid() || !portal.setAppConfigStateValue("update", next, false)) {
  Serial.println("Runtime value rejected");
}
```

The third argument `false` avoids queuing an immediate MQTT publication. Use `true` or omit it to queue changed state. RAM updates work regardless of `persist`. Alternatively, with **`"persist": false`**, `setAppConfigValue()` also follows the RAM-only path.

### Read the current value

```cpp
const ArdJSON::JSONVar current = portal.getAppConfigValue("update");
if (!current.isUndefined() && current.isValid()) {
  Serial.println(ArdJSON::JSON.stringify(current));
}
```

The getter returns the effective field value: RAM override first, then saved AppConfig, then the definition default. A RAM-only update is lost on restart; the saved value/default becomes effective again. **Choose the persistent or RAM path for a given value.** A RAM override can mask a later persistent change for the same ID; there is no API to clear just that override. Prefer separate setting and telemetry IDs when both are needed.

[Back to control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls)
