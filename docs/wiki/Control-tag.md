# tag control

[Control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls) · [Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

![Portal tag control](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/control-tag.jpg)

`class` is always optional. See [the shared class catalogue](Control-Classes) for valid values, defaults and protocol mapping limits.

**Minimal field JSON:**

```json
{
  "id": "tag",
  "type": "tag",
  "name": "Tag"
}
```

**Full field JSON:**

```json
{
  "id": "tag",
  "type": "tag",
  "name": "Tag",
  "default": "",
  "readonly": true,
  "controls": []
}
```

**Properties in the full example:**

| Property | Required? | Meaning / default |
| --- | --- | --- |
| `id` | Required | Stable application key; unique across registered pages. |
| `type` | Required | Enabled field type. |
| `name` | Required title¹ | Display title; may be replaced by a nonempty `names` object. |
| `default` | **Optional** | Omit to use the built-in initial value. |
| `readonly` | **Optional** | Omit to use the preset editability policy. |
| `controls` | **Optional** | Omit to use the preset controls; supplying an array replaces them. |

¹ `name` can be replaced by `names`. Other shared optional properties are listed [in the catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls#required-and-optional-properties).


**Default / example:** `""`; example `"tag-123"`

Creates MQTT tag discovery. Publish the scanned identifier as a string with `portal.emitAppConfigEvent("reader", "tag-123")`; the library does not read a physical tag.

## Integration with Home Assistant

**MQTT component:** `tag`. Requires the control’s feature flag, dynamic pages, MQTT and HA support to be enabled. ArdPortal registers MQTT Discovery and routes state/commands; the sketch implements device behavior. Action callbacks acknowledge acceptance, not physical completion.

**Supported by ArdPortal:** Transient tag identifiers emitted through `emitAppConfigEvent()` on the discovered topic can drive HA tag automations.

**Not provided by the built-in integration / limits:** No RFID/NFC reader driver, automatic tag scanning, persistent tag history or editable tag control. The application provides the tag identifier.

Explicit `ha` properties can override Discovery metadata. They do not create additional hardware behavior, application value properties or command handlers by themselves. See [HA integration](https://github.com/rkubera/ArdPortal/wiki/Home-Assistant) for the shared setup.

## Integration with Zigbee

**Status: not mapped by ArdPortal.** Tag identifiers are not transported through Zigbee. Enabling Zigbee does not create an endpoint for this field. The field remains usable in the portal and through MQTT/HA when those features are enabled.

This describes the current ArdPortal implementation, not a claim that Zigbee cannot represent a similar device through a standard cluster or a gateway-specific converter.

**Possible extension, not implemented:** Character/octet-string attribute or custom scan command. A tag identifier can be transmitted, but existing standard generic clusters do not describe arbitrary NFC/RFID scans as HA tags. Repeatable scans require event semantics; a converter must interpret the identifier and emit the host tag event. See the [mapping feasibility table](https://github.com/rkubera/ArdPortal/wiki/Zigbee#possible-mappings-not-yet-implemented).

See [Zigbee integration](https://github.com/rkubera/ArdPortal/wiki/Zigbee) for pairing, endpoint lifecycle, gateway limitations and OTA requirements.

## Integration with Matter

**Native mapping:** Not supported by the current ArdPortal Matter adapter.

**Supported by ArdPortal:** This control remains available in the portal and through its existing integrations described above; it does not create a Matter endpoint.

**Not provided / limits:** Matter does not transport tag scans for this control. Adding `class` or `ha` metadata does not enable it, and MQTT Discovery settings do not configure Matter. This is an ArdPortal adapter limitation, not a statement that the Matter standard has no related device type. See [Matter control mappings](Matter#mappings) for supported alternatives.

## Read and update values from C++

These examples use the field ID from the JSON above and an existing `ArdPortal portal`. Run them after page registration has finished and `portal.portalAndAppConfigReady()` is true. Call `portal.loop()` frequently. Register callbacks once in `setup()`; do not repeat setters on every loop iteration.

This field represents a transient action/event, not a durable setting. Do not persist its command and automatically replay it after restart. The state setters are not a substitute for action execution or event publication.

### Publish an occurrence and read the current local value

```cpp
ArdJSON::JSONVar occurrence = "tag-123";
if (!occurrence.isValid() || !portal.emitAppConfigEvent("tag", occurrence)) {
  Serial.println("Event rejected: MQTT disconnected, queue full or invalid value");
}
const ArdJSON::JSONVar current = portal.getAppConfigValue("tag");
if (current.isValid()) Serial.println(ArdJSON::JSON.stringify(current));
```

An accepted event updates local RAM and, with MQTT enabled, queues a nonretained message. It does not write ArdFS. With MQTT enabled, a disconnected broker or full queue returns `false`; the occurrence is not stored for replay. Without MQTT, supported events update local RAM only.

### Automatic persistence

This control has no persistent setting value: actions and events are transient. Do not save its command as a setting or replay it after restart. Automatic ArdFS persistence applies to persistent setting controls through `setAppConfigValue()`, not to this action/event field. No separate storage key or manual ArdFS write is needed for using this control.

[Back to control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls)
