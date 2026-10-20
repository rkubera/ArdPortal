# Changelog

## Unreleased — Firmware version in device information

- Show the application firmware version in the Device information view and the `firmwareVersion` field of `/api/info`, using `setFirmwareVersion()` or the default `1.0`.

## Unreleased — Startup copy repair

- Validate both configuration copies at startup and repair a missing or invalid alternate from the surviving verified document before accepting persistent edits.
- Stream and verify the replacement without rewriting the good copy; leave two already valid copies untouched, including copies with different generations.
- Limit repair to three attempts with one-second failure backoff. If repair exhausts its attempts, restore the surviving configuration for operation but block ordinary persistent edits until a successful startup repair or explicit recovery.
- Cover either damaged/missing slot, healthy-copy preservation, repair failures and interrupted repair with torn raw flash operations.

## Unreleased — Bounded save retries

- Retain failed configuration batches for up to three total attempts, with at least one second between failed attempts, including during explicit flush/restart draining.
- Keep later application edits in their own queue and retain MQTT acknowledgement state until the batch succeeds or exhausts its attempts.
- Expose `portalAndAppConfigSaveAttempts()` and `onAppConfigValueSaveFailed()` with key, attempted value, attempt count and error after terminal failure. Existing save callbacks continue reporting each attempt.

## Unreleased — Application edit queue

- Accept valid application edits during an active configuration save using a separate coalescing patch, preserving the immutable streaming snapshot.
- Include waiting edits in storage busy status, explicit flush and restart draining; retain the latest accepted value per key, including deletions.
- Keep `true` as acceptance in RAM, with persistent completion reported by the existing save callback.

## Unreleased — Configuration recovery

- Maintain an erased end marker when appending to reused flash banks so older committed records cannot override newer configuration after restart.
- Report allocation failures while validating configuration journals instead of silently selecting an older valid copy.
- Keep the newest committed flash bank authoritative when scanning fails; do not automatically format on allocation, flash I/O, index capacity or layout errors.
- Cover either journal slot, generation rollover, corrupted-copy recovery, failed mount preservation and rapid multi-key edits across restart.

## Unreleased — Zigbee2MQTT Home Assistant discovery

- Preserve expose endpoint identity and explicit registered names so repeated controls have unique Home Assistant Discovery IDs instead of colliding or falling back to the device name.
- Read initial thermostat values without configuring reporting for SDK-unreportable setpoints, preventing a thermostat configuration failure from skipping subsequent controls after interview/re-pairing.
- Exercise multiple same-type controls against the actual Zigbee2MQTT Discovery mapper and verify thermostat initial reads against a reporting-rejection fixture.

## Unreleased — Application firmware version

- Add `setFirmwareVersion()` and `getFirmwareVersion()` with a default application version of `1.0`; configure before `begin()` using up to 16 UTF-8 bytes.
- Include application version in Home Assistant device `sw_version` and standard Zigbee Basic `SWBuildID` on management/application endpoints, independently of the OTA image version and layout fingerprint.

## Unreleased — Zigbee hardware validation

- Prevent a false Zigbee OTA timeout when the SDK task updates its activity timestamp after the caller sampled the loop time; test concurrent tick ordering and millis rollover.

- Size the native Zigbee scheduler before stack initialization according to staged attributes, retaining commissioning capacity when many reporting endpoints are registered.
- Keep inactive single-target thermostat setpoints outside the active target range so SDK deadband validation does not reject large remote target changes. Independent `heat_cool` thresholds retain their deadband validation.
- Cover reporting scheduler capacity and remote thermostat writes with native adapter regression tests.

## Unreleased — Conditional UI and typed text

- Exclude Zigbee refresh/navigation logic and disabled Console channels from generated WWW JavaScript using existing feature switches.
- Add optional `ARDPORTAL_ENABLE_TEXT_TYPES` (default enabled) to omit numeric text subtypes and automatic HA patterns while retaining plain text. Disabled numeric subtype definitions are rejected explicitly.
- Add borrowed `JSONVar::stringValue()` access and avoid copying subtype names and text readings during validation.
- Validate typed text on/off, HA defaults, native Zigbee mappings and generated frontend syntax.


