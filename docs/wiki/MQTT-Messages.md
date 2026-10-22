# MQTT message construction

[Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home) · [All topics](https://github.com/rkubera/ArdPortal/wiki/Home#start-here) · [Source](https://github.com/rkubera/ArdPortal)

## On this page

- [MQTT, TLS and NTP](#mqtt-tls-and-ntp)
- [Registered field topics](#registered-field-topics)
- [Action acceptance and events](#action-acceptance-and-events)
- [Retain, scheduling and delivery](#retain-scheduling-and-delivery)
- [Topic placeholders](#topic-placeholders)

## MQTT, TLS and NTP

The built-in client implements MQTT 3.1.1 with clean sessions, QoS 0 and retain
publishing. It does not implement QoS 1/2 or persistent sessions. With HA enabled and
registered fields, it configures a retained `offline` Last Will for device
availability; there is no public custom Last Will API.
`mqttConnected()` becomes true only after an accepted CONNACK. An MQTT settings
form tests the candidate broker in RAM and persists it only after that acceptance.
Transport/authentication errors, timeout or storage failure restore previous
settings. An empty broker disables MQTT without a connection test.

```cpp
portal.wifiConnected();
portal.mqttConnected();
portal.wifiState();
portal.mqttState();
portal.apActive();
portal.localIP();
portal.apIP();

String commands = portal.mqttTopic("cmnd", "+");
portal.subscribe(commands.c_str());
String topic = portal.mqttTopic("stat", "status");
portal.publish(topic.c_str(), "online", false);
portal.onMqttMessage([](const String& topic, const uint8_t* bytes, size_t length) {
  // bytes are binary, not zero-terminated, and valid only during this callback.
});
```

Device topics use exactly three levels:

- `cmnd/<device name>/<command>`
- `get/<device name>/<command>`
- `stat/<device name>/<command>`

Public `publish` / `subscribe` validate this device namespace. Subscription
wildcards are allowed in the final level; publishing wildcards are forbidden.
The library processes received messages for this namespace, and the main example
subscribes to `cmnd` and `get`. Command names and query responses are implemented
by the application; the prefixes do not automatically define handlers.
Resubscribe after each MQTT reconnect.

The send buffer accepts one packet at a time. `publish` / `subscribe` return
`false` for disconnected/busy/invalid requests. The incoming packet capacity is
2048 bytes; outgoing bodies are limited to 2043 bytes. A missing/rejected SUBACK,
malformed/unsupported packet or expired keep-alive causes reconnecting.

TLS uses the core's `WiFiClientSecure`, requires a trusted PEM CA and a broker
hostname matching the certificate, and never falls back to plaintext after a TLS
failure. NTP synchronization starts through `configTime()` after each Wi-Fi
connection; the core retries/refreshes it in the background. Time is stored in
UTC; the browser formats it using the selected language and its local timezone.
TLS waits for a plausible time (2024-01-01 or later), without waiting in a loop.
Use a reachable local NTP server if the network has no Internet access.

### Custom MQTT payloads and reconnect handling

`publish()` accepts a NUL-terminated text payload. Serialize JSON yourself with
ArdJSON; the library does not add a wrapper or write the payload to ArdFS.
Binary payloads containing NUL are not supported by this public sending API.
Incoming callbacks expose bytes and length, so incoming binary payloads can be
handled by application code.

Public `portal.publish()` and `portal.mqtt().publish()` both restrict topics to
`cmnd`, `get` or `stat` for the current device name, with exactly one command
level. They do **not** expose the Console's arbitrary-topic publisher. For example,
`stat/<device>/custom_telemetry` is allowed, but `other/device/data` and
`stat/<device>/room/temperature` are rejected. Avoid registered field topics and
reserved `status`/`availability` when defining your own commands. There is no
public `publishRaw()` escape hatch.

Keep a pending message in RAM and retry once per loop when `publish()` returns
false. Do not spin until a send succeeds; HTTP, MQTT and the Wi-Fi driver need
continued servicing. Only one packet fits the shared sending buffer, and the
library's own subscriptions/state/discovery also use it. Coalescing to the latest
telemetry prevents an unbounded queue. A successful call means buffered locally;
QoS 0 has no broker delivery acknowledgement.

```cpp
// README example: custom MQTT helpers (call from the normal Arduino task).
String pendingPayload;
bool customMqttSession = false, customSubscriptionPending = false;

bool queueCustomTelemetry(double temperature, double humidity) {
  ArdJSON::JSONVar payload = ArdJSON::JSONVar::object();
  payload["temperature"] = temperature;
  payload["humidity"] = humidity;
  String error;
  String encoded = ArdJSON::JSON.stringify(payload, false, &error);
  String topic = portal.mqttTopic("stat", "custom_telemetry");
  if (!encoded.length() || !topic.length() ||
      topic.length() + encoded.length() + 2 > 2043) return false;
  pendingPayload = std::move(encoded); // Replace any unsent older reading.
  return true;
}

void serviceCustomMqtt() {
  if (!portal.mqttConnected()) {
    customMqttSession = false;
    return;
  }
  if (!customMqttSession) {
    customMqttSession = true;
    customSubscriptionPending = true;
  }
  if (customSubscriptionPending) {
    String filter = portal.mqttTopic("cmnd", "+");
    if (portal.subscribe(filter.c_str())) customSubscriptionPending = false;
    return;
  }
  if (pendingPayload.length()) {
    String topic = portal.mqttTopic("stat", "custom_telemetry");
    if (portal.publish(topic.c_str(), pendingPayload.c_str(), true))
      pendingPayload = String();
  }
}
// In loop(): portal.loop(); serviceCustomMqtt();
// On a sample/timer: queueCustomTelemetry(22.5, 48.0);
```

With registered dynamic fields, the library already subscribes to device `cmnd/+`
and `get/+` after reconnect. The explicit subscription above also works when
there are no forms; omit it if you only publish telemetry or rely on those
automatic subscriptions. Subscription `true` means the request was queued, not
that SUBACK has arrived. Broker rejection/timeouts cause reconnection.

```cpp
// Register this in setup(), before begin(). One callback handles custom messages.
portal.onMqttMessage([](const String& topic, const uint8_t* bytes, size_t length) {
  if (topic != portal.mqttTopic("cmnd", "custom_command") || length > 512) return;
  String text;
  if (!text.reserve(length)) return;
  for (size_t i = 0; i < length; ++i) text += char(bytes[i]);
  String error;
  const auto command = ArdJSON::JSON.parse(text, &error);
  if (!command.isValid() || command.type() != ArdJSON::JSONVar::Type::Object) return;
  // Copy/validate command properties and queue your application work here.
  // Send a custom stat reply later from loop(); the send buffer may be busy now.
});
```

Payloads of built-in fields are separate contracts: strings are sent as text,
`switch`/`binary_sensor` use `ON`/`OFF`, and numbers/composite states are serialized
JSON. `queueAppConfigStatePublish()` publishes one of these current values; use
`publish()` for a custom object containing several readings.

## Registered field topics

Use `mqtt.state_topic` to override a field's publication topic, with or without HA:

```json
{"id":"pump","name":"Pump","type":"switch","mqtt":{"state_topic":"stat/$device/pump_state"}}
```

The same topic is used for state updates, state reports after actions and event
emissions, and is advertised by HA discovery when enabled. `mqtt.state_topic`
takes precedence over legacy `ha.state_topic` (or `ha.topic` for tags/triggers
and `ha.activity_state_topic` for lawn mowers). Missing, empty or `$state`
values select the default ID-based topic; an explicitly empty MQTT setting also
overrides a legacy HA setting. Command and acknowledgement topics remain unchanged.


With at least one registered field, ArdPortal subscribes to `cmnd/<device>/+`,
`get/<device>/+` and `homeassistant/status`. Retained commands are ignored;
publish commands without retain. `get/<device>/<id>` queues fresh current state.
States are normally retained at `stat/<device>/<id>`.

| Field | Example command topic | Command payload | State payload |
| --- | --- | --- | --- |
| Switch `relay` | `cmnd/room/relay` | `ON` or `OFF` (plain text) | `ON` or `OFF` |
| Slider `level` | `cmnd/room/level` | `50` | JSON number `50` |
| Basic select `profile` | `cmnd/room/profile` | Stable option value, e.g. `eco` | `eco` as plain text; HA templates map translated labels |
| Edit `note` | `cmnd/room/note` | Text such as `Ready` | Plain text |
| Read-only text `temperature` | None accepted | Report from code | JSON number or plain string, according to value type |
| Climate `thermostat` target | `cmnd/room/thermostat_temperature` | `22.5` | Complete object at `stat/room/thermostat` |
| Climate mode | `cmnd/room/thermostat_mode` | `heat` | Same complete object |
| Climate fan | `cmnd/room/thermostat_fan` | `low` | Same complete object |
| Light `lamp` | `cmnd/room/lamp` | JSON patch, e.g. `{"color":{"r":255,"g":80,"b":0}}` | Complete light state object |
| Binary sensor | No writable command | Report from code | `ON` or `OFF` |

Descriptor-based entities can define suffix commands through `controls` and `ha`.
For example, `command:"percentage"` creates `<fieldId>_percentage`; it does not
create a fourth slash level. See the complete descriptor contracts in [Controls](https://github.com/rkubera/ArdPortal/wiki/Controls).

### Climate example

Command:

```text
cmnd/room/thermostat_temperature = 22.5
```

State (required/optional keys depend on the registered climate schema):

```json
{"temperature":22.5,"current_temperature":24.5,"mode":"heat","action":"heating"}
```

### Light example

Commands on `cmnd/room/lamp` can change one or several supported properties:

```json
{"state":"ON","brightness":200,"color":{"r":255,"g":80,"b":0}}
```

The retained state on `stat/room/lamp` contains the complete reported value:

```json
{"state":"ON","brightness":200,"color":{"r":255,"g":80,"b":0},"color_mode":"rgb"}
```

## Action acceptance and events

Accepted explicit actions attempt to queue a nonretained reply containing
`accepted`, `command` and `value`. Transient actions use `stat/<device>/<id>`;
stateful action entities use `stat/<device>/<id>_ack` so their ordinary state
topic keeps its own payload contract:

```json
{"accepted":true,"command":"","value":"PRESS"}
```

The command suffix and value come from the descriptor/callback. This acknowledges
local acceptance, not physical completion or a broker delivery acknowledgement.
MQTT disconnection/full reply FIFO does not reject a locally accepted action.
Events/device triggers/tags are emitted with `emitAppConfigEvent`, nonretained,
and are never replayed periodically. With MQTT enabled they require connection
and a free slot in the shared eight-entry emission FIFO.

## Retain, scheduling and delivery

| Message | Retained? | Trigger |
| --- | --- | --- |
| Field state | Yes | Changed value, accepted command reply, get, reconnect, HA birth or configured state interval |
| Discovery config | Yes | Registration/initial connection, reconnect, HA birth, visibility change or explicitly enabled Discovery interval |
| Discovery removal | Yes, empty payload | Dependency becomes false |
| Availability | Yes | Connected `online`; broker Last Will `offline` |
| Action acceptance / transient event | No | Accepted action / event emission |
| Custom `publish` | Caller chooses | Application retry scheduler |

`publish()==true` means queued locally, not delivered. QoS 0 has no broker
acknowledgement. State echo confirms the reported/requested application value;
`onPortalAndAppConfigSaved` is the separate durable-save notification. Application
hardware confirmation must be reported explicitly.

## Topic placeholders

| Placeholder | Expanded value |
| --- | --- |
| `$state` | Selected MQTT state topic in HA discovery; default `stat/<device>/<fieldId>` |
| `$command` | `cmnd/<device>/<fieldId>` |
| `$command:<suffix>` | `cmnd/<device>/<fieldId>_<suffix>` |
| `$device` | Current normalized MQTT device name, in direct `mqtt` and `ha` topic properties |

Expansion happens when reading a field, so immutable PROGMEM sources stay in
flash. Payloads, names and Jinja templates are not modified by `$device`.
`$device` applies to direct string-valued MQTT/HA topic properties; HA discovery also resolves `$state` and `$command`. These do not apply to arbitrary
nested JSON objects. IDs remain stable across changes of device name.

![MQTT Console example](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/Demo7.png)

---

[Back to documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)
