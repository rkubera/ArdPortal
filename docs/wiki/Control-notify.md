# notify control

[Control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls) · [Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

![Portal notify control](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/control-notify.jpg)

`class` is always optional. See [the shared class catalogue](Control-Classes) for valid values, defaults and protocol mapping limits.

**Minimal field JSON:**

```json
{
  "id": "notify",
  "type": "notify",
  "name": "Notify"
}
```

**Full field JSON:**

```json
{
  "id": "notify",
  "type": "notify",
  "name": "Notify",
  "default": null,
  "controls": [
    {
      "key": "",
      "type": "action_text",
      "label": "ui_109",
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


**Appearance:** Text input and Send button

**Value:** String notification text, up to 512 bytes; the application delivers or processes it.

## Integration with Home Assistant

**MQTT component:** `notify`. Requires the control’s feature flag, dynamic pages, MQTT and HA support to be enabled. ArdPortal registers MQTT Discovery and routes state/commands; the sketch implements device behavior. Action callbacks acknowledge acceptance, not physical completion.

**Supported by ArdPortal:** A text message is delivered to the action callback on the base command topic; the preset uses the MQTT notify component.

**Not provided by the built-in integration / limits:** No built-in email, SMS, push service, message queue or delivery receipts. The text action is bounded to 512 bytes and requires the application command callback.

Explicit `ha` properties can override Discovery metadata. They do not create additional hardware behavior, application value properties or command handlers by themselves. See [HA integration](https://github.com/rkubera/ArdPortal/wiki/Home-Assistant) for the shared setup.

## Integration with Zigbee

**Status: not mapped by ArdPortal.** Text notifications are not sent through Zigbee. Enabling Zigbee does not create an endpoint for this field. The field remains usable in the portal and through MQTT/HA when those features are enabled.

This describes the current ArdPortal implementation, not a claim that Zigbee cannot represent a similar device through a standard cluster or a gateway-specific converter.

**Possible extension, not implemented:** Character-string payload in a custom ZCL command. A notification message can be transported over Zigbee, but standard measurement/control clusters do not define the ArdPortal notify service. Delivery semantics, size limits, repeat handling and gateway presentation require a custom protocol/converter. See the [mapping feasibility table](https://github.com/rkubera/ArdPortal/wiki/Zigbee#possible-mappings-not-yet-implemented).

See [Zigbee integration](https://github.com/rkubera/ArdPortal/wiki/Zigbee) for pairing, endpoint lifecycle, gateway limitations and OTA requirements.

## Integration with Matter

**Native mapping:** Not supported by the current ArdPortal Matter adapter.

**Supported by ArdPortal:** This control remains available in the portal and through its existing integrations described above; it does not create a Matter endpoint.

**Not provided / limits:** Matter does not transport notification delivery for this control. Adding `class` or `ha` metadata does not enable it, and MQTT Discovery settings do not configure Matter. This is an ArdPortal adapter limitation, not a statement that the Matter standard has no related device type. See [Matter control mappings](Matter#mappings) for supported alternatives.

## Read and update values from C++

These examples use the field ID from the JSON above and an existing `ArdPortal portal`. Run them after page registration has finished and `portal.portalAndAppConfigReady()` is true. Call `portal.loop()` frequently. Register callbacks once in `setup()`; do not repeat setters on every loop iteration.

This field represents a transient action/event, not a durable setting. Do not persist its command and automatically replay it after restart. The state setters are not a substitute for action execution or event publication.

### Handle an action without saving it

```cpp
portal.onAppConfigCommand([](const String& field, const String& command,
                            const ArdJSON::JSONVar& value,
                            ArdPortal::ChangeSource source) {
  if (field != "notify") return false; // Add branches for other action IDs.
  Serial.println(ArdJSON::JSON.stringify(value));
  // Queue the hardware/application action here; keep this callback nonblocking.
  return true; // Acceptance, not confirmation of physical completion.
});
```

Portal and MQTT actions reach this callback. The action payload is available as `value`; `getAppConfigValue("notify")` returns the field's null state, not a command history. Keep a separate application variable if the last action needs to remain available in RAM.

### Automatic persistence

This control has no persistent setting value: actions and events are transient. Do not save its command as a setting or replay it after restart. Automatic ArdFS persistence applies to persistent setting controls through `setAppConfigValue()`, not to this action/event field. No separate storage key or manual ArdFS write is needed for using this control.

[Back to control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls)