## Unreleased — Compile-time memory trimming

- Omit HTTP server/client, response/asset state and dynamic HTTP buffers when WWW is disabled.
- Omit captive DNS without WWW while retaining AP and programmatic Wi-Fi support.
- Remove MQTT Console history when MQTT is disabled; retain independent Messages history.
- Use AP credentials directly from PortalConfig instead of retaining duplicate strings, and omit NTP server copies without Wi-Fi.
- Exercise persistent storage with WWW/Wi-Fi disabled and Messages enabled without MQTT.


## Unreleased — Headless OTA and ESP32-H2

- Remove HTTP OTA whenever WWW is disabled while retaining independently enabled Zigbee OTA.
- Support native Zigbee on ESP32-H2 and automatically disable Wi-Fi/AP, WWW and MQTT on this target.


## Unreleased — Converter export without WWW

- Add asynchronous callback-based Zigbee2MQTT/ZHA exports, readiness/busy status and cancellation, with bounded 256-byte output and retry/backpressure handling.
- Gate HTTP download routes/state/methods on WWW while retaining layout identity and format-specific API exports.
- Test streamed file syntax, repeat-safe backpressure, memory retry, cancellation, layout changes and stalled output.


## Unreleased — Conditional public API

- Omit disabled Wi-Fi/AP, MQTT, dynamic registration, HA entity, action/event and diagnostic APIs from the public interface.
- Remove disabled registration stubs and unused network callback/registration metadata storage.
- Retain shared storage/configuration APIs and necessary private status/log helpers.
- Add compile-time API availability probes for enabled and disabled feature combinations.


## Unreleased — WWW and Wi-Fi feature switches

- Add enabled-by-default `ARDPORTAL_ENABLE_WWW` and `ARDPORTAL_ENABLE_WIFI`.
- WWW disabled stops HTTP and WebSockets while retaining programmatic network control.
- Wi-Fi disabled stops STA/AP and forces WWW/MQTT/TLS/HA off; local storage and Zigbee continue.


## Unreleased — Programmatic network control

- Add Wi-Fi connect/disconnect, retry policies, asynchronous scans and state/scan callbacks.
- Add AP lifetime policies and start/stop notifications without changing persistent AP configuration.
- Add MQTT connection control, unsubscribe acknowledgements, connection tests and callbacks.
- Expose Zigbee status and automatic retry policy through the owned component.
- Refresh WWW configuration forms after API configuration changes; WWW stays enabled.


## Unreleased — Zigbee clock synchronization

- Add a background Time client that probes the coordinator for valid UTC/time status, updates the system clock and periodically retries.
- Keep time synchronization separate from AppConfig controls and converter exports.


## Unreleased — Zigbee dependency handling

- Accept Zigbee commands for fields hidden by field/page dependencies, including compound controls and actions.
- Keep value validation and read-only restrictions; Portal and MQTT visibility checks remain unchanged.
- Document that dependencies do not affect Zigbee control visibility.

## Unreleased — Zigbee2MQTT action choices

- Group action endpoints of the same AppConfig control into one immediate-action choice.
- Route each choice to its original endpoint and clear the transient selection after sending, allowing repeated actions without an Execute button.
- Preserve standalone actions and the existing device layout signature.

## Unreleased — Zigbee2MQTT control labels

- Hide endpoint annotations in exported features while retaining unique MQTT properties and endpoint command routing.
- Clear action selection using the unsuffixed converter key, avoiding a duplicate endpoint suffix in Z2M optimistic state.

- Apply AppConfig names to standard-device child features and all measurement labels.
- Keep climate as one expose and document frontend property rendering and converter replacement.

## Unreleased — ZHA quirk export

