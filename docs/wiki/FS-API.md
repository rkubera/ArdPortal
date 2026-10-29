# ArdFS and storage API

[Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home) · [All topics](https://github.com/rkubera/ArdPortal/wiki/Home#start-here) · [Source](https://github.com/rkubera/ArdPortal)

## Storage classes

ArdPortal provides cooperative storage for journaled JSON documents and raw binary files. Operations advance through `loop()` and report completion through callbacks.

| Class | Include | Purpose |
| --- | --- | --- |
| `ArdFS` | `ArdFS.h` | Standalone storage using the native ArdFS filesystem. |
| `ArdPortalStorage` | `ArdPortalStorage.h` | Storage selected by `ARDPORTAL_USE_LITTLEFS`. |
| `ArdStorage<Volume, FileHandle>` | `ArdStorage.h` | Shared transaction engine used by the storage adapters. |
| `ArdFSVolume`, `ArdFSFile` | `ArdFSVolume.h` | Native volume and move-only file handles. |

ArdPortal initializes and services its own storage. Use the [Portal configuration API](https://github.com/rkubera/ArdPortal/wiki/Portal-API) for PortalConfig and AppConfig. Do not create another storage instance on the same partition while the portal owns it.

During startup and while storage is mounting or formatting, `portal.loop()`
advances storage in batches of at most 64 steps, stopping after 4 ms between
steps. This includes initial loading, startup redundancy repair and remounting
after formatting. Individual flash operations and callbacks are synchronous
and can exceed that time budget. Ordinary transactions after configuration is
ready advance one step per `portal.loop()` call. Standalone `ArdFS::loop()`
advances one step per call throughout its lifecycle.

## Initialization and lifecycle

```cpp
#include <ArdFS.h>

ArdFS storage;

void setup() {
  storage.begin();
}

void loop() {
  storage.loop();
  // Submit operations after ready() && mounted(), while !busy().
}
```

| Method | Contract |
| --- | --- |
| `void begin()` | Start mounting. Continue calling `loop()` until `ready()`. |
| `void loop()` | Advance initialization or the active transaction. Call frequently. |
| `bool ready() const` | Initialization has finished, including a failed mount. |
| `bool mounted() const` | Storage is mounted and available. |
| `bool busy() const` | Initialization or a transaction is pending. |
| `const String& error() const` | Most recent storage error. |
| `uint32_t commits() const` | Completed persistent writes, repairs and file mutations during this boot. |
| `uint32_t skipped() const` | Identical JSON saves skipped during this boot. |
| `bool format(Callback callback)` | Queue destructive formatting and remounting while idle; also available after a completed mount failure. |

One transaction runs per instance. A queued operation returns `true` when accepted. Completion is reported later from `loop()`. A `false` return means no callback will run for that request. Keep the instance and its callback data alive until completion. The instance becomes idle before invoking its completion callback, so that callback may submit the next operation.

Source, sink and completion callbacks execute synchronously and must return promptly. Physical flash operations also execute synchronously. Cooperative scheduling bounds logical work; it does not make a sector erase nonblocking.

## Completion result

```cpp
using Callback = std::function<void(const Result&)>;
```

| `Result` member | Meaning |
| --- | --- |
| `bool ok` | Operation completed successfully. |
| `bool found` | A valid document/file was found. A missing JSON document reports `ok=true`, `found=false`. A missing raw file reports an error. |
| `bool changed` | The operation changed persistent contents; false for an identical skipped JSON save. |
| `bool recovered` | A valid JSON copy was selected while another present copy was invalid. |
| `bool redundant` | Both JSON copies have validated redundancy. |
| `bool repaired` | Repair of the alternate JSON copy succeeded. |
| `uint8_t repairAttempts` | Number of repair attempts. |
| `String error` | Failure description. Check `ok`; translated text is not an error code. |

The result reference is valid during the callback. Copy information needed afterward. File contents are delivered to the sink, separately from the result.

## Paths

Paths start with `/`, contain 2–64 bytes and cannot contain `..`. Native ArdFS uses a flat namespace: slashes do not create directories. Reserve `path + ".journal"` for each journaled JSON document and allow space for that suffix within the path limit.

A JSON document occupies two physical paths. Raw file operations, deletion, renaming and enumeration operate on individual physical paths. Use separate names for raw files and journaled documents.

## Journaled JSON documents

```cpp
using JsonSource = std::function<String(size_t offset, size_t maximum)>;
using FileSink = std::function<bool(size_t offset,
                                   const uint8_t* bytes, size_t length)>;

bool writeJsonConfig(const String& path, size_t length,
                     JsonSource source, Callback callback);
bool readJsonConfig(const String& path, FileSink sink,
                    Callback callback, bool repair = false);
```

### Writing

`length` is the exact serialized JSON byte count, including any whitespace. Each source invocation must return exactly the requested slice: `min(maximum, length - offset)` bytes. Requests contain at most 256 bytes. Keep the source immutable until completion and support repeated reads of the same offsets. An empty or short response fails the operation.

The engine validates JSON syntax, compares exact payload bytes and skips an identical save. Whitespace differences count as a content change. Changed documents are stored with a generation number and CRC32 in alternating records at `path` and `path + ".journal"`:

```json
{"journal":1,"generation":42,"crc32":123456789,"data":"<JSON payload string>"}
```

The payload is escaped inside the envelope. Capacity must accommodate that encoding, both journal copies, filesystem metadata and other live files. Successful completion follows write verification. CRCs detect corruption; they do not encrypt or authenticate data.

JSON validation checks number syntax, UTF-8, escapes, surrogate pairs, nesting, container bounds and duplicate keys. It retains structural metadata and key hashes for open objects, without building a DOM. A key-hash collision rejects the document. There is no fixed document byte limit: `MaxBytes == SIZE_MAX` is a sentinel, not usable capacity or a buffer size.

### Reading and repair

Both copies are validated before delivering the selected payload. The newest valid generation is selected, including across generation-counter wraparound. The sink receives consecutive decoded payload slices of at most 256 bytes. Its byte buffer is borrowed for that invocation. Return `false` to abort with an error.

A missing document completes with `ok=true`, `found=false` and no sink calls. Invalid copies or I/O/allocation errors are reported through completion. Defer committing external side effects until completion confirms success; delivery performs checksum verification too.

Set `repair=true` to reconstruct a missing or invalid alternate from the surviving verified copy. Two valid copies may contain different generations and payloads. Repair preserves the selected record's generation and encoding, and leaves the surviving copy intact. It makes at most three attempts, with one-second backoff after failures. If repair fails, the surviving payload is still delivered, followed by `ok=false` and the repair error. Without a valid surviving copy, repair cannot reconstruct the document.

Plain JSON objects are also accepted as input records. A normal save uses the journal envelope.

### Example

The [AsyncStorage example](https://github.com/rkubera/ArdPortal/blob/main/examples/AsyncStorage/AsyncStorage.ino) demonstrates a complete standalone write/read cycle.

```cpp
static const char settings[] = "{\"enabled\":true}";

// Submit after mounting, while idle. Continue calling storage.loop().
bool accepted = storage.writeJsonConfig(
  "/settings.json", sizeof(settings) - 1,
  [](size_t offset, size_t maximum) {
    String part;
    if (!part.reserve(maximum) ||
        !part.concat(settings + offset, maximum)) return String();
    return part;
  },
  [](const ArdFS::Result& saved) {
    if (!saved.ok) {
      Serial.println(saved.error);
      return;
    }
    storage.readJsonConfig(
      "/settings.json",
      [](size_t offset, const uint8_t* bytes, size_t length) {
        Serial.write(bytes, length);
        return true;
      },
      [](const ArdFS::Result& loaded) {
        if (!loaded.ok) Serial.println(loaded.error);
      });
  });
```

## Raw binary files

```cpp
using FileSource = JsonSource;

bool writeFile(const String& path, size_t length,
               FileSource source, Callback callback);
bool readFile(const String& path, FileSink sink, Callback callback);
```

Raw files store arbitrary bytes, including embedded zero bytes. Empty files are supported. The exact write length is required; `SIZE_MAX` is not accepted. For a zero-length file the source is not called, but a source callback must still be supplied.

The source and sink contracts match the JSON API: bounded slices, immutable repeatable source, borrowed sink buffers and asynchronous completion from `loop()`. Raw writes replace the complete file and verify its contents. Each raw write publishes a record; there is no identical-content skip step. File size is bounded by available storage capacity, without a full-file RAM buffer.

## File management and capacity

| Method | Contract |
| --- | --- |
| `bool remove(const String& path, Callback callback)` | Queue deletion of one physical file. |
| `bool rename(const String& source, const String& destination, Callback callback)` | Queue rename; destination must not exist. |
| `size_t fileCount() const` | Number of physical files, including JSON journal copies. |
| `bool fileInfo(size_t index, String& path, size_t& size) const` | Retrieve one physical file's path and byte length; check the return value. Enumeration order is unspecified and can change after mutations. |
| `size_t totalBytes() const` | Backend-reported usable storage capacity. Native ArdFS reports one usable bank. |
| `size_t usedBytes() const` | Backend-reported occupied capacity, including native pinned/staged sectors where applicable. |
| `size_t freeBytes() const` | Reported available capacity; metadata and alignment still constrain payload size. |

For native ArdFS, capacity queries use RAM metadata without flash I/O. They return zero before mounting. A free-byte count does not guarantee that a payload of that size will fit. Native ArdFS supports up to 32 physical files, including journal copies.

Deletion publishes metadata without copying file payload. Native rename copies the renamed file; collection may also copy other live files. An open native reader keeps its referenced sectors pinned until it closes.

## Selecting LittleFS

`ARDPORTAL_USE_LITTLEFS` selects storage for all Portal operations:

```cpp
#define ARDPORTAL_USE_LITTLEFS 1
#include <ArdPortal.h>
```

| Value | Backend |
| --- | --- |
| `0` (default) | Native ArdFS. |
| `1` | Arduino core LittleFS. |

Define it before including Portal headers and use the same value in each application translation unit. See [Defines](https://github.com/rkubera/ArdPortal/wiki/Defines#storage-backend) and the [LittleFS example](https://github.com/rkubera/ArdPortal/blob/main/examples/LittleFS/LittleFS.ino).

Both backends provide PortalConfig/AppConfig persistence, journal validation, deduplication, repair, raw files, file management and storage formatting through the shared transaction engine. Standalone `ArdFS` explicitly selects the native backend.

The LittleFS adapter uses the board's filesystem region on ESP8266. On ESP32 it uses the Arduino LittleFS defaults: partition label `spiffs` and mount point `/littlefs`. Mount failure does not trigger automatic formatting. Use a formatted LittleFS volume or explicitly request storage formatting after initialization finishes.

LittleFS writes through a temporary file and publishes it by rename. Adapter temporary paths are reserved and excluded from enumeration; interrupted operations can leave temporary files that a subsequent write to that path replaces. Enumeration lists root files. Payload processing uses slices of at most 256 bytes; LittleFS mounting, flushing, formatting, renaming and garbage collection are synchronous core operations.

Each backend requires its own on-flash format. Use separate, nonoverlapping regions for simultaneous filesystem instances. Do not mount the same region with both drivers. Export required data before formatting a volume for another backend.

## Native volume and persistence

ArdFS requires a flash region of at least 64 KiB. ESP8266 uses the board-selected filesystem layout. ESP32 selects a data partition named `ardfs`, then `littlefs`, then the standard `spiffs` partition. Encrypted ESP32 partitions are unsupported. Only one instance may own a region at a time.

The volume uses format V4, two banks, checksummed catalogs and atomic publication. Live files and working metadata must fit in one bank; the spare bank supports collection and recovery. File payload can span noncontiguous 4 KiB sectors. Replacements publish the complete file before releasing the previous allocation. In-place file edits are not supported.

A completely erased usable region is initialized automatically. Unsupported formats, ambiguous corruption and I/O errors fail mounting without formatting. Explicit `format()` discards all files. If `requiresRemount()` is true after an uncertain publication, clear the underlying fault and call `begin()` to mount again.

Allocation distributes writes through a circular per-bank cursor. Metadata logs rotate through free sectors and periodic collection moves live files. Retired sectors reduce usable capacity. Failed destination writes may be retried in another sector using verified data; unreadable committed data requires another valid copy for recovery. Power loss can discard uncommitted RAM updates.

Native operations advance through bounded transfers or individual sector erases. Writers share a 512-byte allocation containing a 256-byte program cache and 256-byte relocation scratch; queues, handles, indexes and parser metadata require additional RAM. Readers own no program cache. See [Memory and limits](https://github.com/rkubera/ArdPortal/wiki/Memory-and-Limits).

## Low-level volume API

`ArdFSVolume` exposes native files without JSON envelopes. Synchronous methods drain the corresponding step operations internally.

| Method | Contract |
| --- | --- |
| `bool begin()` / `int beginStep()` | Mount synchronously / advance one mount step. |
| `void end()` | Unmount and release runtime metadata. Close file handles first. |
| `bool format()` / `int formatStep()` | Format synchronously / advance one format step. |
| `bool requiresRemount() const` | The volume must be mounted again before use. |
| `bool exists(const char* path)` | Test a physical path. |
| `ArdFSFile open(path, mode, length=SIZE_MAX, deferred=false)` | Open a C-string path with `"r"` or `"w"`; false handle on failure. An exact write length permits bounded reservation. Without a length, reserve provisional capacity from available bank space. With deferred preparation, drain `prepareStep()` before writing. |
| `ArdFSFile mutate(source, destination=nullptr)` | Create a deferred rename handle, or deletion handle when destination is omitted. Drain preparation and commit steps. |
| `fileCount()`, `fileInfo(index, path, size)` | Enumerate physical files. |
| `totalBytes()`, `usedBytes()`, `freeBytes()` | Query capacity. |
| `size_t maxFileBytes(size_t pathBytes) const` | Upper bound for one file; live files and working space can reduce actual capacity. |
| `int lastError() const` | Most recent internal error code. |

## Low-level file API

`ArdFSFile` owns its handle and supports moves, not copies. Closing an uncommitted writer abandons that write.

| Method | Contract |
| --- | --- |
| `explicit operator bool() const` | Whether the handle is open. |
| `size_t size() const` | File byte length. |
| `int read(uint8_t* destination, size_t count)` | Bytes read; negative on error. |
| `bool seek(size_t position)` | Set a read cursor in `[0, size()]`; invalid seeks preserve it. Writers reject seeking. |
| `size_t position() const` | Current cursor. |
| `size_t write(const uint8_t* source, size_t count)` | Bytes accepted, zero on refusal. Acceptance is not a persistence guarantee. Input memory may be reused after acceptance. |
| `int prepareStep()` | Advance preparation/collection. |
| `int writeStep()` | Drain queued input and program/verify cached pages. |
| `int flushStep()` | Advance final verification and commit. |
| `bool flush()` | Synchronously drain final verification and commit. |
| `bool close()` | Release the handle; abandon an uncommitted write. |

Step methods return `-1` on error, `0` while pending and `1` when that phase completes. Drain preparation before writing, and `writeStep()` between consecutive input slices. Finalize with `flush()` or completed `flushStep()` calls. Do not submit input during pending preparation or commit.

## Portal configuration and maintenance

Portal startup loads JSON with redundancy repair before marking configuration ready. Single-field application edits are staged in RAM and persisted through subsequent `portal.loop()` calls. The [Portal API](https://github.com/rkubera/ArdPortal/wiki/Portal-API) documents configuration callbacks, save scheduling and restart handling.

The Device page's **Format flash storage** action erases the selected storage volume and restarts after successful format/remount. It preserves firmware and Zigbee/Matter pairing. The HTTP equivalent is `POST /api/storage/format` with form body `confirm=FORMAT`. Responses are 200 on success, 500 on storage failure, 400 for invalid confirmation and 409 when lifecycle work prevents acceptance. Continue servicing `portal.loop()` while the request completes.

After confirmation, the Device page shows a formatting dialog with an
indeterminate bar moving back and forth. It remains visible until the operation
completes, then hands off to the restart dialog. A failed request restores the
Device controls and displays the error without starting the restart timer.

Factory reset saves default PortalConfig and an empty AppConfig, then restarts. Other files remain. Manual/OTA restart cleanup uses `onBeforeRestart()`; factory reset and storage formatting skip that callback.

---

[Back to documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)
