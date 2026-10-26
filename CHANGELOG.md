# Changelog

## 0.9.0 — 2026-10-26

### Added

- Add opt-in native Matter over Wi-Fi or Thread on supported ESP32 targets, consuming the shared control classes, values and application callbacks independently of Zigbee.
- Map outlets, On/Off lights, dimmers, RGB lights, temperature/humidity/pressure/illuminance sensors, contact/occupancy/leak sensors, fans, thermostats and lift/tilt covers to native Matter endpoints.
- Add an ESP-IDF component and pinned ESP-Matter source project with `wifi`, `thread` and `thread-wifi` profiles. The mixed profile carries Matter exclusively over Thread while retaining the Wi-Fi portal; BLE commissioning stays enabled.
- Add default SDK development credentials and optional per-device factory credential providers, with a dedicated factory partition and no fallback to test credentials.
- Add Matter status, pairing-window and reset APIs, a main-menu Matter page and locally rendered QR/manual pairing codes for development credentials.
- Add optional human-readable Matter traffic diagnostics with a dedicated read-only WebSocket, bounded history and lifecycle messages for commissioning, connectivity, fabric removal and startup errors.
- Show Matter connectivity and separate Thread attachment in the status bar; show Thread device mode and actual network role in the Matter page and status API.
- Add optional router-capable Thread FTD source builds with `ARDPORTAL_THREAD_ROLE=router`. MTD remains the default; the network assigns the actual role, and router builds reject ICD and radio sleep.
- Add optional connected power saving for Matter Wi-Fi and Short Idle Time ICD for Matter Thread, integrated with ArdPower admission and pending work. Block forced light/deep sleep under Matter.
- Add `suspendPortal()`, `resumePortal()` and `portalSuspended()` to temporarily stop Wi-Fi, WWW, WebSockets, DNS and MQTT while Zigbee or Matter Thread keeps running. Preserve connection settings and reject suspension for Matter Wi-Fi.
- Add an optional sketch-owned `onLog` callback for Messages, MQTT, Matter and Zigbee, including builds without WWW; guard recursive forwarding.

### Changed

- Make dynamic-page `order` optional, using the zero-based registration position when omitted and preserving registration order for ties.
- Use two-character generated CSS classes and DOM IDs, including dynamic pages.
- Remove endpoint and unsupported-control counters from Matter/Zigbee web views while retaining API counts.
- Expand README/wiki documentation for Matter, Thread roles, credentials, power policies and portal suspension; update portal, Zigbee and Matter screenshots.
- Reduce firmware and RAM use through selective diagnostics/control compilation, shorter messages, shared command decoding and Console rendering, and sensor-only paths without command queues in Matter and Zigbee.
- Exclude unused MQTT WebSocket parsing and reduce Messages socket storage when MQTT is disabled; omit WWW history in headless builds while retaining sketch log callbacks.
- Disable C++ exceptions in source SDK profiles and validate saved build configurations. Reduce unused BLE reservations while preserving commissioning.
- Tune mixed Thread/Wi-Fi SDK memory use with bounded Wi-Fi/OpenThread buffers, on-demand Matter pools and eligible RTC-heap allocations; defer Thread startup until Wi-Fi/storage activity and available heap permit it.
- Cache Matter connectivity instead of repeatedly acquiring SDK locks in every portal loop.
- Run nine representative Arduino firmware builds and five source SDK builds as separately named Actions jobs, retaining the complete 19-profile Arduino suite locally.
- Retry transient Arduino tool downloads and install pinned clang-format 14.0.6 from a PyPI wheel instead of relying on Ubuntu package mirrors.

### Fixed

- Bind mixed-profile Matter IPv6 UDP traffic to Thread so CASE replies do not follow the Wi-Fi portal's default route; keep Thread commissioning on endpoint 0 and enable SDK radio coexistence.
- Correct dual-band Matter Wi-Fi capabilities and scan-result bands on ESP32-C5. Preserve commissioning credentials and fail-safe rollback across restarts.
- Prevent stale Matter reports from overwriting queued commands and merge same-source updates during configuration-save debounce.
- Keep dimmer brightness separate from On/Off; changing a remembered level while off no longer turns the light on, and turning on restores its level.
- Fix illuminance endpoint initialization, cover readiness/motion reporting and restored SDK state synchronization after reboot; publish bounded endpoint name labels for Home Assistant.
- Treat an empty compatible Matter inventory as informational `no_endpoints`, allow later page registration, and retain errors for actual registration failures.
- Load translations before Matter/Zigbee status polling and refresh labels after language changes; avoid transient SDK lock failures when reading cached pairing codes.
- Show Matter status when MQTT is disabled; hide OTA space and its API field when OTA is disabled.
- Pause radio saving during portal HTTP/WebSocket/AP activity, suppress repeated unchanged power decisions and report ArdPortal power admission changes to Messages.
- Show Wi-Fi scan/reconnect guidance before scanning and stop reconfiguring the fallback AP channel when disabling it after a 5 GHz connection.
- Correct Matter example page registration and USB console setup, mixed-profile Arduino feature overrides, and Linux host-test/lint failures.