- Add enabled-by-default `ARDPORTAL_ENABLE_ZHA_QUIRK` and a separate Zigbee-page Python download button.
- Share metadata, streamed export and layout identity with the independent Zigbee2MQTT exporter.
- Generate named Multistate entities, number ranges, measurements, action buttons and native entity labels/thermostat limits.
- Document installation, supported ZHA API versions and composite/native mapping limits.

## Unreleased — Zigbee memory optimization

- Replace persistent endpoint label Strings with startup-only ZCL buffers and compact report flags.
- Normalize Zigbee composite definitions once instead of deep-copying them during registration and command handling.
- Reuse diagnostic history buffers and avoid temporary traffic/timestamp Strings.
- Compact the shared converter asset without changing the supported control set.
- Derive converter labels from definitions after SDK startup, preserving the layout identity across downloads.

## Unreleased — Zigbee2MQTT converter export

- Add enabled-by-default `ARDPORTAL_ENABLE_ZIGBEE_CONVERTER` and a Zigbee-page `.mjs` download.
- Generate converters cooperatively from registered endpoints without buffering the complete file.
- Identify devices by exported control layout rather than IEEE address; identical layouts share one converter.
- Export named Multistate choices/status, configured climate ranges/modes, measurements and scoped endpoint commands.
- Document external converter setup and limits; enable the option in Zigbee examples.

## Unreleased

### Added

- Simplify the Zigbee network page to saved-network Rejoin and Reset pairing actions; pairing starts automatically and retries until connected. Scope network details/messages to the Zigbee section.

- Show the current Zigbee connection status beside Wi-Fi in the shared header on every portal page, including builds with MQTT disabled; remove its duplicate state row and connection banner from the Zigbee page.

- Zigbee Console displays JSON payloads for incoming APS metadata, decoded ZCL responses/attributes/control commands, applied commands and queued TX reports. Remove raw hexadecimal previews, preserve transaction sequences, redact lock PINs and explicitly identify unsupported/truncated formats without allocating a JSON tree.

- AP Setup offers a persistent 2.4/5 GHz band preference on dual-band Wi-Fi chips; single-band boards keep the existing setup. AP follows the station channel while connected.

- Native Zigbee light supports standard XY color attributes and commands, with sRGB conversion alongside Hue/Saturation and Enhanced Hue.

- Optional `ARDPORTAL_MQTT_DEBUG_SERIAL` mirrors MQTT lifecycle and RX/TX diagnostics to Serial, independently of Console and Messages.

- Optional `ARDPORTAL_ZIGBEE_DEBUG_SERIAL` mirrors Zigbee diagnostics to sketch-owned Serial without waiting for transmit buffer space.

### Fixed

- Report missing or rejecting application action callbacks explicitly in Portal command errors instead of misclassifying them as invalid field values/definitions.

- Forward Zigbee pairing/rejoin attempts, successful connection, link loss, retries and operation failures to Console Messages when enabled; keep frame payload traffic in the dedicated Zigbee history without replacing application callbacks.

- Place Zigbee connection status after IP. Zigbee logs use the same UTC timestamps and browser-local date/time formatting as Messages, falling back to uptime until the device clock is valid.

- Automatically retry Zigbee pairing or saved-network rejoin after each unsuccessful series, with a 30-second pause and no overall retry limit; Stop pairing also cancels scheduled retries. Preserve network identity during recovery and keep HTTP processing cooperative.

- Pace native Zigbee reports at one attribute per step, at least 50 ms apart; retain unsent attributes without treating normal progress as an error. Enhanced Hue metadata remains readable without extra unsolicited reports.

- Support standard Zigbee Enhanced Hue/Saturation commands used by Zigbee2MQTT, including 16-bit hue attributes and RGB conversion.

- Normalize composite control definitions before decoding incoming Zigbee commands; retain queued commands while field parsing or normalization lacks memory.

- Schedule Zigbee state reports fairly per endpoint; failures and retry delays no longer block unrelated controls or replay completed composite endpoints.

- Explicitly enable RX-on-when-idle for Zigbee end devices so commands can arrive without waiting for a periodic parent poll.

