# Network-reserve test — 2026-10-08

ESP32-D0WDQ6 revision 1.0, Arduino ESP32 core 3.3.12. Two host HTTP workers
issued eight `/api/status` and `/api/info` requests per pressure level, while MQTT
remained connected. UART pressure commands held 512-byte internal/8-bit chunks.
A page was queued at the 24 KiB level and remained pending until memory release.

| Target free heap8 | Successful HTTP requests | Observed cumulative minimum heap8 | MQTT |
| --- | ---: | ---: | --- |
| 32 KiB | 8/8 | 24204 bytes | Connected in every Info sample |
| 24 KiB | 8/8 | 14340 bytes | Connected in every Info sample |
| 16 KiB | 8/8 | 10560 bytes | Connected in every Info sample |
| 8 KiB | 8/8 | 1840 bytes | Connected in every Info sample |
| Pressure released | 8/8 | 1840 bytes, retained historical minimum | Connected |

No extra boot banner, abort or backtrace appeared during the pressure sequence.
The initial startup phase had two timeouts before Wi-Fi readiness; those are not
included in the pressure rows. Each phase started from the actual reported pool,
not from `ESP.getFreeHeap()`; the 8 KiB phase reported 8580 bytes after allocation.
Heap8 and DMA were overlapping pools on this board and were measured separately.
Registration reported `accepted=1`, stayed pending with `permit=0`, and reported
`registration=completed` after command `0` released held chunks.

An additional check of `/api/pages` returned HTTP 503 with
`Network memory reserve; retry later` under pressure and HTTP 200 with all three
registered fields after release. No fields/entities were removed to recover.

32 KiB nominal free memory fell to roughly 24 KiB under load; earlier runs showed
8–11 KiB transient consumption. Defaults therefore retain 24 KiB heap8, 16 KiB
DMA and a 4 KiB contiguous block *in addition to* the estimated work allocation.
This is measured headroom for this workload, not a hard partition of the heap or
a guarantee for arbitrary TLS/radio/application activity. The deliberately held
8 KiB phase lies below the normal admission threshold and tests graceful deferral.

ESP8266: compilation and host ASan/UBSan checks passed, including low-heap and
fragmented-block deferral, HTTP 503, preserved pending registration and recovery.
No physical ESP8266 pressure measurement was performed; its 4 KiB free-space and
1 KiB block defaults require validation on the intended project/device.