## 0.8.2 — 2026-10-24

### Breaking changes

- Replace `textType` and top-level `device_class` with optional, type-checked `class`; rename `ARDPORTAL_ENABLE_TEXT_TYPES` to `ARDPORTAL_ENABLE_CLASSES`. Move event vocabularies from `ha.event_types` to top-level `event_types`. No compatibility aliases are retained.
- Regenerate layout-specific Zigbee2MQTT converters and ZHA quirks after upgrading: schema 2 adds class, canonical-unit and native/generic mapping metadata. Gateways may need a re-interview to refresh their cached layout.

### Added

- Add 119 semantic classes across 12 control types: `text`, `slider`, `binary_sensor`, `switch`, `valve`, `cover`, `event`, `device_trigger`, `button`, `humidifier`, `update` and `infrared`. `class` is always optional; class names and metadata are compiled only with their enabled controls.
- Add outlet and On/Off-light endpoint identities, class-aware covering presets and infrared receiver events. Keep protocol-independent control capabilities and validation available to adapters; Matter is not implemented in this release.
- Add enabled-by-default `ARDPORTAL_ENABLE_POWER_MANAGEMENT` to exclude the power-management subsystem when unused.
- Expand the DHT example with configurable GPIO and DHT11/12/21/22 sensor type, remote Console diagnostics and sensor reinitialization after repeated read failures.

### Changed

- Define built-in control presets once in a protocol-independent catalogue. Generate HA Discovery action payloads, command routes, templates, options, ranges and capabilities before compilation; keep explicit HA overrides for integration-specific customizations.
- Reduce firmware storage with compact class and preset tables, enum preset identities, shared unit/name strings, compressed Unicode device-name ranges and packed translated-message pools. Preserve accepted device-name characters and keep disabled controls and diagnostics behind their feature flags.
- Reduce portal HTML/CSS/JavaScript assets with generated one-character CSS classes and DOM IDs, direct translation of text-only containers, optional HTML attribute-quote removal and Console CSS exclusion when disabled. Preserve semantic page IDs, form names, API operations, ARIA references and readable authoring sources across control profiles.
- Reduce temporary strings and JSON allocations during control validation, normalization and HA Discovery. Visit Discovery members directly, borrow existing values, and reuse type/component/topic metadata.
- Centralize firmware diagnostics and shared UI/validation messages; use the Arduino IDE formatting style for sources and generated C++.
- Update the control catalogue, class migration guide, HA/Zigbee mapping documentation and screenshot preview to use the shared presets without repeated HA defaults.

### Fixed

- Publish DHT measurements as numeric Celsius/percent values or `null`, matching their classes. Remove the portal C/F selector and HA text-conversion template; HA handles display unit conversion.
- Generate lawn-mower HA command templates from shared action payloads so commands send the accepted `START`/`PAUSE`/`DOCK` values.
- Derive fan power/oscillation and humidifier power payloads from shared On/Off values. Keep device-tracker presence vocabulary independent of its initial state, without adding runtime metadata.
- Make the DHT host regression portable to Linux by explicitly importing `std::isnan`.

### Validation

- Pass host regressions with ASan/UBSan, gateway exporter compatibility, the Arduino example/API feature matrix, Arduino Lint, formatting and generated-resource checks in CI.
- Cover all 30 control types and class/type combinations, adapter payload/route/options mutations, numeric DHT readings and every generated portal JavaScript variant.

## 0.8.1 — 2026-10-22

### Added