- Zigbee retries only failed attributes, respects SDK reporting permissions, and updates existing light attributes before adding missing optional attributes, preventing repeated light report bursts.
- Zigbee startup and timeout checks tolerate work started later in the same portal loop.
- Explicitly enable Wi-Fi/IEEE 802.15.4 radio coexistence before starting the native Zigbee stack; report setup failure through lifecycle diagnostics.
- Show Wi-Fi/AP disconnection guidance before submitting credentials, retain it across connection loss, display scan bands and link to the station address when received.
- Keep Console available without MQTT; show Zigbee logs in an independent Console tab and place Zigbee navigation after AP Setup. Enable Messages and disable MQTT/HA in Zigbee examples.
- Correct Analog Output minimum/maximum attribute IDs, register optional Zigbee ProductLabel through the generic typed attribute API, and report endpoint/attribute initialization failures precisely. Keep the Zigbee network page inside the shared portal navigation.
- Add the required page order to ZigbeePortal so its demonstration controls register successfully.
- Retire the fallback AP on ESP32-C5 after a 5 GHz station connection even when stale AP clients or HTTP requests remain; use the station channel when starting an AP while connected.

### Added
- Native Zigbee support for ESP32-C5 alongside ESP32-C6, with chip-specific model identity and shared examples/partition layout.
- Decompose fan, valve, humidifier, water heater, alarm panel, vacuum, mower, button and scene into separate standard Zigbee endpoints without supplied converters; add read-only home/not-home tracker status. Route projected commands to the original controls, label endpoint functions, and stage/report only one endpoint per loop.
- Document HA/Zigbee support and actual limitations on all 30 control pages, including unimplemented richer mappings and portal page/order limitations.
- Optional `textType` on text controls: plain text by default (no Zigbee endpoint), generic number and eleven standard measurement subtypes. Automatically apply HA sensor patterns and native Zigbee unit/scaling/unknown-value mappings.
- Optional ESP32-C6 Zigbee network management (`ARDPORTAL_ENABLE_ZIGBEE`, default off), with End Device/Router role selected by `ARDPORTAL_ZIGBEE_ROUTER` and a matching Arduino board mode.
- Automatic boot pairing or saved-identity rejoin, bounded operation timeouts/retries, start/stop pairing, Zigbee-only reset-and-pair, and loop-delivered state/result callbacks.
- Zigbee page and dedicated `/api/zigbee/console` WebSocket/history, independent of MQTT, HA, Messages and AppConfig sockets. Report stack signals and incoming APS metadata/payload prefixes without replacing the stack's TX confirmation handler.
- ZigbeePortal example with a 4 MiB partition layout containing two 1792 KiB OTA slots, 320 KiB ArdFS and separate Zigbee partitions.

- Automatically map native AppConfig controls to standard Zigbee endpoints: switch, slider, select, numeric/binary sensors, RGB light, thermostat, cover, lock and IAS siren. Share callbacks, effective state and `persist` behavior with existing portal/application paths.
- Add a standard Zigbee OTA client for one `update` field, configurable image identity/version, split-header parsing, validation, failure/timeout handling and portal-managed restart.

### Changed
- Full portal factory reset also clears Zigbee network state before restart; ordinary restart, OTA and storage formatting preserve pairing.

- Freeze native endpoints after initial asynchronous page registration; preserve network reserves, bound loop work and free copied startup buffers. Report endpoint/command/attribute diagnostics in the dedicated Zigbee console.

### Validation
- Host tests cover lifecycle/allocation failures, native cluster construction, typed commands/queue saturation, units/RGB/position conversions, persistence and OOM recovery, attribute-report retries, and OTA length/flash/abort/timeout/deferred-restart handling. Dedicated frontend socket/switch and factory-reset tests also pass. Physical coordinator interoperability still needs hardware validation.

## 0.7.8 — 2026-10-11

