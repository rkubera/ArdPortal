# Changelog

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