- Add integrated power management through `portal.power()` with AlwaysOn, EventDriven, Periodic and Manual policies, configurable connected/light/deep sleep, admission blockers, wake deadlines and boot/wake-cause reporting. AlwaysOn remains the default.
- Add GPIO/ISR signals, software timers, periodic application cycles and Wi-Fi/Zigbee sleep coordination, with examples and documentation for supported network combinations and platform limitations.
- Add independent MQTT field publication settings through `mqtt.state_topic`, including `$device` expansion with HA enabled or disabled. Keep legacy HA topic overrides as backward-compatible aliases; explicit MQTT settings take precedence.
- Add CI checks for host regressions, gateway exporter compatibility, Arduino compilation across ESP8266/ESP32/C5/C6/H2, feature-switch API probes, Arduino Lint and generated assets. Pin actions, tools and board cores.

### Changed

- Reduce RAM copies of flash strings, translation text and page definitions, and reduce optional networking memory overhead. Remove internal size-report and measurement tooling.
- Parse portal style/script elements with the standard HTML parser, preserving generated assets and covering mixed-case tags, attributes and feature variants.
- Expand power-management documentation with policy/sleep combinations, wake sources, application locks and Wi-Fi/Zigbee behavior.

### Fixed

- Publish application state, events and action-triggered state reports to the configured MQTT topic instead of always using the field ID. Keep HA discovery and its default secondary state topics aligned with publication; missing, empty or `$state` settings retain the ID-based default.
- Correct generated ZHA quirks so default-entity filtering preserves explicitly exposed entities, metadata targets the intended platform and unknown thermostat temperatures do not appear as valid readings.
- Avoid allocating runtime state and triggering callbacks or MQTT updates for unchanged application values, and preserve MQTT packet buffers across partial writes.
- Clarify parser, JSON and Zigbee control flow for strict GCC warnings and retain the mocked clock in host tests.

### Validation

- Host regressions cover power policies, platform adapters, wake causes, GPIO/ISR events, timers, MQTT topic precedence and actual retained PUBLISH packets with HA enabled and disabled, streaming topic expansion, persistence and generated UI behavior.

## 0.8.0 — 2026-10-20

### Added

- Optional native Zigbee End Device and Router support on ESP32-C5, ESP32-C6 and ESP32-H2. Zigbee remains disabled by default and requires a supported chip and matching Arduino board mode.
- Automatic joining, saved-network rejoining, retry policies, network-only reset, state/result callbacks and background UTC synchronization from the coordinator.
- Map supported AppConfig controls to Zigbee: switches, numbers, selections, typed measurements, binary sensors, RGB lights, thermostats, covers, locks and sirens. Composite mappings expose supported portions of fans, valves, humidifiers, water heaters, alarm panels, vacuums, mowers, actions and presence states through separate endpoints.
- Generate layout-specific Zigbee2MQTT converters and ZHA quirks, with registered control names and endpoint routing. Download through WWW or export through bounded asynchronous C++ callbacks with backpressure and cancellation.
- Receive and validate Zigbee OTA images, report progress and failures, and restart through the existing firmware-update lifecycle. Image version/manufacturer/type are separate from the application display version.
- Add Zigbee network management, connection status and an independent Console channel/WebSocket with decoded traffic and lifecycle diagnostics. Lock PINs are redacted from diagnostics.
- Add ZigbeePortal and ZigbeeClimate examples with OTA/ArdFS/Zigbee partition layouts, setup instructions and a control support matrix.
- Add `setFirmwareVersion()` / `getFirmwareVersion()`, defaulting to `1.0`. Show this application version in Home Assistant device metadata, Zigbee Basic SWBuildID, Device information and `/api/info` as `firmwareVersion`.
- Add programmatic Wi-Fi, AP, scan and MQTT connection management, retry policies and callbacks, including MQTT unsubscribe acknowledgements and connection tests.
- Add enabled-by-default `ARDPORTAL_ENABLE_WIFI`, `ARDPORTAL_ENABLE_WWW` and `ARDPORTAL_ENABLE_TEXT_TYPES` switches. Support headless Zigbee builds while retaining local configuration/storage and Zigbee OTA.
- Add optional numeric/measurement `textType` mappings with HA defaults, units, scaling and unknown-value handling; plain text remains the default.
- Add an AP band preference on supported dual-band chips and optional sketch-owned Serial diagnostics for MQTT and Zigbee.
- Add configuration save attempt reporting and `onAppConfigValueSaveFailed()` with the affected key, attempted value, attempt count and terminal error.

### Changed

