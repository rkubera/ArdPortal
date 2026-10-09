# Changelog

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
