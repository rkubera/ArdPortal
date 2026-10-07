# Changelog

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