- Compile out disabled networking, captive DNS, HTTP OTA, Console channels, registration state and public APIs. Applications referencing disabled-feature APIs must guard those calls with the matching feature flags.
- Wi-Fi disabled also disables AP/WWW/MQTT/TLS/HA; ESP32-H2 selects this headless behavior automatically. WWW disabled still permits programmatic Wi-Fi/AP use.
- Freeze native Zigbee endpoint layouts after initial asynchronous page registration. Supported Zigbee commands share application state, callbacks and persistence; portal visibility dependencies do not hide Zigbee endpoints.
- Full factory reset also clears Zigbee pairing. Ordinary restart, OTA and storage formatting preserve pairing.
- Accept and coalesce application configuration edits during an active save in a separate queue, retaining the newest accepted value per key and including queued work in flush/restart draining. A setter returning `true` acknowledges RAM acceptance; save callbacks report durability.
- Retry a failed configuration batch up to three total attempts, at least one second apart. Preserve newer queued edits and MQTT acknowledgements through retries; after exhaustion, stop the failed batch and notify affected application values. MQTT connection-trial rollback remains deliberate.
- Validate both configuration copies at startup and repair a missing or invalid alternate from the verified survivor before accepting ordinary persistent edits. Repair preserves the good copy and uses up to three attempts; after exhaustion, restore the surviving data for operation but block ordinary persistent edits until recovery. Two valid copies, including different generations, are left untouched.
- Remove the internal HeapPressure and ESP32AllocationFailure diagnostic examples from the distributed example set.

### Fixed

- Prevent old records in a reused flash bank from overriding newer configuration after restart. Keep an erased successor marker before append and preserve committed data through interrupted operations.
- Distinguish allocation, flash I/O, layout and index-capacity failures from corrupt storage; do not silently select stale data or format on operational failures.
- Preserve unique Zigbee2MQTT Home Assistant Discovery identities and registered names after interview/re-pairing. Read initial thermostat state without unsupported reporting setup that previously skipped later controls.
- Correct Zigbee thermostat inactive-target/deadband handling and retain independent heat/cool thresholds. Extend the climate UI for dual targets.
- Size the native Zigbee scheduler before initialization and pace/fairly retry attribute reports, preserving unrelated commands and endpoints under load or allocation pressure.
- Prevent false Zigbee OTA timeouts from concurrent timestamp ordering and millis rollover; handle incomplete images, flash failures, aborts and deferred restart.
- Correct native Analog Output attributes, optional labels, light attribute updates and standard XY/Hue/Saturation/Enhanced Hue conversions. Enable RX-on-when-idle for the current awake End Device implementation.
- Enable Wi-Fi/IEEE 802.15.4 coexistence before Zigbee startup and report setup failures. Retire a conflicting fallback AP after a 5 GHz station connection on ESP32-C5.
- Paint Wi-Fi/AP disconnection guidance before submitting credentials, retain it across connection loss, and provide a link to the station address.
- Keep Console available without MQTT, isolate channel histories and omit disabled Zigbee/UI logic from generated assets. Reject missing/rejecting application action callbacks with explicit errors.
- Reduce endpoint, converter, JSON and disabled-module memory overhead, including borrowed `JSONVar::stringValue()` access.

### Validation and limitations

- Host regression suites cover native Zigbee lifecycle/clusters/commands/reporting/OTA, configuration queues and bounded retries, startup repair, allocation refusal, power-loss recovery, exports, feature switches, networking and generated UI behavior.
- Release checks compile DynamicPages for ESP8266 (core 3.1.2) and ZigbeeClimate for ESP32-C5 (Arduino-ESP32 3.3.12), verify generated assets, and exercise the exporters against ZHA 2.3.1 / zha-quirks 2.3.0 / zigpy 2.3.0 and Zigbee2MQTT 2.14.2 / zigbee-herdsman-converters 26.115.1. Update the HTTP memory test fixture to explicitly enable the MQTT history it exercises.
- Physical ESP32-C5/Zigbee2MQTT tests confirmed bidirectional controls and complete OTA activation, restart callbacks and rejoining, both with Wi-Fi disabled and enabled. Earlier failed runs remain documented; these results are not a guarantee for every board, role, gateway or workload.
- Zigbee remains an optional extension: it does not transport every Portal/HA control or property. Composite endpoints may appear as separate gateway entities; consult the documented mapping and gateway support limits.
- The current End Device implementation remains awake; it is not a battery-sleep implementation. Wi-Fi/Zigbee coexistence is role/workload dependent, and concurrent AP/Router operation is not supported.
- Expand README, Wiki, API references and hardware/storage validation reports while retaining the existing Wi-Fi, MQTT, Home Assistant, ArdFS and ArdJSON functionality.

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
