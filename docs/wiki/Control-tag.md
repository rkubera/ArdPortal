# tag control

[Control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls) · [Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

![Portal tag control](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/control-tag.jpg)

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
  "controls": [],
  "ha": {
    "topic": "$state"
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
| `readonly` | **Optional** | Omit to use the preset editability policy. |
| `controls` | **Optional** | Omit to use the preset controls; supplying an array replaces them. |
| `ha` | **Optional** | Omit to use automatic HA discovery; supplied properties override defaults. |

¹ `name` can be replaced by `names`. Other shared optional properties are listed [in the catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls#required-and-optional-properties).


**Default / example:** `""`; example `"tag-123"`

Creates MQTT tag discovery. Publish the scanned identifier as a string with `portal.emitAppConfigEvent("reader", "tag-123")`; the library does not read a physical tag.

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
