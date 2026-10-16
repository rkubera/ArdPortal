# Zigbee hardware validation — 2026-10-15

Tested with an ESP32-C5, 8 MiB PSRAM, Wi-Fi enabled, MQTT/HA in ArdPortal disabled, and Zigbee2MQTT 2.14.2. The fixture registers 22 application fields across 48 native endpoints. Actuator callbacks simulate hardware state; this is not a physical motor, valve, alarm or lock test.

## Results

| Control | Verified behavior |
| --- | --- |
| Switch | Both directions, ON/OFF |
| Slider | Both directions, boundaries and intermediate values |
| Select | Both directions, configured options |
| Typed text | Application-to-Zigbee temperature/humidity readings |
| Binary sensor | Application-to-Zigbee true/false |
| Light | Both directions: power, brightness, RGB primary colors, HS commands; XY command decoding |
| Climate | Modes; active target at min/max/intermediate values; independent heating/cooling thresholds; current temperature and heating/cooling/idle reports |
| Cover | Both directions: position, tilt, OPEN/CLOSE/STOP callbacks |
| Lock | Both directions: LOCK/UNLOCK callbacks and state; OPEN callback through Portal only |
| Siren | Portal settings; Zigbee warning and stop; application duration timer |
| Fan | Both directions: state, percentage, oscillation, direction |
| Valve | Both directions: open/close actions, position; application status reports |
| Humidifier | Both directions: state, target humidity, mode; current humidity reports |
| Water heater | Both directions: target temperature and all configured modes; current temperature reports |
| Alarm control panel | Both directions: five actions; all configured application status reports |
| Vacuum | Both directions: six mapped actions and fan modes; application status reports; custom JSON action through Portal only |
| Lawn mower | Both directions: START/PAUSE/DOCK; application status reports |
| Button / scene | Repeated incoming Zigbee actions and Portal callbacks |
| Device tracker | Application-to-Zigbee home/not_home |
| Update | Native fault-injection suite passed; real OTA failed during reception with a local timeout followed by INVALID_IMAGE; version 2 was not activated |

A repeated unchanged value intentionally does not force a new report. Tests of telemetry transitions first establish a different value. Transient Portal actions have callback semantics and do not represent outgoing Zigbee button events. Read-only measurements have no incoming setter exposed by Zigbee.

## Defects found and corrected

- Zigbee2MQTT retained the previous layout model ID. Re-interview read the current ID and activated the already installed matching converter, restoring application labels and composite controls.
- The SDK reporting scheduler exhausted its default 80 entries with the large endpoint layout. The scheduler now scales with staged attributes before `esp_zb_init()`, in addition to the network commissioning reserve. The hardware fixture retained its loop watchdog during validation.
- A single heating target of 10 °C previously parked cooling at 12.5 °C, so the SDK rejected a remote heating target of 22 °C before invoking ArdPortal. The inactive threshold now remains outside the configured active range. Hardware retests of 10→22 °C, 30→22 °C and independent `heat_cool` thresholds passed.

## Automated checks

`tests/run-zigbee.sh` exercises mappings, typed values, SDK command validation, reporting scheduler capacity, pending report retries, bounded command queues, queue ordering, commissioning, time synchronization, OTA parsing/flash failures/restart sequencing and decoded traffic. The generated converter was also exercised against the actual installed zigbee-herdsman-converters API.

Hardware logs and per-case JSON results are retained in the local test run directory; no gateway source code was changed. The real OTA attempt failed on 2026-10-15 at 09:04:10 Europe/Warsaw. The device logged a local timeout while OTA frames were still arriving, then Zigbee2MQTT reported INVALID_IMAGE. The old firmware remained reachable over HTTP and connected to Zigbee; version 2 was not activated. A regression test reproduced a false timeout when the SDK activity timestamp is one tick newer than the loop timestamp, including rollover. The timeout comparison has been corrected and native tests rerun; the subsequent hardware retry is documented below. The temporary OTA server and periodic monitor were stopped after failure.


## Full OTA retry after timeout correction

The corrected base firmware (OTA image version 1) was uploaded over USB, and a newly built version 2 image containing the same correction was served through the temporary gateway-only server. Reception started on 2026-10-15 at 09:10:00 Europe/Warsaw.

This retry reached **82.1%** and failed. The last recorded OTA frame arrived at **11:56:43**; the device timed out at **11:57:43**, after an actual 60-second reception gap. The SDK also reported `ZDO Device Unavailable` at 11:57:40 and later. Zigbee2MQTT ended the update with its 150000 ms request timeout at approximately **11:59:13**. This differs from the earlier false elapsed-time comparison: the corrected run stopped after a real gap in traffic. These logs do not establish the underlying cause of that communication loss.

No image verification, activation, firmware-version-2 boot or OTA-triggered restart was confirmed. HTTP management remained reachable on the existing firmware. Its cached Zigbee status still reported connected, despite the SDK unavailable indications; that status is not proof of a working radio link. The temporary image server and periodic monitor were stopped after the failed attempt. Full end-to-end Zigbee OTA remains **not passed**; no new transfer was started automatically.