### Added
- Add `onBeforeFactoryReset(void())`, called once for a confirmed, eligible factory-reset request before scheduling default configuration. Invoke it even when current settings already match defaults. The handler completes synchronously; ordinary/OTA restarts and storage formatting skip it. Configuration-change callbacks remain change-only.
- Publish comprehensive English GitHub Wiki documentation for Portal, ArdFS and ArdJSON APIs, feature defines, all 30 controls, dependencies, MQTT and Home Assistant, with screenshots and minimal/full JSON examples.
- Add a complete two-page example showing sequential asynchronous registration, immediate and deferred errors, and configuration readiness before applying startup values.

### Fixed
- Hide the repeated label before the light color picker by default. Optional `colorLabel` and localized `colorLabels` supply a dedicated portal label without changing the HA entity name.
- Remove misleading-indentation compiler warnings in JSON object-patch serialization and the streaming journal reader without changing behavior.

### Documentation and validation
- Shorten README to installation, a portal preview, a minimal sketch and links to the Wiki.
- Add host-side factory-reset callback regression coverage for ordering, request guards, unchanged-default notification, missing handlers and save rejection.
- Verify generated portal assets, run storage/topic/HTTP/runtime regression suites, and compile DynamicPages for ESP8266 and ESP32.

## 0.7.7 — 2026-10-09

### Added
- Resolve `$device` in HA topic overrides from the current normalized MQTT device name. RAM and flash page sources stay immutable, and registration, discovery and HTTP use the same resolved topics.
- Nested `and`/`or` groups in field and page `visibleWhen` conditions, while retaining single-field leaves and optional `property` comparisons. Conditions support six nested groups and 64 total nodes.
- Shared Boolean evaluation for portal visibility, HTTP/MQTT command checks and incremental HA discovery; page and field conditions remain combined with AND.

### Fixed
- Default HA switch icons to `mdi:toggle-switch`, preserving explicitly configured icons and `optimistic` settings; do not add an explicit optimistic-mode option.
- Allow `.gz` firmware files in the Upload picker using the final extension and both common gzip MIME types, avoiding browsers that reject the compound `.bin.gz` filter.
- Stage only modified persistent application fields for Portal, MQTT/HA and application setters instead of cloning the complete saved configuration. Merge staged deletions and edits; preserve full replacement APIs and change callbacks.
- Save generated configuration JSON through bounded ArdFS journal chunks (at most 256 bytes) without a whole-document serialization, journal DOM or full old-envelope buffer for canonical records. Retain the existing two-slot format, generation/CRC validation, unchanged-save suppression and readback verification. Legacy/noncanonical records retain the compatibility reader; initial loading still uses the full-document reader.
- Add ASan/UBSan regression coverage for patch capacity/allocation refusal, Portal/MQTT saves, restart restoration, bounded buffers, corrupt/write-failed records and 152 raw-flash power-cut points: `bash tests/run-config-memory.sh`.
- Report the ESP8266 DRAM heap arena size from the core allocator configuration; keep free heap, minimum sampled free heap and largest block separate from total RAM.
- Distinguish aggregate field working memory from the largest individual buffer in registration, HTTP, dependencies and HA discovery. Estimate basic definition parsing from JSON node counts; retain conservative fallback estimates for unknown/extended parser work. Network reserve amounts are unchanged.
- Stream validated basic and extended field definitions without a complete DOM, preserving topic expansion and registration's extended/persist flags. Resolve explicit basic defaults and HTTP visibility rules without parsing unrelated HA metadata. Tests: `tests/run-topic-templates.sh`, `python3 tests/page-stream-test.py`.
- Release consumed HTTP fragments before checking the next allocation, drain pending fragments without a new working-set reservation, and release WebSocket frame buffers after transmission.
- Reclaim older Console history under dynamic HTTP memory pressure while retaining the newest MQTT record and three newest diagnostic messages. Reserve memory for the unscoped combined app snapshot before copying its members. Test: `python3 tests/http-memory-test.py`.
- Avoid allocating runtime state members for values already supplied by saved configuration or field defaults. Preserve first readings and real change notifications; reject serialization failures instead of treating them as equality. Regression test: `python3 tests/runtime-state-test.py`.
- Honor explicit `persist:false` for basic form submissions, MQTT state commands and `setAppConfigValue()`, retaining volatile values only in RAM. Mixed forms save only persistent fields; omitted persistence keeps existing defaults.
- Dynamic HTTP admission reserves the largest field working set before sending success headers. Streamed definitions use connection-close framing so topic expansion and identity changes cannot leave a stale Content-Length. Retryable fragment failures roll back stream cursors instead of skipping fields or being mistaken for end-of-stream.
- Measure ESP32 byte-addressable internal heap with INTERNAL|8BIT capabilities, including free/minimum/largest blocks; expose DMA separately in Info. Use ESP8266 free/largest-block measurements on that platform.
- Preserve configurable network headroom before cooperative registration, dependency/Discovery and dynamic HTTP work, with HTTP 503 for new requests under pressure. Add heap8/DMA registration diagnostics and a pressure-test example; store page conditions as compact JSON instead of permanent DOM copies.
- Use class-local malloc/free allocation for JSON members and library-owned nothrow objects/arrays, returning errors on allocation refusal instead of relying on throwing global allocation paths. Add an ESP32 allocation-refusal/HTTP/UART diagnostic sketch.
- Avoid deep-copying basic HA discovery definitions; normalize copies only for extended controls. Retry failed discovery and dependency work after one second without treating memory failure as hidden visibility.
- Build a sparse reverse dependency index during atomic registration; invalidate only direct/transitive dependents on actual value changes, preserving HA traversal during unrelated telemetry or publication requests.

