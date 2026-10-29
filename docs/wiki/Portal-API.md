# Portal API

[Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home) · [All topics](https://github.com/rkubera/ArdPortal/wiki/Home#start-here) · [Source](https://github.com/rkubera/ArdPortal)

## On this page

- [Integrated configuration API](#integrated-configuration-api)
- [Options and remaining blocking operations](#options-and-remaining-blocking-operations)
- [Registration status accessors](#registration-status-accessors)
- [Callback distinction](#callback-distinction)

## Integrated configuration API

Names explicitly identify the domain: `PortalConfig` covers Wi-Fi, MQTT and device
settings; `AppConfig` covers custom JSON values, dynamic pages, commands, state and
events. `PortalAndAppConfig` denotes shared loading/storage operations: both domains
are persisted in one document.


The portal owns storage initialization, configuration loading and saving.
Application code uses the methods and notification callbacks below.

```cpp
portal.onPortalAndAppConfigReady([](bool restored) {
  // Called once after initialization, including a missing file/storage failure.
  // restored means a valid saved configuration was loaded.
  ArdJSON::JSONVar value = portal.getAppConfigValue("reportInterval");
  if (!value.isUndefined()) {
    int64_t interval;
    if (value.toInteger(interval)) {
      // Validate the application-specific range before using it.
    }
  }
});

portal.onPortalConfigChanged([](const ArdPortal::PortalConfig& config,
                         ArdPortal::ChangeSource source) {
  // Called only when PortalConfig changes; AppConfig-only writes do not trigger it.
  // source: Application, Portal, Mqtt or FactoryReset.
});

portal.onPortalAndAppConfigSaved([](bool ok, const String& error) {
  // Write/readback completed, or an identical document was skipped.
  // If ok is false, the pending update was rejected; application code may retry.
});
```

`onPortalConfigChanged` and `onAppConfigValueChanged` each use one callback for
all change sources. Filter `source == ArdPortal::ChangeSource::Portal` to handle
only web portal changes.
For persisted settings, notifications follow successful saving and application;
unchanged values and failed writes do not trigger change callbacks. AppConfig
RAM-only state updates and MQTT commands also notify when applied in RAM.
Use `onPortalAndAppConfigSaved` for the storage result, including unchanged or failed writes.

Setters accept changes into RAM; `true` does not mean flash has already been
written. `onPortalAndAppConfigSaved` reports completion. Once configuration is ready:

```cpp
portal.setAppConfigValue("reportInterval", 5000);
portal.setAppConfigValue("enabled", true);
portal.removeAppConfigValue("obsoleteKey");

ArdPortal::PortalConfig changed = portal.getPortalConfig();
changed.deviceName = "My-Device";
portal.setPortalConfig(changed);

// Optional: bypass debounce/minimum interval; writing still happens in loop().
portal.flushPortalAndAppConfig();
```

| Method | Meaning |
| --- | --- |
| `getPortalConfig()` | Current portal settings; MQTT trials temporarily expose candidate settings |
| `setPortalConfig(config)` | Validate and queue application changes to portal settings |
| `getAppConfigValue(key)` | Copy the current application value, live state or registered field default |
| `setAppConfigValue(key, value)` | Queue a persistent JSON value, or update a volatile extended entity |
| `removeAppConfigValue(key)` | Queue removal of an application key |
| `portalAndAppConfigReady()` | Startup loading has finished |
| `portalAndAppConfigBusy()` | Initialization, pending save, physical save or pending application |
| `flushPortalAndAppConfig()` | Bypass save delay; does not perform flash I/O in the caller |
| `portalAndAppConfigStorageOK()` / `portalAndAppConfigError()` | Result/error of the last storage operation |

Missing unregistered application keys return JSON `Undefined`; registered fields
return their declared or type-specific default. Successive accepted application
updates merge into the pending document. Setters can return `false` while a write,
OTA, MQTT trial or another settings transaction is running; retry later.
Callbacks may read settings or queue another update when the API accepts it.
Change callbacks are not invoked for unchanged settings or failed writes.
`onPortalAndAppConfigSaved` may precede applying a network change; change callbacks run after
application. Register dynamic page definitions to expose selected `app` keys in
portal forms and Home Assistant.

The configuration payload contains `config` and `app` objects. The complete
payload is limited to 13376 bytes, serialized PortalConfig to 5120 bytes and
serialized AppConfig to 8192 bytes
and a CA certificate to 4096 bytes. Available RAM, particularly with TLS on
ESP8266, can impose lower practical limits. `ConfigJson.h` validates the portal
schema, and `ArdJSON.h` handles JSON parsing/serialization.

### Portal configuration fields

`ArdPortal::PortalConfig` is the complete network/device configuration. Application
keys belong to the separate `app` object, not to this struct. Length limits below
are bytes, not Unicode characters. All strings reject embedded NUL bytes.

| Member | Type / default | Constraints and purpose |
| --- | --- | --- |
| `ssid` | `String`, empty | Up to 32 bytes; empty disables station connection. |
| `password` | `String`, empty | Up to 64 bytes; station Wi-Fi password. Empty supports an open network. |
| `host` | `String`, empty | Up to 253 bytes; MQTT hostname or IPv4 address. Empty disables MQTT. |
| `port` | `uint16_t`, `1883` | 1–65535; set explicitly to `8883` for typical TLS brokers. |
| `user` | `String`, empty | Up to 128 bytes; MQTT username. |
| `mqttPassword` | `String`, empty | Up to 128 bytes; nonempty requires a nonempty `user`. |
| `mqttTls` | `bool`, `false` | Verified TLS; setting it is rejected when MQTT TLS is compiled out. |
| `caCert` | `String`, empty | Up to 4096 bytes; PEM CA certificate/bundle, required for a nonempty TLS broker host. |
| `deviceName` | `String`, generated at startup | Up to 32 UTF-8 bytes: Unicode letters/numbers, spaces, hyphens and underscores. Other symbols and all-space names are rejected. Empty becomes `ArdUI-<chip ID>`. AP SSID and MQTT identity/topics replace spaces with hyphens. Hostname uses ASCII letters/digits and hyphens, with the generated name as fallback. Portal and HA retain the display name. |
| `deviceDescription` | `String`, empty | Up to 128 bytes; optional Device input and Info row; HA `device.model`. |
| `deviceManufacturer` | `String`, `"DYI"` | Up to 128 bytes; set from code; optional Info row and HA `device.manufacturer`. |
| `apName` | `String`, normalized to device name | Must be nonempty and at most 32 bytes in a validated Config; accepted saves derive it from `deviceName` with spaces replaced by hyphens. It is not an independent SSID setting. |
| `apPassword` | `String`, `"1234567890"` | Empty for an open AP, otherwise 8–63 bytes. |
| `apBand` | `uint8_t`, `24` | Preferred AP band: `24` = 2.4 GHz, `5` = 5 GHz on dual-band chips; connected STA determines the active channel. |

Copy `getPortalConfig()`, edit the copy, then call `setPortalConfig()`. A change
made through this API is validated and scheduled for storage; it does not run the
connect-before-save trial used by the Wi-Fi/MQTT portal forms. Network changes
may disconnect current clients. Device-name changes update AP/hostname/MQTT
identity; do not rely on the old MQTT topics afterward.

`setPortalConfigDeviceManufacturer(value)` and `setPortalConfigDeviceDescription(value)` are dedicated
persistent setters. They preserve an already pending application update and
queue an ArdFS save automatically. Neither requires HA to be enabled; only their
HA discovery metadata is conditional on `ARDPORTAL_ENABLE_HA`. Empty strings
hide their Info rows. There is no dedicated `setDeviceName()` method: use
`setPortalConfig()` for that change. `getPortalConfig()` returns the currently
applied configuration, not a general mutable reference to pending edits.

```cpp
// Register before begin(); apply device metadata after configuration readiness.
portal.onPortalAndAppConfigReady([](bool restored) {
  if (!restored) {
    if (!portal.setPortalConfigDeviceManufacturer("My company"))
      Serial.println("Manufacturer update rejected.");
    if (!portal.setPortalConfigDeviceDescription("Living room sensor"))
      Serial.println("Description update rejected.");
  }
});
portal.onPortalAndAppConfigSaved([](bool ok, const String& error) {
  if (!ok) Serial.println(error);
});
```

A ready callback with `restored == false` also covers storage failure, so setters
can still return `false`. For required updates, retain the requested value and
retry from a later `loop()` iteration when ready/not busy; do not wait in a loop.
Avoid rebuilding a Config from an old snapshot for each successive edit: that
can overwrite other pending portal settings. The two metadata setters explicitly
merge into the pending PortalConfig.

### Public ArdPortal API reference

All methods below belong to `ArdPortal`. Each callback registration replaces the
previous callback of the same kind. Reference arguments in callbacks are valid
only during the callback; copy values you need afterward. Keep callbacks short.

| Method | Contract |
| --- | --- |
| `begin()` / `begin(const Options&)` | Start initialization using default/custom options; returns option-validation/start acceptance, not Wi-Fi/MQTT/storage readiness. Call once from setup. |
| `loop()` | Advance HTTP, Wi-Fi, MQTT, OTA and storage work; call frequently from the Arduino task. |
| `portalAndAppConfigReady()` | Configuration loading has completed; does not guarantee successful restoration or mounted storage. |
| `onPortalAndAppConfigReady(callback)` | One initialization notification: `void(bool restored)`; register before `begin()`. |
| `getPortalConfig()` | `const PortalConfig&` of current portal settings; copy before editing. |
| `validPortalConfig(config)` | Static schema/length/value validation; does not test credentials, connectivity or available storage. |
| `setPortalConfig(config)` | Persistent portal configuration transaction; returns acceptance. |
| `setPortalConfigDeviceDescription(value)` / `setPortalConfigDeviceManufacturer(value)` | Merge and schedule one metadata change into the pending PortalConfig. |
| `onPortalConfigChanged(callback)` | `void(const PortalConfig&, ChangeSource)` after a changed PortalConfig is applied. |
| `onBeforeFactoryReset(callback)` | `void()` once after confirming an eligible factory-reset request, before replacing configuration. Runs even when settings are already default; restart and formatting skip it. |
| `onBeforeRestart(callback)` | `void(RestartReason)` once immediately before an ArdPortal restart; factory reset and storage formatting skip it. |
| `onPortalAndAppConfigSaved(callback)` | `void(bool ok, const String& error)` after storage completion or an identical-save skip. A successful network change can still await application. |
| `getAppConfigValue(key)` | Copy of live RAM state first, then persisted application value, then registered default; missing unregistered key is Undefined. |
| `setAppConfigValue(key, value)` | Persistent application update, except registered fields with `persist:false`, which use RAM state. Rejects invalid/Undefined values, invalid registered values and unavailable transactions. |
| `removeAppConfigValue(key)` | Remove the persisted application key through a save transaction; does not delete its field definition or clear a RAM state override. A registered field falls back to its default when no RAM/persisted value remains. |
| `onAppConfigValueChanged(callback)` | `void(const String& key, const JSONVar& value, ChangeSource)` for changed effective application values, including RAM-only updates. It is not a durable-save notification. |
| `flushPortalAndAppConfig()` | Accept a request to bypass debounce/minimum write spacing for a pending save; no flash I/O in the caller. Returns false when no eligible pending save exists. |
| `portalAndAppConfigBusy()` | Storage initialization/operation, queued save or pending application; useful for deciding when to retry. |
| `portalAndAppConfigStorageOK()` / `portalAndAppConfigError()` | Last portal storage result/error, not a standalone filesystem handle. |
| `startAppConfigPageRegistration(String)` / `startAppConfigPageRegistration(FPSTR(...))` | Queue registration: small pages (≤2048 bytes/8 fields) finish in one loop pass, larger pages cooperatively. `true` means accepted pending work; use state/progress and `onAppConfigPageRegistrationFinished(bool)`. |
| `addAppConfigEntity(String)` / `addAppConfigEntity(FPSTR(...))` | Register one HA-only field using the same definition schema. It has no portal page/menu/control, and shares IDs, commands, state and the 1024-field/configurable definition budgets (256 KiB by default) with panel fields. Requires HA and dynamic AppConfig support; otherwise returns false. |
| `setFieldVisible(id, visible)` | Set a registered control's RAM-only visibility; returns false for null/unknown ID. Available with `ARDPORTAL_ENABLE_VISIBILITY` (default `1`). |
| `setPageVisible(id, visible)` | Set a registered page's RAM-only visibility using its page ID, not its navigation group ID. Groups follow member-page visibility automatically. Combined with field visibility and any `visibleWhen` rules. Same feature flag and return behavior. |
| `setAppConfigStateValue(key, value, publishMqtt=true)` | RAM-only update of a registered nontransient field; validates type/state, triggers change callback and live portal revision when changed; optionally queues MQTT state. |
| `queueAppConfigStatePublish(key)` | Queue the current registered nontransient field state even if unchanged. Works as a dirty flag while disconnected; returns false without MQTT/dynamic pages or for unknown/transient fields. |
| `emitAppConfigEvent(key, value)` | Validate a registered event/device_trigger/tag payload and update its RAM state/revision. With MQTT enabled, requires a connection and free emission FIFO slot to queue a nonretained message; without MQTT it updates local event state only. |
| `onAppConfigCommand(callback)` | `bool(const String& field, const String& command, const JSONVar& value, ChangeSource)` for supported explicit actions. Return whether accepted; report actual hardware state separately. No-op when action support is absent. |
| `wifiConnected()` / `wifiState()` | Station connection boolean / `WifiState`. |
| `mqttConnected()` / `mqttState()` | Broker CONNACK acceptance boolean / `MqttState`; false/Disabled when compiled out. |
| `apActive()` / `localIP()` / `apIP()` | AP active flag, station IP and soft-AP IP. Check connectivity before using an IP. |
| `tlsClockReady()` | Clock has reached the minimum plausible TLS date; not a guarantee of current NTP accuracy. |
| `chipId()` / `defaultDeviceName()` | Static chip identifier and generated `ArdUI-<chip ID>` name. |
| `mqttTopic(kind, command)` | Build an allowed device topic; returns an empty String on invalid arguments or when MQTT is absent. |
| `validMqttTopic(topic, subscription=false)` | Validate the device namespace and final-level wildcard policy; false without MQTT. |
| `publish(topic, payload, retain=false)` | Queue one QoS 0 text packet; true means queued locally, not delivered/acknowledged. |
| `subscribe(topic)` | Queue one subscription; retry busy requests and resubscribe after reconnect. |
| `onMqttMessage(callback)` | `void(const String& topic, const uint8_t* bytes, size_t length)` for received messages; bytes are not NUL-terminated. No-op without MQTT. |
| `log(message)` / `logMessage(message)` | Diagnostic logging to Console → Messages; does not intercept Serial. |
| `mqtt()` | Owned `ArdMqtt&` (also const overload); accessor exists only when MQTT is enabled. |
| `homeAssistant()` | Owned `ArdHomeAssistant&` (also const overload); accessor exists only when HA is enabled. `discoveryConfig(field)` builds discovery JSON; it does not publish it. |
| `ota()` | Owned `ArdOta&` (also const overload); accessor exists only when OTA is enabled. `active()`, `received()`, `expected()` report upload status; there is no public code-level URL updater. |

`ChangeSource` values are `Application`, `Portal`, `Mqtt`, `Zigbee` and `FactoryReset`.
`WifiState` values are `NoCredentials`, `Connecting`, `Connected` and `FallbackAP`.
`MqttState` values are `Disabled`, `WaitingForWifi`, `WaitingRetry`, `Connecting`,
`Connected` and `WaitingForTime`. These enum values are runtime states, not error
codes or elapsed-time promises.

### Registering pages

`startAppConfigPageRegistration()` uses the same public lifecycle for all page sizes.
`true` means the job was accepted; state is `Pending`, and the page is not yet
registered. Validation success/failure is always delivered exactly once through
`onAppConfigPageRegistrationFinished(bool)` from `portal.loop()`, after the start
method returns. `false` means immediate rejection (input/busy/features/resources);
no completion callback is issued for a job that was not accepted.

Definitions up to 2048 bytes with at most 8 fields are completely validated and
registered in one loop pass. Larger definitions/field counts use the cooperative
work budget across passes. Both modes use the same schema, limits, diagnostics,
atomic commit and callback contract. Install the callback before registering,
call `portal.begin()` and keep calling `portal.loop()` to advance pending jobs.

```cpp
portal.onAppConfigPageRegistrationFinished([](bool success) {
  if (success) {
    // The page and its fields are now registered. Restore application hardware
    // only once portalAndAppConfigReady() is also true.
    Serial.println("Page registered");
  } else {
    const auto& error = portal.appConfigRegistrationError();
    Serial.printf("%s: %s: %s\n", error.stage, error.fieldId.c_str(),
                  error.reason.c_str());
  }
  // This callback may queue the next page with startAppConfigPageRegistration().
});
if (!portal.startAppConfigPageRegistration(FPSTR(PAGE_DEFINITION))) {
  Serial.println(portal.appConfigRegistrationError().reason);
}
portal.begin();
```

`appConfigPageRegistrationState()` returns `Idle`, `Pending`, `Succeeded` or
`Failed` from `ArdPortal::AppConfigPageRegistrationState`.
`appConfigPageRegistrationBusy()` reports a pending job.
`appConfigPageRegistrationProcessedFields()` and
`appConfigPageRegistrationTotalFields()` report index-building progress; the total
becomes available after the initial scan. Discovery is checked while building
the index; dependency and command-collision checks can remain after the field
count reaches its total.

Only one page registration can be pending. Queue each next page from the completion callback. A failed registration does not publish a partially registered page. Wait for both registration completion and `portalAndAppConfigReady()` before applying saved values to hardware. Keep PROGMEM definitions immutable and alive for the portal lifetime.

### Page registration diagnostics

`startAppConfigPageRegistration()` returns `false` only when the job cannot be accepted. Inspect the
error after a rejected call and inside `onAppConfigPageRegistrationFinished(false)` for later
validation, memory or Discovery failures:

```cpp
auto reportError = []() {
  const auto& error = portal.appConfigRegistrationError();
  Serial.printf("Registration: stage=%s field=%s reason=%s\n",
                error.stage, error.fieldId.c_str(), error.reason.c_str());
};
portal.onAppConfigPageRegistrationFinished([reportError](bool success) {
  if (!success) reportError();
});
if (!portal.startAppConfigPageRegistration(FPSTR(PAGE_DEFINITION))) reportError();
```

The result contains `stage`, `fieldId` and `reason`. Stages distinguish input,
JSON parsing, page/field validation, descriptor normalization, limits, registry
allocation, serialization, field indexing, command collisions and HA Discovery.
Parser errors include the byte offset and distinguish memory exhaustion from
input/node/depth/string/container limits. Discovery overflow reports payload
size and MQTT capacity. `fieldId` is empty when no field can be identified, for
example a JSON parse error or page allocation failure. A successful page
registration clears the previous error; copy the result if retaining it across
another registration. The getter works even with Console diagnostics disabled.
Failures are also sent to Console > Messages when
`ARDPORTAL_ENABLE_CONSOLE_MESSAGES` is enabled. Rejected pages are rolled back and do not
restart Discovery.

### Home Assistant entities without panel pages

Use `addAppConfigEntity()` for telemetry or controls that belong in Home Assistant
but do not need a dynamic page. Pass a single field object, not a page containing
`fields`. All supported controls use the same validation,
MQTT topic namespace, callbacks, state setters and persistence rules as panel
fields. Entity IDs must be unique across the entire application. HA-only entities
have their own source registry, consume no slots from the configured panel-page limit, and do
not appear in `/api/pages` or the portal menu. Their current values remain available
through `getAppConfigValue()` and the complete `/api/app` values endpoint. The 1024-field and configurable source
budgets cover both registries together. `ARDPORTAL_ENABLE_HA` and
`ARDPORTAL_ENABLE_DYNAMIC_PAGES` must be enabled.

```cpp
const char TEMPERATURE_ENTITY[] PROGMEM = R"JSON({
  "id":"air_temperature", "name":"Air temperature", "type":"text",
  "default":0, "class":"temperature",
  "state_class":"measurement"
})JSON";

// During setup; check registration failures.
if (!portal.addAppConfigEntity(FPSTR(TEMPERATURE_ENTITY)))
  Serial.println("Invalid HA entity");

// After portalAndAppConfigReady(), report sensor readings in RAM.
portal.setAppConfigStateValue("air_temperature", 22.5);
```

For writable entities, register `onAppConfigValueChanged()` or
`onAppConfigCommand()` exactly as for panel controls. Reporting a sensor value
with `setAppConfigStateValue()` does not write flash. See
`examples/HomeAssistantEntities/HomeAssistantEntities.ino` for an HA sensor and
switch with no dynamic panel pages.

### Bounded Home Assistant work

```cpp
ArdPortal::Options options;
options.haWorkBudgetMs = 2;
options.haMaxOperationsPerLoop = 1;
portal.begin(options);
```

`haWorkBudgetMs` limits how long HA may start additional work during a `loop()` call; `haMaxOperationsPerLoop` limits the number of operations. Both must be nonzero. Keep the defaults unless measurements show that your application needs a different balance between HA updates, HTTP and UART responsiveness.

The time budget is cooperative: parsing/normalizing one field, serializing a
payload or a core transport call cannot be interrupted and may exceed it.
Application callbacks and MQTT connection/TLS handshakes have their own timing;
this budget is not a hard deadline for the entire portal loop.

Adding a page does not restart discovery of already registered entities. Broker reconnects and Home Assistant's `online` birth message request rediscovery; periodic discovery is disabled by default. PROGMEM definitions must remain immutable and available for the portal lifetime.

### Persistent settings versus live status

| Goal | API to use | Flash write | MQTT behavior |
| --- | --- | --- | --- |
| Save arbitrary application settings without a form | `setAppConfigValue()` | Yes | No automatic topic for unregistered keys; publish your own payload if needed. |
| Save a registered editable scalar field | `setAppConfigValue()` | Yes | Changed effective state is marked for retained publication. |
| Update a registered field with `persist:false` | `setAppConfigValue()` or `setAppConfigStateValue()` | No | Changed state is queued by default. |
| Report a registered sensor/composite status | `setAppConfigStateValue()` | No, regardless of field persistence | Changed state queued if the third argument is true. |
| Update the portal without an immediate MQTT send | `setAppConfigStateValue(key, value, false)` | No | Reconnect/get and configured periodic snapshots still apply. |
| Send the latest field value on your own schedule | `queueAppConfigStatePublish()` | No | Coalesced retained state, even when unchanged. |
| Send a custom JSON/text message | `publish()` | No | Caller chooses retain; caller retries rejection. |
| Send a transient event | `emitAppConfigEvent()` | No | Nonretained, never periodically replayed. |

For registered scalar fields, `setAppConfigValue()` honors `persist:false`.
Use `setAppConfigStateValue()` for explicitly RAM-only readings regardless of
persistence policy. A RAM override has priority when reading/rendering/publishing a
field. Do not mix a live override with persistent setters for the same key unless
you intend that priority; use separate telemetry and configuration field IDs.
Removing a persisted key does not remove a live override. There is no public
method to remove just one RAM override; reboot clears RAM-only state.

```cpp
// Register a text field with id "temperature" before using these calls.
// Invoke after portalAndAppConfigReady(), from the normal Arduino task.
if (!portal.setAppConfigStateValue("temperature", 22.5, false))
  Serial.println("Invalid or unavailable temperature field.");
// Later, on your own timer; this just marks the field for cooperative sending.
if (!portal.queueAppConfigStatePublish("temperature"))
  Serial.println("MQTT state publication is unavailable.");
```

For a climate-style object, read the current value, change only reported properties
such as `current_temperature` or `action`, and pass the complete object back to
`setAppConfigStateValue()`. Preserve required keys and valid modes. The library renders
and publishes state; application code controls the physical actuator.

## Options and remaining blocking operations

Every `ArdPortal::Options` member is listed here. Pass the object to
`begin(options)`; there is no runtime Options setter. Saved Config values take
precedence over device/AP defaults. Time values are milliseconds unless the
member explicitly ends in `Seconds`.

| Option | Default | Purpose |
| --- | --- | --- |
| `deviceName` | `nullptr` | Startup/default AP and device name; null generates `ArdUI-<chip ID>`. |
| `apPassword` | `"1234567890"` | Default AP password when no saved configuration applies; empty opens the AP. |
| `ntpServer1` | `"pool.ntp.org"` | Required nonempty NTP hostname, up to 253 bytes. |
| `ntpServer2` | `"time.cloudflare.com"` | Optional secondary NTP hostname, up to 253 bytes; empty disables it. |
| `appStateIntervalMs` | `600000` | Retained application state refresh; zero disables periodic snapshots, not change/reconnect/get publication. |
| `appConfigRegistrationWorkBudgetMs` | `2` | Cooperative registration budget in milliseconds, checked between work units; nonzero. Small pages finish in one loop pass. |
| `appConfigRegistrationMaxOperationsPerLoop` | `16` | Maximum registration units per loop; nonzero. |
| `haWorkBudgetMs` | `2` | Cooperative HA budget in milliseconds, checked between work units; nonzero. |
| `haMaxOperationsPerLoop` | `1` | Maximum HA work units per portal loop; nonzero. |
| `discoveryIntervalMs` | `0` | HA discovery refresh; zero disables periodic discovery, not initial/birth/dependency updates. |
| `appConfigSaveDelayMs` | `750` | Debounce/coalesce accepted application changes; zero removes debounce. |
| `appConfigMinWriteIntervalMs` | `5000` | Minimum spacing between debounced application commits; zero removes spacing. Portal saves/flush can bypass it. |
| `wifiTimeoutMs` | `30000` | Saved/requested Wi-Fi connection attempt timeout; must be nonzero. |
| `retryMs` | `10000` | Retry spacing for network state machines; must be nonzero. |
| `mqttTimeoutMs` | `15000` | MQTT connect/protocol-response timeout; must be nonzero. |
| `tcpTimeoutMs` | `250` | Synchronous broker TCP connect timeout where supported by the core; must be nonzero. DNS is separate. |
| `keepAliveSeconds` | `30` | MQTT keepalive; must be nonzero. |
| `tlsHandshakeTimeoutSeconds` | `5` | ESP32 TLS handshake timeout; must be nonzero even if TLS is disabled. ESP8266 uses core BearSSL timeouts. |


The design is cooperative, not hard real-time. It has no `delay()`, `readString()`
or connection-wait loops, but the following core operations can still block:

| Operation | Limit / consequence |
| --- | --- |
| ArdFS mount, open, read/write, sync/close, format | The journal advances through `loop()` in phases with 256-byte logical transfers. ArdFS buffers a document and commits it on flush; raw flash I/O, bank scanning, compaction and formatting are synchronous. Physical flash operations do not guarantee a hard time bound. |
| JSON, CRC32, strings and callbacks | Synchronous RAM/CPU work; slow callbacks or full Serial buffers also delay portal servicing. |
| Broker DNS | Core `hostByName` may take seconds independently of TCP timeout; numeric IPv4 avoids this lookup. |
| MQTT TCP connection | Synchronous core connect, using `tcpTimeoutMs` where available. |
| TLS parsing/handshake/I/O | Core cryptography and waits remain synchronous. ESP32 has a handshake timeout; ESP8266 BearSSL uses internal core timeouts, not an equivalent effective public override. |
| Wi-Fi driver and UDP DNS | Starting/stopping networks and processing UDP packets have core costs. ESP32 3.3.12 uses its built-in AsyncUDP DNS implementation. |
| OTA Update | Flash preparation/writes/final verification are synchronous even though reception is streamed. |

Plain HTTP/MQTT writes on ESP32 use `send(MSG_DONTWAIT)` to avoid the core write
retry loop. ESP8266 checks `availableForWrite`, uses bounded chunks and aborts
interrupted clients; finishing a response uses bounded `flush(1)` / `stop(1)`.
These precautions do not guarantee zero interruption of the processor during
flash programming or TLS.

Wi-Fi equipped ESP8266/ESP32 Arduino boards are supported; chips without a Wi-Fi
radio, such as ESP32-H2/P4 without an external module, cannot provide this portal.
The portal/OTA have no authentication and settings contain plaintext credentials;
use a trusted AP/LAN. Verified MQTT TLS does not add HTTP authentication.

## Registration status accessors

| Method | Input | Return |
| --- | --- | --- |
| `appConfigPageRegistrationState()` | None | `Idle`, `Pending`, `Succeeded` or `Failed` |
| `appConfigPageRegistrationBusy()` | None | Boolean pending-job status |
| `appConfigPageRegistrationProcessedFields()` | None | Number of indexed fields processed |
| `appConfigPageRegistrationTotalFields()` | None | Total fields, available after the structural scan |
| `appConfigRegistrationError()` | None | Const reference with `stage`, `fieldId`, `reason`; copy before the next registration if retaining it |

The rvalue overload `startAppConfigPageRegistration(String&&)` transfers the
source buffer on acceptance. The `const String&` overload copies it. The
`const __FlashStringHelper*` overload borrows an immutable flash source for the
portal lifetime. All page sizes report completion through the same callback.

## Before factory reset

Register this callback before a reset can be requested:

```cpp
portal.onBeforeFactoryReset([]() {
  // Reset application-owned hardware state or mark external data for cleanup.
  // Keep this synchronous handler short; do not wait for portal.loop() here.
});
```

The handler completes before ArdPortal schedules default PortalConfig and an
empty AppConfig for storage. It runs once per eligible, confirmed factory-reset
request, even if the values are already default. Rejected requests (missing
confirmation, initialization or another operation in progress) do not invoke it.
It reports reset intent, not successful persistence: a later storage error can
still prevent the reset. Existing configuration-change callbacks keep their
change-only behavior. Ordinary/OTA restarts and storage formatting do not invoke
this handler, and factory reset still skips `onBeforeRestart`.
Use `portal.onBeforeFactoryReset(nullptr)` to remove the handler.

## Zigbee API

When `ARDPORTAL_ENABLE_ZIGBEE=1`, see [Zigbee network operations and callbacks](https://github.com/rkubera/ArdPortal/wiki/Zigbee#api). Factory reset also clears Zigbee pairing before restart; ordinary restarts and storage formatting preserve it.

## Callback distinction

- `onPortalConfigChanged`: changed network/device settings have been applied.
- `onAppConfigValueChanged`: one effective application value changed; this may be
  an immediate live update and does not prove flash completion.
- `onPortalAndAppConfigSaved`: the storage transaction finished or an identical
  document was skipped; check `ok` and `error`.
- `onPortalAndAppConfigReady`: startup restoration finished; `restored=false`
  includes defaults and storage failure, not just a first boot.
- `onAppConfigCommand`: accept/reject an explicit action; report physical results
  separately with a state setter.
- `onAppConfigPageRegistrationFinished`: an accepted registration finished once;
  immediately rejected jobs do not invoke it.
- `onBeforeFactoryReset`: a confirmed reset is about to replace configuration,
  independently of whether current values differ from defaults.
- `onBeforeRestart`: runs once for manual/OTA restart after prior saves finish;
  factory reset and formatting skip it. A save failure does not cancel restart.

Register callbacks before the operation they observe. Registrations replace the
previous handler of that kind, and callback references are borrowed only for the
invocation. Do not retain a pointer to incoming MQTT bytes.

---

[Back to documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

Native Zigbee control commands use `ChangeSource::Zigbee` in the same AppConfig
value/action callbacks. Register initial pages before Zigbee starts; see
[automatic native controls](https://github.com/rkubera/ArdPortal/wiki/Zigbee#automatic-native-controls).

### Access point band

`PortalConfig::apBand` is **24** (2.4 GHz, default) or **5** (5 GHz).
Dual-band chips expose this preference in AP Setup and save it with PortalConfig.
A disconnected AP starts on channel 1 or 36 respectively. When STA is connected,
AP and STA share its channel, so the router determines the active band.
The portal explains this beside the selector. Single-band boards hide and disable
the selector and reject a 5 GHz request through the HTTP settings API.
The config API returns `apBand` and the read-only capability `apDualBand`.

## Network control without using WWW

WWW is enabled by default; `ARDPORTAL_ENABLE_WWW=0` disables its server without
disabling the C++ API. `ARDPORTAL_ENABLE_WIFI=0` disables Wi-Fi/AP and forces
WWW/MQTT off; Wi-Fi/AP and MQTT public methods/types are then omitted. See
[Defines](https://github.com/rkubera/ArdPortal/wiki/Defines#www-and-wi-fi-combinations).
All methods operate on the same state used by its status
view. Persisted settings changed through `setPortalConfig()` reload configuration
forms on the next status refresh. Normal network operations require `begin()`,
configuration readiness and continued calls to `loop()`. Call these APIs from
application/loop context, not interrupt or native stack callbacks.

| Wi-Fi/AP method | Behavior |
|---|---|
| `connectWifiStation()` | Start association with saved credentials and enable retries; returns false if not ready/busy or credentials are missing. |
| `disconnectWifiStation()` | Disconnect STA and suspend retries without erasing credentials; MQTT transport closes too. |
| `setWifiAutoReconnect(bool)`, `wifiAutoReconnect()` | Set/read runtime retry policy. Disabling retries does not immediately disconnect an existing association. |
| `onWifiStateChanged(callback)` | Deferred `WifiState` notifications from `loop()`. |
| `scanWifiNetworks()`, `wifiScanBusy()` | Queue/check an asynchronous scan; an active association attempt delays scanning. |
| `wifiNetworksJson()` | Borrowed last scan array with `ssid`, `rssi`, `channel`, `secured`. |
| `onWifiScanFinished(callback)` | Receives `(bool success, const String& results)` once per completed scan, including scans requested in WWW. |
| `setApPolicy(policy)`, `apPolicy()` | `Automatic` uses fallback behavior; `AlwaysOn` keeps AP active; `Disabled` stops AP and prevents fallback. |
| `startAccessPoint()`, `stopAccessPoint()` | Shortcuts for `AlwaysOn` and `Disabled`. |
| `onApStateChanged(callback)` | Deferred `(bool active)` notification, including automatic AP changes. |

SSID/password, AP password and preferred AP band remain persistent settings in
`PortalConfig`. AP SSID is derived from `deviceName`; changing `apName` alone does
not override this rule. On dual-band chips AP and STA share the radio channel;
an STA band conflict can still close AP. Runtime connection/AP policies are not
persisted across reboot. Policy/operation setters do not erase saved settings.
A `true` return means accepted (or AP active), not successful association.

```cpp
portal.onPortalAndAppConfigReady([](bool ok) {
  if (!ok) return;
  auto config = portal.getPortalConfig();
  config.ssid = "MyNetwork";
  config.password = "MyPassword";
  config.deviceName = "My device"; // Also determines AP SSID.
  config.apPassword = "setup-password";
  config.apBand = 24; // 5 only on supported dual-band chips.
  portal.setPortalConfig(config); // Save, then apply and connect automatically.
});
portal.onWifiStateChanged([](ArdPortal::WifiState state) {
  // Update application connection status; do not block.
});
portal.onWifiScanFinished([](bool success, const String& networks) {
  // Copy networks if needed after this callback returns.
});
```

### MQTT connection API

Available when `ARDPORTAL_ENABLE_MQTT=1`, through `portal.mqtt()`:

| Method | Behavior |
|---|---|
| `connect()`, `disconnect()` | Resume connection/retries or close transport and suspend retries. |
| `setAutoReconnect(bool)`, `autoReconnect()` | Runtime retry permission; disabling is applied on the next MQTT loop. |
| `onStateChanged(callback)` | Deferred `MqttState` notifications from portal `loop()`. |
| `unsubscribe(topic)` | Queue MQTT UNSUBSCRIBE and track matching UNSUBACK; returns false when disconnected, busy or invalid. Uses the same filter validation as `subscribe()`. |
| `testConnection(config)` | Test candidate MQTT settings using the WWW trial mechanism: persist on success, restore old settings on failure. Wi-Fi credentials must match the current configuration. |
| `connectionTestBusy()` | Trial or its persistent save is pending. |
| `connectionTestResult()`, `connectionTestMessage()` | Last result: 0 pending, 1 success, 2 failure, and explanation. |
| `onConnectionTestFinished(callback)` | Deferred `(bool success, const String& message)` after trial/save completion. |

`publish()`, `subscribe()` and `onMessage()` remain available. Connect/test calls
return acceptance, not broker acknowledgement. Connection testing requires active
Wi-Fi and completed storage initialization. It enables MQTT retries. TLS requires
TLS support compiled in and a valid candidate certificate. Always copy
`getPortalConfig()` before replacing host/port/user/password/TLS settings so other
settings are preserved. Continue `loop()` to complete trials and saves.

### Zigbee status and policy API

When Zigbee is enabled, `portal.zigbee()` exposes the owned Zigbee component:
`state()`, `busy()`, `pairing()`, `hasNetwork()`, `nativeReady()`, `channel()`,
`pan()`, `attempts()` and `error()`. No second stack instance is created.

`setAutoConnect(bool)` and `autoConnect()` control automatic pairing/rejoin
retries at runtime. Disabling retries does not cancel an active operation:
call `stopZigbeePairing()` to stop it too. Re-enabling schedules connection when
disconnected. Existing `startZigbeePairing()`, `stopZigbeePairing()`,
`rejoinZigbee()`, `resetZigbeePairing()`, `setZigbeeOptions()`,
`onZigbeeStateChanged()` and `onZigbeeOperationFinished()` remain the convenient
portal API. Status changes are also visible in WWW.


## Application firmware version

Set the application version before `begin()`. The default is `1.0`.

```cpp
ArdPortal portal;

void setup() {
  portal.setFirmwareVersion("2.3.4");
  portal.begin();
}
```

The Device information view and `GET /api/info` expose this value as `firmwareVersion`, including when HA and Zigbee are disabled.

- `bool setFirmwareVersion(const char* version)` copies a nonempty version of up to 16 UTF-8 bytes. It returns `false` for null/empty/oversized input or after startup, preserving the previous version.
- `const char* getFirmwareVersion() const` returns the current version; the returned text belongs to the portal.

Home Assistant MQTT Discovery includes this value as device `sw_version`. Zigbee exposes it through the standard Basic cluster `SWBuildID` (`0x4000`) on the management and application endpoints. Existing Zigbee devices may need a new interview/read to refresh cached software metadata. The version is firmware metadata, not a saved user setting; assign it in each firmware build. Changing it does not change the control-layout model ID. This display string is independent of the integer `ARDPORTAL_ZIGBEE_OTA_FILE_VERSION` used to order Zigbee OTA images; set that macro separately for OTA releases.

## Suspending portal networking

**Available since 0.9.0.**

| Method | Return value |
| --- | --- |
| `bool suspendPortal()` | `true` after shutdown, or when already suspended; `false` if startup/busy guards or Wi-Fi shutdown prevent it. Always `false` in Matter-over-Wi-Fi builds. |
| `bool resumePortal()` | `true` when restoration starts; when already active, returns configuration readiness. `false` while restart/OTA/configuration work or driver startup prevents restoration. |
| `bool portalSuspended() const` | Explicit suspension flag; not a Wi-Fi connectivity or sleep-state test. |

With `ARDPORTAL_ENABLE_WIFI=1`, call `portal.suspendPortal()` from the application
loop to turn Wi-Fi off and close HTTP, WebSocket, DNS and MQTT connections.
Zigbee and Matter over Thread keep running. Continue calling `portal.loop()`.
`portal.portalSuspended()` reports the explicit suspension state.

`portal.resumePortal()` restarts Wi-Fi and the HTTP listener, restoring the AP
policy and automatic station reconnect policy saved at suspension. Saved network
credentials are retained. With the default automatic AP policy, saved station
credentials are tried first and the fallback AP starts on failure; without
credentials the AP starts immediately. For a button that should always expose
an AP, set `portal.startAccessPoint()` after a successful resume.

Repeated successful calls are safe. Resume does not wait for Wi-Fi or MQTT to reconnect, and it does not restore old browser socket sessions; clients must reconnect. Suspension returns false
before configuration is ready, during configuration writes, OTA, reboot, MQTT
connection trials or queued/running Wi-Fi scans. It is unavailable for Matter
over Wi-Fi because Wi-Fi carries Matter traffic. Network-changing API calls
return false while suspended. Calls belong in `loop()`, never a GPIO ISR.
Suspension itself does not select a Zigbee sleepy role or Thread ICD profile.

See [PortalStandby](https://github.com/rkubera/ArdPortal/blob/matter/examples/PortalStandby/PortalStandby.ino) for Zigbee
connection detection, a debounced button and a two-minute portal window. For
Matter over Thread, use `portal.matter().commissioned()` together with
`portal.matter().connected()` as the readiness condition.

Suspension does not erase settings, release all compiled networking buffers, or
reduce firmware size. It keeps application processing and enabled mesh transports
running. Use feature defines for a permanently smaller build. For a compiled-out
Wi-Fi build these three methods are not exposed.

```cpp
// In loop(), after portal.loop(); retry on a later loop when false.
if (wantPortalOff && !portal.portalSuspended()) {
  portal.suspendPortal();
} else if (!wantPortalOff && portal.portalSuspended()) {
  portal.resumePortal();
}
```

Here `wantPortalOff` is an application-owned flag, for example from a debounced
button. Resume needs a local trigger while Wi-Fi is off; a web request cannot
reach the suspended portal. See [power management](Power-Management#portal-network-suspension).

## Configuration persistence

### Application edits during a save

`setAppConfigValue()` and `removeAppConfigValue()` accept valid application edits
while another configuration save is active. They return `true` after retaining the
edit in RAM. A separate bounded patch stores the latest intent per key; it never
changes the snapshot currently being written. Subsequent `portal.loop()` calls save
that patch in the next transaction. Edits to different keys are preserved, and the
latest accepted edit to the same key wins.

`portalAndAppConfigBusy()` includes these waiting edits. Calling
`flushPortalAndAppConfig()` requests their next save without the normal debounce;
manual/OTA restart draining also waits for them. Each completed transaction still
reports its result through `onPortalAndAppConfigSaved()`. `true` is acceptance in
RAM, not synchronous confirmation of flash persistence. Invalid values, memory or
size limits, unavailable storage and incompatible lifecycle operations can still
return `false`. Power loss before the queued transaction commits can lose RAM edits.

### Failed writes and retry limit

An ordinary configuration batch gets at most **three total attempts** (the first
write plus two retries). A failed batch remains intact in RAM. Retries wait at
least 1000 ms after the preceding failure; explicit flush and restart draining do
not bypass that delay. New application edits remain in their separate queue,
including newer values of the same key. They form the next batch and have their
own attempt budget. No failed older batch is resubmitted after a newer batch.

`portalAndAppConfigSaveAttempts()` returns the attempts made for the active or
most recently completed batch (0 initially, then 1–3). The
`onPortalAndAppConfigSaved()` callback receives the outcome of every attempt.
After the third failure, retrying that batch stops and
`onAppConfigValueSaveFailed()` reports each application field in its failed
snapshot, including its attempted value and the last error:

```cpp
portal.onAppConfigValueSaveFailed([](const String& id,
    const ArdJSON::JSONVar& value, uint8_t attempts, const String& error) {
  Serial.printf("Field %s failed after %u attempts: %s\n",
                id.c_str(), unsigned(attempts), error.c_str());
  // value.isUndefined() means that deleting this key failed.
  // Copy any data needed beyond this callback; references are borrowed.
});
```

A full application snapshot reports all fields it contains, plus deleted keys;
a patch reports only its affected keys. Portal/network-only failures still use
the ordinary configuration callback. A newer value of a reported key may still be
waiting in the next batch. After exhaustion, a new accepted edit creates a new
batch; it does not automatically resubmit the exhausted one.

The failed snapshot is retained without copying it on the error path.
Already accepted later edits survive both retries and exhaustion. Power loss can
still discard all changes awaiting a successful commit in RAM. Failed MQTT
connection-trial saves keep their deliberate rollback behavior instead of
retrying settings that the trial rejected.

### Startup redundancy repair

Portal startup calls `readJsonConfig(path, sink, callback, true)` on its selected storage backend to validate both copies and
repair a missing or invalid alternate before marking configuration ready. Two
valid copies do not need equal generations or contents: the older valid copy is
normal journal history and is left untouched. Repair copies the surviving
verified physical record, preserving its generation and encoding, and checks all
written bytes against the source checksum before publishing the record commit.
It never writes the surviving copy. Interrupted repair can be attempted again at the next boot.

Repair uses at most three attempts with one-second failure backoff. If all fail,
the surviving document is still delivered to the sink, followed by `ok=false`
and an error. Portal can restore it for operation, reports unsuccessful storage status,
and refuses ordinary persistent changes until a later successful startup repair.
Explicit factory reset/storage format remain available as deliberate recovery
operations. If neither copy is valid, repair cannot reconstruct a document.

`Result::redundant` reports validated redundancy for a read;
`Result::repaired` indicates successful repair and `Result::repairAttempts` records
its attempt count. Standalone `readJsonConfig(path, sink, callback)` defaults to read-only;
pass `true` as the fourth argument to request repair. Startup repair uses the read
completion/ready path, not the per-value failure callback for a new configuration
batch. The storage error/status API exposes a failed repair.