## Retry with firmware-version metadata

The user requested another attempt. A fresh version-2 OTA image of **1,670,414 bytes** included application firmware-version metadata. After the old device stopped answering OTA queries, an ordinary restart restored communication without erasing pairing. Reception started on 2026-10-15 at **13:35:38 Europe/Warsaw**.

At approximately **15:05**, with the latest reported progress **56.33%**, the running version-1 firmware aborted with `Zigbee stack assertion failed mac/mac.c:2539`. Symbolization using the running base firmware ELF located the assertion in `zb_mac_ack_timeout`, called from the SDK scheduler. This identifies the failing SDK path; it does not establish why that assertion was triggered. The boot log explicitly reports `TEST_FIRMWARE otaVersion=1` afterward. The Portal became reachable and the cached Zigbee status again reported connected.

The update is **not passed**: no verified image activation, OTA before-restart callback or version-2 boot was observed. The gateway had not yet emitted its final transfer response when the incident was captured. The current serial log, `ota-mac-assert-symbols.txt` and `ota-mac-assert-incident.json` retain the evidence. Only the agent-owned OTA server and monitor were stopped; the gateway was not modified or restarted, and no subsequent transfer was started. Periodic monitoring was disabled after this definitive device-side failure.


## OTA comparison with Wi-Fi disabled

On 2026-10-15, the same 48 application endpoints and existing Zigbee pairing were tested with `ARDPORTAL_ENABLE_WIFI=0`. AP, HTTP and device-side MQTT were consequently excluded. Base image 1 and target image 2 retained the timeout and scheduler corrections. The OTA container was **1,538,126 bytes**, smaller than the Wi-Fi-enabled image, so this is an isolation test rather than a size-matched benchmark.

Reception started at **16:05:18 Europe/Warsaw**. Zigbee2MQTT confirmed successful version **1 → 2** at **18:21:09**, approximately **2 h 16 min** later. Serial evidence includes `TEST_RESTART reason=1`, followed by `TEST_FIRMWARE otaVersion=2`. A subsequent USB status request independently confirmed `otaVersion=2`, `wifiEnabled=false` and connected Zigbee state 5. Image verification, activation, restart callback and rejoin therefore passed for this run.

Post-update checks passed: changing the relay through the application USB API produced an ON report in Zigbee2MQTT; a Zigbee2MQTT OFF command changed the application value to false. Climate was first changed to heat and then to off through Zigbee2MQTT, and USB reads confirmed both transitions. WWW could not be tested in this intentionally headless firmware. Evidence is retained in `/private/tmp/ardportal-no-wifi-audit`, including `ota-result.json`, `ota-serial.log` and `post-ota-verification.json`.

This successful run shows that the failure is not inevitable with the retained endpoint layout and OTA implementation. Together with the earlier Wi-Fi-enabled failures, it warrants further investigation of coexistence, timing and resource pressure; one smaller-image run does not prove that Wi-Fi alone caused the previous SDK assertion or reception gap. No gateway code or converters were changed.


## Wi-Fi-enabled repeat, 2026-10-16

The Wi-Fi-enabled comparison started on 2026-10-15 was stopped at the user's request at its last reported **72.04%**. It is neither a completed update nor evidence of a transfer failure.

At the user's request, the same version-1 base firmware was uploaded again through USB on 2026-10-16, preserving pairing. Reception of the **1,670,878-byte** version-2 image started at **10:10:20 Europe/Warsaw**. Wi-Fi and WWW were enabled, device-side MQTT disabled, and the same 48 application endpoints retained. Zigbee2MQTT returned **status ok, 1 → 2 at 12:37:22**, after approximately **2 h 27 min**. No OTA timeout or SDK assertion was recorded in this run.

Serial confirmed `TEST_RESTART reason=1` and `TEST_FIRMWARE otaVersion=2`. A separate USB request confirmed version 2, Wi-Fi enabled and connected Zigbee state 5. HTTP information confirmed the restored Wi-Fi connection. Post-update tests passed: application USB relay ON reached Zigbee2MQTT, a remote relay OFF reached application state, and remote Climate heat followed by off reached application state. An HTTP read independently confirmed Climate OFF in the Portal. Results and logs are retained in `/private/tmp/ardportal-wifi-retry-audit`; older interrupted-run logs are under `previous-*`. The temporary OTA server was stopped after verification and periodic monitoring disabled.

Both Wi-Fi-disabled and Wi-Fi-enabled OTA have now completed on this device. The earlier intermittent failures therefore cannot be attributed to merely enabling Wi-Fi. The earlier assertion was resolved to the SDK MAC ACK-timeout path; its triggering condition remains unproven. An independent report of a similar function-level failure exists in [Espressif SDK issue 880](https://github.com/espressif/esp-zigbee-sdk/issues/880), but it is not proof of an identical cause here. No speculative radio workaround or gateway code change was introduced for this repeat. The passing run validates the tested path, not the absence of intermittent SDK or RF faults.
