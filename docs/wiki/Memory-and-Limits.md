# Memory and limits

[Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home) · [All topics](https://github.com/rkubera/ArdPortal/wiki/Home#start-here) · [Source](https://github.com/rkubera/ArdPortal)

## On this page

- [Network memory reserves](#network-memory-reserves)
- [Handling memory errors](#handling-memory-errors)
- [Independent limits](#independent-limits)

## Network memory reserves

On ESP32, memory diagnostics and Console pressure checks use
`MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT` for free bytes, total heap and largest
blocks. DMA measurements additionally require `MALLOC_CAP_DMA`; these pools
overlap and must not be added together. `/api/info` exposes `heap8FreeBytes`,
`heap8LargestBlockBytes`, `dmaAvailable`, `dmaFreeBytes`,
`dmaLargestBlockBytes` and `minimumDmaFreeBytes`. Existing free/minimum/maximum
heap keys also report byte-addressable memory; Info displays DMA separately.
ESP8266 uses `getFreeHeap()`/`getMaxFreeBlockSize()` and reports DMA unavailable.
Under pressure Console history shrinks to 1 KiB per stream; below 2 KiB free or
a 512-byte largest block, new records are skipped to preserve networking.

Before registration, dependency traversal, Discovery or a new dynamic HTTP
fragment, ArdPortal checks an estimated working set plus network headroom.
It leaves pending work intact while memory is constrained; new dynamic API
requests receive HTTP 503 rather than beginning an incomplete JSON response.
Basic flash/static HTTP and existing buffered writes continue. Estimates account
for per-field source length and JSON/output overhead; they are precautionary,
not a guarantee against another task allocating concurrently.

Override these defines globally in the library build when needed:

| Define | ESP32 default | ESP8266 default |
| --- | ---: | ---: |
| `ARDPORTAL_NETWORK_HEAP_RESERVE` | 24576 bytes | 4096 bytes |
| `ARDPORTAL_NETWORK_DMA_RESERVE` | 16384 bytes | Unavailable |
| `ARDPORTAL_NETWORK_BLOCK_RESERVE` | 4096 bytes | 1024 bytes |

Reserves are additional to the estimated work allocation. The ESP32 defaults
were exercised with two concurrent HTTP clients and a connected MQTT broker:
HTTP/network activity consumed roughly 8–11 KiB temporarily. Registration was
held pending below the reserve and completed after pressure was released.
These measurements cover the included test, not every TLS/radio/project load.
ESP8266 defaults are conservative starting values verified by compilation and
host tests; calibrate them on the actual device/workload.

With `ARDPORTAL_ENABLE_CONSOLE_MESSAGES=1`, registration reports accepted,
deferred and completed/failed states with before/after heap8 and DMA samples,
largest blocks and signed free-space deltas. Deltas include concurrent network
activity and diagnostic history; they are not an exact registry allocation count.
Page conditions are stored as compact JSON text rather than persistent JSON trees;
flash page sources remain in flash, RAM sources are moved into the registry, and
fields retain compact indexes instead of permanent parsed definitions.

`examples/HeapPressure` exercises controlled memory pressure on ESP32/ESP8266.
UART commands `0` release memory, `1`–`4` target 32/24/16/8 KiB free memory and `r`
queues its sample page. Watch the actual reported pools, registration state and
HTTP/MQTT behavior. Use a dedicated test board; press `0` after the experiment.

## Handling memory errors

Check JSON values with `isValid()` and check Boolean returns from registration and update methods. A rejected operation must not be treated as a successful value change. Use the Console's Messages tab, registration error details and heap metrics to diagnose memory pressure; see [Troubleshooting](https://github.com/rkubera/ArdPortal/wiki/Troubleshooting).

## Independent limits

| Resource | Limit / default | How configured |
| --- | --- | --- |
| Dynamic panel pages | 16 | Source constant; HA-only entities do not consume page slots |
| Registered fields | 1024 total across pages and HA-only entities | Source constant; not a guarantee that 1024 values fit RAM/flash |
| Total definition source | 256 KiB | `ARDPORTAL_APP_CONFIG_MAX_DEFINITION_BYTES` |
| Individual page source | No separate 32 KiB limit | Still subject to total definition, parser/header/field limits and RAM |
| Page node validation budget | 4096 | Registration parser budget |
| Serialized PortalConfig | 5120 bytes | `MaxPortalConfigBytes`, includes escaped CA and other settings |
| Serialized AppConfig | 8192 bytes | `MaxAppConfigBytes`, includes keys/JSON syntax |
| Combined config payload | 13376 bytes | `MaxConfigDocumentBytes` |
| ArdFS standalone JSON document | 14336 bytes | `ArdFS::MaxBytes` |
| CA certificate text | 4096 bytes | Portal validation, also must fit escaped PortalConfig |
| Field ID | 1–48 ASCII bytes | Letters, digits, underscore and hyphen |
| Label | 80 bytes | Schema validation; UTF-8 bytes, not characters |
| Basic select options | 16 | Unique stable values |
| Editable / read-only text | 128 / 512 bytes | Value validation |
| Extended composite state | 1024 encoded bytes | Extended value validation |
| Dependency condition | 6 nested groups, 64 nodes | Per field/page condition |
| General JSON defaults | 32768 I/O bytes, 8192 string bytes, 256 nodes, depth 16 | Per-operation `ArdJSON::Limits` |
| Hard JSON depth/container | 32 / 1024 elements | Source limits |
| MQTT received packet / outgoing body | 2048 / 2043 bytes | Topic/framing use part of capacity |
| Pending event/action reply FIFO | 8 entries | Shared transient-emission capacity |
| Console history | 16 entries, 4 KiB per channel | Reduced under pressure; independent MQTT/Messages histories |

A 128 KiB storage region is needed for the full portal journal budgets. It does
not allocate 128 KiB RAM. Similarly, a 256 KiB definition budget does not reserve
256 KiB heap: immutable flash sources plus compact indexes avoid full source
copies, but every individual field still needs temporary parse/serialization RAM.

A high total free heap with a small largest block can still reject a large String
allocation. On ESP32 use byte-addressable heap8 and DMA metrics, not a sum of
pools with different access capabilities. DMA is a subset, so do not add it to heap8.

---

[Back to documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)