### Memory and firmware
- Grow dependency traversal buffers according to actual graph depth instead of allocating one frame for every field; reuse two registration masks and share Boolean/schema traversal code without allocating callback wrappers.
- ESP8266 DynamicPagesWithDependencies comparison: firmware 554848 → 554000 bytes (848 bytes saved); initial traversal buffer for 1024 fields 4096 → 32 bytes for short chains.

### Changed
- Add English documentation for functions and parameters across the library and examples.
- Reject dependency cycles during atomic page registration instead of accepting permanently hidden cyclic definitions. Cycle checks run within the registration work budget.
- Document grouped conditions in README and demonstrate them in DynamicPagesWithDependencies.


## 0.7.6 — 2026-10-07

### Breaking changes
- Replace `addAppConfigPage()` with `startAppConfigPageRegistration()`. `true` means accepted pending work; validation results are delivered once from `loop()` through `onAppConfigPageRegistrationFinished(bool)`. Only one page job can be pending; queue subsequent pages from the callback and wait for configuration readiness before applying defaults.

### Added
- Cooperative, atomic page registration with bounded scanning, individual-field validation, progress/state getters and `appConfigRegistrationError()` reporting stage, field ID and cause. Small pages (up to 2048 bytes/8 fields) finish in one loop pass; larger pages use configurable 2 ms/16-operation work budgets.
- `addAppConfigEntity()` registers HA-only entities without adding portal pages; entities and page fields share IDs, state, commands and budgets. Added the HomeAssistantEntities example.
- `ARDPORTAL_APP_CONFIG_MAX_DEFINITION_BYTES` overrides the shared definition budget (256 KiB by default). Removed the separate 32 KiB page limit; 32-bit source offsets support definitions beyond 64 KiB.
- `startAppConfigPageRegistration(String&&)` accepts ownership of a dynamically built source without copying its buffer. Direct `FPSTR(PROGMEM_ARRAY)` registration remains preferable for fixed definitions.
- Configurable HA work limits and resumable dependency traversal keep HTTP and application loop work responsive.

