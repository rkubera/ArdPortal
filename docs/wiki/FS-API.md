# ArdFS API

[Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home) · [All topics](https://github.com/rkubera/ArdPortal/wiki/Home#start-here) · [Source](https://github.com/rkubera/ArdPortal)

## What is ArdFS?

**ArdFS is the flash storage layer included in ArdPortal.** It saves data in the ESP's nonvolatile flash memory, so settings survive a restart or loss of power. Flash is different from RAM: RAM holds the running program's temporary values, while flash retains saved files when the device is switched off.

ArdFS has two layers:

- **`ArdFSVolume` and `ArdFSFile`** implement a small filesystem in a reserved flash region. They mount/format the volume and read, write or remove named files. Records have checksums and commit markers; two flash banks allow compaction while retaining the previous committed bank.
- **`ArdFS`** provides cooperative, journaled storage of a JSON document. Your application queues `read()`, `write()` or `writeJson()`, calls `loop()` frequently and receives the result through a callback. Work is advanced in bounded chunks, rather than building an additional complete serialized document for every save. The underlying flash operations themselves remain synchronous.

ArdFS implements its own on-flash format. It is not a wrapper around LittleFS or SPIFFS. On ESP32 it selects a suitable flash partition; on ESP8266 it uses the filesystem region selected by the board layout. Do not mount that same region with another filesystem. Formatting removes its stored files. See [Memory and limits](https://github.com/rkubera/ArdPortal/wiki/Memory-and-Limits) for partition sizes and document budgets.

Inside ArdPortal, ArdFS stores the shared document containing **PortalConfig** (Wi-Fi, MQTT and device settings) and **AppConfig** (application values). The portal initializes and services this storage itself. For a standalone application, include `ArdFS.h`, create an `ArdFS` object, call `begin()` once and `loop()` repeatedly. The [AsyncStorage example](https://github.com/rkubera/ArdPortal/blob/main/examples/AsyncStorage/AsyncStorage.ino) demonstrates this without a portal or MQTT connection.

**ArdFS stores bytes; [ArdJSON](https://github.com/rkubera/ArdPortal/wiki/JSON-API) interprets the JSON data.** Changing a JSON value in RAM does not by itself write flash. A storage request must be submitted and completed. Conversely, reading a saved JSON string does not directly configure application hardware: the application parses and applies its values.

## On this page

- [Journal, power loss and flash wear](#journal-power-loss-and-flash-wear)
- [Configuration write memory](#configuration-write-memory)
- [Streaming write API](#streaming-write-api)
- [Low-level volume and file API](#low-level-volume-and-file-api)

## Journal, power loss and flash wear

Two JSON records alternate between `/ardportal.json` and
`/ardportal.json.journal`. Each contains:

```json
{"journal":1,"generation":42,"crc32":123456789,"data":"<JSON payload string>"}
```

CRC32 covers the generation and exact payload bytes. A save overwrites the older
record, retaining the latest valid one. After writing, it flushes/closes the file
and reads it back in chunks before reporting success. Loading selects the newest
valid record, including across generation counter wraparound. Truncated records
or invalid CRCs are rejected in favor of the other record.

After a power interruption, loading can recover the previous document or a fully
committed new one; uncommitted RAM updates are lost. The journal runs over
ArdFS, which adds atomic record commits and two-bank compaction.
An erased successor header bounds each bank log, including after bank reuse and
compaction. It is prepared before replacing the previous end marker, so a restart
cannot expose committed records left over from an earlier bank generation.
Both invalid records produce an error and an AP with default settings, without
formatting the entire filesystem. Each stored configuration is a JSON document.

Flash wear is reduced by comparing exact payloads, skipping identical writes,
coalescing application updates for `appConfigSaveDelayMs` (750 ms by default) and
spacing application commits by `appConfigMinWriteIntervalMs` (5 seconds). Built-in
portal settings saves and explicit `flushPortalAndAppConfig()` bypass these delays; automatic
dynamic control edits retain the debounce. Logs, uptime, connection
states, NTP time and write statistics are not persisted.

A mount with no valid committed bank header triggers automatic formatting and a
remount, including on a new board; this can remove files when both bank headers
are damaged. Allocation, flash I/O, index capacity and flash layout errors abort
mounting without formatting or selecting an older committed bank. Likewise, an
allocation failure while validating configuration copies reports an error instead
of silently restoring an older value. Genuine invalid CRCs still allow recovery
from the other configuration copy. On ESP8266,
formatting is refused when the configured flash exceeds its physical size. For
a 4 MB ESP-12F, select a matching layout such as 4 MB with 1 MB filesystem and OTA.
A module name alone does not confirm its physical flash or selected layout.

Factory reset writes a new default configuration with an empty application
object, then restarts. It is a logical reset: the older journal remains a recovery
copy and other files remain. Explicit formatting deletes all ArdFS files and
restarts the device. The default factory identity is `ArdUI-<chip ID>` and AP
password `1234567890`.

Before a manual restart or successful OTA restart, register a
cleanup callback:

```cpp
portal.onBeforeRestart([&](ArdPortal::RestartReason reason) {
  // Queue application data; ArdPortal waits for the storage result after return.
  portal.setAppConfigValue("uptimeAtRestartMs", millis());
  // Put outputs in their safe state, stop peripherals or flush a UART.
  digitalWrite(RELAY_PIN, LOW);
  Serial.flush();
});
```

`RestartReason` identifies the triggering operation: `Portal` (Restart button),
`FirmwareUpdate` (successful `.bin` or ESP8266 `.bin.gz` OTA).
**Factory reset and storage formatting skip this callback**, so application cleanup cannot repopulate
the freshly reset configuration. The callback runs once from `portal.loop()`, after the HTTP response connection
has closed, the restart delay has elapsed and earlier storage operations have
completed. During the callback, `setAppConfigValue()` and `removeAppConfigValue()`
may queue application data despite the pending restart. Portal/network settings
remain blocked. After the callback returns, ArdPortal forces the configuration
save without debounce and continues servicing its storage in later `loop()`
passes. `ESP.restart()` runs only after storage finishes and `onPortalAndAppConfigSaved()`
has reported completion. If a configuration write fails, `onPortalAndAppConfigSaved(false, error)` and
`portalAndAppConfigError()` report the failure; restart proceeds after the failed transaction
has finished. An unsuccessful OTA/reset/format does
not invoke the callback. **Factory reset and storage formatting still skip it.**

Do not wait for an asynchronous save inside the callback: queue it and return.
Reentrant `portal.loop()` calls from the callback are ignored. This mechanism
waits for ArdPortal's own storage; a separate application-owned ArdFS instance
needs its own completion handling and is not automatically serviced by ArdPortal.

Direct application calls to `ESP.restart()`, watchdog resets, exceptions and power
loss do not pass through this callback. Clear it with `portal.onBeforeRestart(nullptr)`.

### ArdFS storage format

ArdFS is implemented in `ArdFSVolume.cpp`, with its own disk format and flash
backend. The format is a flat document store;
paths identify documents and do not create directories. It supports up to 32
physical files (journal records count separately), on a region of at least 64 KiB.

ArdFS reserves two storage banks for recovery. All live documents must fit in one bank, so the entire partition size is not available as application payload. Identical saves are skipped. Checksums detect corrupt data but do not authenticate or encrypt it. Keep calling `loop()` to advance pending storage requests; physical flash operations can still block temporarily.

ESP8266 uses the selected core flash layout. ESP32 selects the data partition
named `ardfs`, then `littlefs`, then the standard `spiffs` partition. Partition
labels do not determine the bytes stored inside. Encrypted ESP32 partitions are
currently unsupported. Only one storage instance may own a partition at a time.

ArdFS uses its own disk format. Mount failure triggers automatic formatting,
which can erase existing contents on its partition. ArdFS does not provide an
Arduino `File` API; the public methods store and retrieve JSON documents.

### Standalone storage

`ArdFS` can be used without `ArdPortal`:

```cpp
#include <ArdFS.h>
ArdFS storage;

void setup() { storage.begin(); }
void loop() { storage.loop(); }
// After ready() and mounted(), accept one operation at a time:
// storage.write("/settings.json", json, callback);
// storage.read("/settings.json", callback);
```

`read`, `write` and `format` return whether the operation was accepted. An
accepted operation completes through `Callback(const Result&)`, with `ok`,
`found`, `changed`, `recovered`, `data` and `error`. `commits()` and `skipped()`
are RAM-only statistics. Reserve the `.journal` path suffix for the module.
The public API is a cooperative JSON document store over ArdFS. Do not independently
mount/format the same flash partition or run another filesystem instance while
the portal uses it.

### ArdFS method and result reference

| Method | Contract |
| --- | --- |
| `begin()` | Schedule mount/automatic recovery; returns void. Call once, then service `loop()`. |
| `loop()` | Advance initialization or one transaction phase. No background task advances operations for you. |
| `ready()` | Initialization finished, including failure; check `mounted()` separately. |
| `mounted()` | Filesystem mounted successfully. |
| `busy()` | Initialization or one read/write/format transaction is running. |
| `read(path, callback)` | Accept an asynchronous document read; callback required. |
| `write(path, json, callback)` | Accept a JSON document save; callback required. Copies input into storage-owned RAM. JSON validation/readback can fail later in the callback. |
| `format(callback)` | Accept an explicit destructive format/remount while idle, including retry after mount failure; callback reports completion. |
| `error()` | Last storage error; returns a const String reference. |
| `commits()` | Completed changed document commits in this boot; RAM-only counter. |
| `skipped()` | Identical document saves skipped in this boot; RAM-only counter. |
| `MaxBytes` | `14336`, maximum input document bytes. JSON limits and heap can impose smaller bounds. |

`read`, `write` and `format` return acceptance, not completion. If a call returns
false, its callback will not run. Retry a busy rejection later; invalid paths or
oversized documents need correction. Paths must start with `/`, contain at least
two and at most 64 bytes, and must not contain `..`. Reserve the `.journal` suffix;
do not write journal slots yourself. Documents need not be objects, but must be
valid JSON. The exact serialized bytes determine deduplication: changed whitespace
can produce another commit even if parsed values are equal.

| `ArdFS::Result` member | Meaning |
| --- | --- |
| `ok` | Operation completed successfully. |
| `found` | A valid existing journal/document record was found; primarily relevant to reads. Missing files return `ok=true`, `found=false`. |
| `data` | JSON text on a successful found read; not the journal wrapper. Empty for writes/formats. |
| `changed` | A new document was committed, or formatting succeeded; false for an identical skipped save. |
| `recovered` | A valid record was recovered while another present journal record was invalid. |
| `error` | Failure text; check `ok` rather than interpreting translated error strings as codes. |

The Result reference and its strings belong to the callback invocation. Copy
`data` if you need it later. One transaction is allowed per storage instance;
the instance becomes idle before invoking its completion callback, so starting
a read after a completed write is supported.

```cpp
// README example: standalone journaled JSON storage (do not also start a portal).
#include <ArdFS.h>
ArdFS documents;
bool storageStarted = false;

void setup() {
  Serial.begin(115200);
  documents.begin();
}

void loop() {
  documents.loop();
  if (storageStarted || !documents.ready() || documents.busy()) return;
  if (!documents.mounted()) {
    storageStarted = true;
    Serial.println(documents.error());
    return;
  }
  storageStarted = documents.write("/settings.json", "{\"enabled\":true}",
    [](const ArdFS::Result& saved) {
      if (!saved.ok) { Serial.println(saved.error); return; }
      if (!documents.read("/settings.json", [](const ArdFS::Result& loaded) {
        if (!loaded.ok) { Serial.println(loaded.error); return; }
        if (!loaded.found) { Serial.println("No saved document."); return; }
        String error;
        const auto value = ArdJSON::JSON.parse(loaded.data, &error);
        if (value.isValid()) Serial.println(value["enabled"].asBool());
      })) Serial.println("Read was not accepted.");
    });
}
```

The portal does not expose its owned ArdFS instance. Store application settings
through `setAppConfigValue()` when using a portal. Starting a second `ArdFS`
instance for arbitrary documents on the portal's partition is unsupported; it
can invalidate the first instance's filesystem state.

### Using ArdFS alongside LittleFS

They use different disk formats. They must not mount or write the same flash
area, including through two different partition labels referencing overlapping
addresses. ArdFS mount failure triggers formatting, so trying to open a LittleFS
partition with ArdFS can erase it.

On ESP8266, ArdFS uses the selected Arduino filesystem region. Standard LittleFS
usually targets that same region: do not start both against the default layout.
The public ArdFS API has no custom partition-address selector.

On ESP32, ArdFS selects a data partition named `ardfs`, then `littlefs`, then
`spiffs` (SPIFFS subtype for that final fallback). For parallel use, provide a
dedicated `ardfs` partition and a separate, nonoverlapping partition for LittleFS;
configure LittleFS to use its own partition. Check the actual partition table and
mount arguments rather than assuming the two instances choose different areas.
ArdFS remains independent of LittleFS; installing LittleFS does not change its
format or API.

## Configuration write memory

Persistent single-field changes from forms, MQTT/HA and `setAppConfigValue()` stage only changed members. The saved application tree is updated after verified storage completion; the normal optimistic form behavior and callbacks are retained. Complete configuration replacement continues to use the existing replacement semantics.

ArdFS `writeJson(path, length, source, callback)` accepts a **validated, immutable JSON serialization producer**. The producer must return exactly `min(maximum, length-offset)` bytes or an empty String on failure and remain available until completion. Portal supplies its strict JSON writer. Journal CRC, encoding and readback verification advance in chunks up to 256 bytes; the journal format and alternating slots are unchanged. `read()` and `write()` accept complete document strings. Noncanonical envelopes use the full reader, and initial configuration loading still requires full-document memory. This optimization reduces write peaks, not the permanent settings tree or all startup allocations; source serialization and core flash operations remain synchronous.

Run `bash tests/run-config-memory.sh` for host memory/recovery regression tests.

## Streaming write API

```cpp
bool writeJson(const String& path, size_t length,
               ArdFS::JsonSource source, ArdFS::Callback callback);
using JsonSource = std::function<String(size_t offset, size_t maximum)>;
```

`path` selects the journaled document. `length` is the exact compact JSON byte
count. `source` supplies immutable validated JSON bytes until completion; every
call must return exactly `min(maximum, length-offset)` bytes, or an empty string
on failure. `callback` receives the same `Result` contract as `write()`.
`true` means accepted; `false` means no callback will run. Do not mutate or free
the source tree/string while the operation is pending.

Journal encoding, CRC and verification use logical chunks up to 256 bytes.
This avoids building a second complete wrapper, but does not make every core
flash operation asynchronous or remove full-document startup reading.

## Low-level volume and file API

Use `ArdFS` for journaled configuration documents. The low-level classes in
`ArdFSVolume.h` operate directly on the project filesystem, without the
application-level alternating JSON envelopes.

| API | Input and return |
| --- | --- |
| `ArdFSVolume::begin()` | Mount; bool success |
| `end()` | Unmount/release index; void |
| `format()` | Discard volume records; bool success |
| `exists(path)` | C-string path; bool existence when mounted |
| `open(path, mode)` | C-string path and `"r"`/`"w"`; move-only `ArdFSFile`, false on failure |
| `lastError()` | Last internal integer error code |
| `ArdFSFile::operator bool()` | Whether the handle is open |
| `size()` | File byte length |
| `read(uint8_t* destination, size_t count)` | Number read; negative on failure |
| `write(const uint8_t* source, size_t count)` | Bytes appended; zero on refusal |
| `flush()` | Commit dirty buffered contents; bool success |
| `close()` | Flush if needed and close; bool flush result |

File handles cannot be copied. Do not mount independently overlapping volumes
alongside the portal. Formatting affects all files in that volume.

---

[Back to documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

Local regression coverage and the USB ESP32-C5 bank-reuse investigation are recorded
in [configuration persistence validation](../testing/Configuration-persistence-validation.md).

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
most recently completed batch (0 initially, then 1–3). The existing
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

The original failed snapshot is retained without copying it on the error path.
Already accepted later edits survive both retries and exhaustion. Power loss can
still discard all changes awaiting a successful commit in RAM. Failed MQTT
connection-trial saves keep their deliberate rollback behavior instead of
retrying settings that the trial rejected.

### Startup redundancy repair

Portal startup calls `ArdFS::read(path, callback, true)` to validate both copies and
repair a missing or invalid alternate before marking configuration ready. Two
valid copies do not need equal generations or contents: the older valid copy is
normal journal history and is left untouched. Repair copies the surviving
verified document with its existing generation, flushes/closes the replacement
and checks the complete bytes by reading it back. It never writes the surviving
copy. Interrupted repair can be attempted again at the next boot.

Repair uses at most three attempts with one-second failure backoff. If all fail,
the surviving document is still returned in `Result::data` with `ok=false` and an
error. Portal can restore it for operation, reports unsuccessful storage status,
and refuses ordinary persistent changes until a later successful startup repair.
Explicit factory reset/storage format remain available as deliberate recovery
operations. If neither copy is valid, repair cannot reconstruct a document.

`Result::redundant` reports validated redundancy for a read;
`Result::repaired` indicates successful repair and `Result::repairAttempts` records
its attempt count. Standalone `read(path, callback)` retains its read-only default;
pass `true` as the third argument to request repair. Startup repair uses the read
completion/ready path, not the per-value failure callback for a new configuration
batch. The existing storage error/status API exposes a failed repair.
