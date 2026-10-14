# button control

[Control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls) · [Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

![Portal button control](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/control-button.jpg)

**Minimal field JSON:**

```json
{
  "id": "button",
  "type": "button",
  "name": "Button"
}
```

**Full field JSON:**

```json
{
  "id": "button",
  "type": "button",
  "name": "Button",
  "default": null,
  "controls": [
    {
      "key": "",
      "type": "action",
      "label": "ui_107",
      "command": "",
      "payload": "PRESS"
    }
  ],
  "ha": {
    "command_topic": "$command",
    "payload_press": "PRESS"
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


**Appearance:** Action button

**Value:** Literal string `PRESS`; perform a short operation, such as queuing a calibration.

## Integration with Home Assistant

**MQTT component:** `button`. Requires the control’s feature flag, dynamic pages, MQTT and HA support to be enabled. ArdPortal registers MQTT Discovery and routes state/commands; the sketch implements device behavior. Action callbacks acknowledge acceptance, not physical completion.

**Supported by ArdPortal:** A HA press sends `PRESS` to the base command topic and invokes `onAppConfigCommand`. The command callback must accept and execute the action.

**Not provided by the built-in integration / limits:** No persistent on/off state, automatic hardware action, repeated/long-press gesture recognition, or confirmation that the hardware finished. An acceptance acknowledgement is not completion.

Explicit `ha` properties can override Discovery metadata. They do not create additional hardware behavior, application value properties or command handlers by themselves. See [HA integration](https://github.com/rkubera/ArdPortal/wiki/Home-Assistant) for the shared setup.

## Integration with Zigbee

**Implemented without a converter supplied by ArdPortal.** The functions below are separate standard Zigbee endpoints on the same physical device; ArdPortal does not create a gateway-specific combined control. Requires ESP32-C5/C6, Zigbee and dynamic pages enabled, and the field’s control flag enabled. MQTT/HA is not required.

**Standard mapping:** Momentary On/Off.

**Supported by ArdPortal:** Sending ON invokes the preset PRESS action through onAppConfigCommand. OFF does not execute the action. After processing, the endpoint reports OFF again.

**Not provided / limits:** It can appear as a switch rather than a button. OFF is a reset/idle state, not an undo command or completion confirmation. Commands sent faster than the bounded queue/report cycle can be rejected or coalesced by the stack/gateway; this is not a lossless gesture-event stream.

Endpoint IDs follow registration order, then the preset’s `controls` order; read-only measurement/status endpoints follow the editable/action endpoints. Composite fields consume multiple slots in the shared 239-endpoint budget. Startup stages one endpoint per loop and reports one endpoint per loop. Values and action callbacks use the existing AppConfig routes and `persist`; accepting an action is not hardware completion.

Standard Basic ProductLabel contains `field name / property or action`, truncated to 63 UTF-8 bytes; Analog/Multistate endpoints also carry their Description. A gateway is not required to use these labels or expose all these clusters. Option/state names are documented here but are not currently sent as StateText. Unknown discrete readings set the standard status fault flag and leave the last representable ordinal unchanged.

`visibleWhen` does not remove endpoints or create a dynamic Zigbee form. Dependency rules do not affect Zigbee control visibility or command acceptance. Native endpoint definitions freeze at startup; changes need restart. Physical coordinator interoperability is still unverified.

See [Zigbee integration](https://github.com/rkubera/ArdPortal/wiki/Zigbee) and [page/order limitations](https://github.com/rkubera/ArdPortal/wiki/Zigbee#portal-pages-and-control-order).

## Read and update values from C++

These examples use the field ID from the JSON above and an existing `ArdPortal portal`. Run them after page registration has finished and `portal.portalAndAppConfigReady()` is true. Call `portal.loop()` frequently. Register callbacks once in `setup()`; do not repeat setters on every loop iteration.

This field represents a transient action/event, not a durable setting. Do not persist its command and automatically replay it after restart. The state setters are not a substitute for action execution or event publication.

### Handle an action without saving it

```cpp
portal.onAppConfigCommand([](const String& field, const String& command,
                            const ArdJSON::JSONVar& value,
                            ArdPortal::ChangeSource source) {
  if (field != "button") return false; // Add branches for other action IDs.
  Serial.println(ArdJSON::JSON.stringify(value));
  // Queue the hardware/application action here; keep this callback nonblocking.
  return true; // Acceptance, not confirmation of physical completion.
});
```

Portal and MQTT actions reach this callback. The action payload is available as `value`; `getAppConfigValue("button")` returns the field's null state, not a command history. Keep a separate application variable if the last action needs to remain available in RAM.

### Automatic persistence

This control has no persistent setting value: actions and events are transient. Do not save its command as a setting or replay it after restart. Automatic ArdFS persistence applies to persistent setting controls through `setAppConfigValue()`, not to this action/event field. No separate storage key or manual ArdFS write is needed for using this control.

[Back to control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls)