### Changed
- Stream selected-page definitions, values and visibility one field at a time, without whole-page parsing or whole-response buffers. An open portal detects newly registered pages through a lightweight catalog during its one-second status refresh, preserving existing controls and unsaved edits.
- Append registration work without restarting HA Discovery, availability or subscriptions. Periodic Discovery is disabled by default; broker reconnect, HA birth and visibility changes still update it. Requeue current state after visible Discovery publication.
- Separate Console MQTT and Messages histories, with independent record/byte budgets, so MQTT traffic cannot evict registration diagnostics.
- Increase the shared field limit to 1024; state/dependency masks, traversal and indices support the full range. The page limit remains 16.
- Define storage budgets for a 128 KiB region: PortalConfig up to 5 KiB (including a 4 KiB CA certificate), AppConfig up to 8 KiB and the combined document up to 13376 bytes. Accept up to 4096 JSON nodes in storage validation.

### Memory and firmware
- Compact field metadata from 28 to 12 bytes on ESP8266. Build shared ID pools directly during registration, without staged String arrays or a final pool copy; spare capacity is bounded to 31 bytes per page.
- Move temporary HA descriptor defaults/overrides rather than deep-copying JSON trees; remove unused request-wide parse caches and keep static registration error reasons in flash.
- Pack Unicode Letter/Number range bounds into 24 bits, preserving the accepted character set and saving 1328 firmware bytes in the DynamicPages build.

### Documentation and validation
- Update README and dynamic-page examples for callback-driven registration, readiness checks, configurable limits and source ownership.
- ESP8266 DynamicPages compilation; sanitizer-enabled host tests covering registration, rollback/allocation failure, definitions beyond 64 KiB, budget overrides, HTTP streaming, HA/MQTT/dependencies, storage and separate Console histories. Frontend tests cover late page loading and preserving edits. Exhaustive Unicode classification check.

## 0.7.5 — 2026-10-06

### Breaking changes
- Configuration API now explicitly distinguishes `PortalConfig` (network/device settings), `AppConfig` (custom values and dynamic pages), and shared `PortalAndAppConfig` storage operations. Old names and type aliases have been removed; update application code to the new API.
- Use `onPortalConfigChanged(config, source)` for network/device changes and `onAppConfigValueChanged(key, value, source)` for custom application values.

### Fixed
- AppConfig-only saves and unchanged PortalConfig no longer trigger `onPortalConfigChanged`. Durable-save results remain available through `onPortalAndAppConfigSaved`.
- DHT example immediately queues updated temperature/humidity after calibration or unit changes, bypassing the regular MQTT reporting interval. It reuses the last sensor sample and retries after reconnection.

### Documentation and validation
- Updated examples and README, including an API migration table.
- ESP8266 compilation of ApplicationConfig and DHT examples; host tests with AddressSanitizer/UndefinedBehaviorSanitizer for configuration notifications and storage-aware restart ordering.

## 0.7.4 — 2026-10-06

### Added
- ESP8266 OTA accepts complete gzip-compressed `.bin.gz` firmware from a local file or URL. Uploads remain streamed; the core/eboot decompresses the image on restart. ESP32 continues to accept application `.bin` images only.
- `onBeforeRestart(callback)` receives the restart reason for manual restart and successful firmware update. Factory reset and storage formatting deliberately skip the callback.

### Fixed and changed
- Restart waits for earlier storage operations before cleanup. The callback may queue application configuration writes; ArdPortal flushes them without debounce and waits for storage completion before restarting. Write failures are reported and do not cancel restart. Network/portal configuration remains blocked during cleanup.
- Reentrant `portal.loop()` calls during restart cleanup cannot restart the device before the callback returns.
- Accepted MQTT control commands update live state and call application handlers immediately, independently of flash activity. Persistence is debounced/coalesced, reported hardware state cannot hide the requested command, and completing a flash commit does not replay the command.
- Documented compressed OTA, restart cleanup and application-data persistence, including the limits of separately owned ArdFS instances.

### Validation
- ESP8266 and ESP32 compilation.
- Host tests with AddressSanitizer/UndefinedBehaviorSanitizer: fragmented gzip uploads, invalid headers and interrupted uploads; restart ordering, durable callback writes, failed-write restart, clean factory reset and storage formatting without cleanup callback.
- Portal JavaScript smoke tests and generated-asset consistency checks.
