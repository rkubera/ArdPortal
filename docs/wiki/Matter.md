# Matter

Matter is opt-in and consumes the same optional `class` and normalized control model as the portal and Home Assistant. Its native SDK adapter and command queue are separate from Zigbee. There is no `matter.device` property and no new JSON configuration block.

## Portal views

The development profile displays local pairing information and pairing/reset
actions. The production profile uses the device's provisioned pairing label.

![Matter over Wi-Fi](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/matter-wifi.png)

With Thread and the Wi-Fi portal enabled, the status bar displays both Matter
and Thread connectivity. These are separate from the portal's Wi-Fi connection.

![Matter over Thread](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/matter-thread.png)

These screenshots use illustrative data and a public development QR. See the
[full gallery](Screenshots).

## ESP-IDF / ESP-Matter build

The source-build project is `idf/MatterPortal`. It uses **ESP-IDF 5.5.5**, **ESP-Matter 1.5.1** and **Arduino-ESP32 3.3.12** (official Git commit `94afccf35fb1e401facddbcf9e13bcf7c76a31d8`) as components. Arduino provides the same endpoint wrapper API, compiled from source against this SDK; the application control adapter and callbacks are shared with the Arduino example. The project downloads the official Arduino source archive, verifies its SHA-256 and caches it in ignored `.build-tools/arduino-esp32`. ESP-Matter dependencies are fetched by IDF Component Manager. Direct SDK versions are pinned; transitive dependencies are resolved by Component Manager. Its generated lockfile stays local because the Arduino override records an absolute source path.

Install and activate ESP-IDF 5.5.5 using its normal `install.sh esp32c6` / `export.sh` workflow, then run from the repository root:

```sh
idf.py -C idf/MatterPortal -B idf/MatterPortal/build-wifi -DARDPORTAL_PROFILE=wifi -DIDF_TARGET=esp32c6 build
python3 tests/check-matter-sdk.py idf/MatterPortal/build-wifi --profile wifi
```

Use a separate build directory for each profile:

| `ARDPORTAL_PROFILE` | Matter transport | Portal | SDK networking |
| --- | --- | --- | --- |
| `wifi` | Wi-Fi | Wi-Fi WWW | Wi-Fi + BLE; OpenThread disabled |
| `thread` | Thread | Serial, no Wi-Fi | OpenThread MTD + BLE; Wi-Fi disabled |
| `thread-wifi` | Thread | Wi-Fi WWW | OpenThread MTD + BLE; Wi-Fi driver for the portal, Matter Wi-Fi commissioning disabled |

For example, replace both `build-wifi` and `wifi` with `build-thread` and `thread`. Profile selection is translated to the existing `ARDPORTAL_MATTER_THREAD` and `ARDPORTAL_ENABLE_WIFI` defines. It also changes actual SDK Kconfig options, rather than merely selecting a network at runtime. Simultaneous Matter on both networks is not implemented. BLE remains enabled. All three source-build profiles disable C++ exceptions throughout the application, Arduino and Matter SDK to reduce flash usage. Error handling must use return values rather than `throw`/`try`/`catch`; fatal library errors can abort and restart the device.

Existing build directories retain their saved settings. When upgrading from a build with exceptions enabled, use a fresh build directory or disable `CONFIG_COMPILER_CXX_EXCEPTIONS` in `idf.py menuconfig` for that build. Configuration rejects exceptions enabled, and the verification script checks `-fno-exceptions` on every compiled C++ source. Arduino IDE precompiled SDK settings are unchanged.

The source-build profiles also disable `ESP_WIFI_IRAM_OPT`, `ESP_WIFI_EXTRA_IRAM_OPT`, `ESP_WIFI_RX_IRAM_OPT` and `ESP_WIFI_SLP_IRAM_OPT`. Keeping these Wi-Fi throughput optimizations in flash leaves SRAM available for BLE, WPA authentication and WWW buffers. This reduces peak Wi-Fi throughput and can increase Wi-Fi power-save current. Existing Wi-Fi build configurations must disable all four options or use a fresh build directory; configuration and build verification reject the previous settings.

On targets with native USB Serial/JTAG, the IDF example uses that port for both flashing and its serial console. The example prints Wi-Fi state changes, station/AP addresses, page registration errors and Matter state. A station address of `0.0.0.0` means the device has not joined a LAN; connect to its setup AP to reach the WWW portal at `http://192.168.4.1/`. The default AP password is `1234567890`. For Wi-Fi Matter, LAN credentials can be provisioned by the Matter controller through BLE commissioning.

`idf.py -C idf/MatterPortal -B idf/MatterPortal/build-wifi -p PORT flash monitor` flashes the built image. Keep the included partition table: this 4 MiB development project has a single app slot and no firmware OTA. Reset Matter using the old firmware before changing the Matter transport or native endpoint identity/capabilities, then commission the new image. The build verification script checks both resolved configuration and the live linker map, so a Wi-Fi build retaining OpenThread is rejected.

### Credential profiles

Credential selection is independent of the transport profile:

| `ARDPORTAL_CREDENTIALS` | Attestation and identity | Pairing information |
| --- | --- | --- |
| `development` (default) | SDK example providers and development certificates | SDK development setup PIN/discriminator; portal QR and manual code; HA may require Test DCL |
| `production` | DAC/key, PAI, CD, commissioning verifier and device identity from the dedicated `fctry` NVS partition | Use the unique QR/manual code supplied with the provisioned device; no development PIN override or generated pairing code in the portal |

The WWW page and API follow the selected credential profile:

| Function | Development | Production |
| --- | --- | --- |
| Status, transport and endpoint counts | Available | Available |
| Credential profile shown in status | `development` | `production` |
| Portal QR/manual pairing code | Generated locally | Hidden; use the provisioned device label |
| Setup PIN/discriminator overrides in `ArdMatter::Options` | Available | Excluded; commissioning data comes from factory NVS |
| **Open pairing window** / `POST /api/matter/pair` | Available, 300 seconds | Available, 300 seconds |
| **Reset Matter** / `POST /api/matter/reset` | Available; requires `confirm=RESET-MATTER` | Available; requires the same confirmation, retains factory NVS |
| `portal.matter().openCommissioningWindow()` / `.reset()` | Available | Available |

Omitting `ARDPORTAL_CREDENTIALS` selects `development` in a new build directory. Use separate build directories for every transport/credential combination. For example:

```sh
idf.py -C idf/MatterPortal -B idf/MatterPortal/build-wifi-production -DARDPORTAL_PROFILE=wifi -DARDPORTAL_CREDENTIALS=production -DIDF_TARGET=esp32c6 build
python3 tests/check-matter-sdk.py idf/MatterPortal/build-wifi-production --profile wifi --credentials production
```

The same choice works with `thread` and `thread-wifi`; supported C5 builds use `-DIDF_TARGET=esp32c5`. CMake rejects a saved SDK configuration with providers or partition settings inconsistent with the requested credential profile. Switching profiles is a rebuild/provisioning operation, not a portal toggle. Reset Matter using the previous firmware before changing the credential identity, then provision and commission the new profile. The production profile is provided by the source SDK project; defining `ARDPORTAL_MATTER_PRODUCTION=1` with a precompiled development SDK is rejected.

#### Provisioning production credentials

Generate a **per-device factory NVS image** using the official [ESP-Matter manufacturing tools](https://docs.espressif.com/projects/esp-matter/en/latest/esp32/production.html). Supply the device's DAC and matching private/public key, its PAI and CD, commissioning discriminator/SPAKE2+ iteration count/salt/verifier, and product/manufacturer identity. Save the generated QR/manual code as the device's pairing label. Keep certificate images and private keys outside the repository.

`partitions.production.csv` keeps the development app and ArdFS offsets/sizes. It replaces the unused flash-coredump slot with **`fctry`, 64 KiB at `0x3F0000`**. Runtime fabrics/settings remain in the separate `nvs` partition; production uses `CONFIG_CHIP_FACTORY_NAMESPACE_PARTITION_LABEL="fctry"`. The factory image must fit this partition. Flash the production firmware and partition table with `idf.py flash`, then provision the factory image separately, using the actual port/chip:

```sh
python -m esptool --chip esp32c6 --port PORT write_flash 0x3F0000 /secure/path/device-factory.bin
```

The firmware build does not generate credentials, bundle a private key, or automatically overwrite the factory partition. Before creating endpoints, production checks that the partition and mandatory records are present, without copying secrets or erasing invalid NVS. Missing/incomplete records stop Matter startup with `Matter factory credentials missing or incomplete`; there is no fallback to example credentials. The SDK and controller still validate cryptographic material and trust: choosing this profile does not make a self-signed/test certificate trusted or certify the product.

The production `ArdMatter::Options` contains only the BLE commissioning choice. It excludes development passcode/discriminator fields and preserves factory vendor/product identity. ESP-Matter's factory provider exposes a SPAKE2+ verifier rather than the original setup PIN, so the portal displays an instruction to use the device label, while keeping status, transport, **Open pairing window** and **Reset Matter** available. `manualPairingCode()` and `onboardingUrl()` return empty strings in production; the QR HTTP endpoint is unavailable. Resetting Matter clears mutable fabrics/network data, not the separately provisioned factory identity. No private key or QR is uploaded through the portal.

## Arduino IDE build

Use ESP32 Arduino **3.3.12**, a Matter-enabled board profile and sufficient flash/RAM. Start with `examples/MatterPortal` on ESP32-C6, 4 MiB flash. Select **Huge APP** and retain its `partitions.csv`: a 3.5 MiB single application, 88 KiB NVS and 320 KiB ArdFS. This profile has **no firmware OTA slot** and disables HTTP OTA. Use a larger flash device and two appropriately sized app slots for OTA.

Define these before `#include <ArdPortal.h>`:

```cpp
#define ARDPORTAL_ENABLE_MATTER 1
#define ARDPORTAL_MATTER_THREAD 0 // 0 Wi-Fi, 1 Thread
#define ARDPORTAL_MATTER_MAX_ENDPOINTS 16 // optional; 1..64
```

Matter is disabled by default. Disabling it excludes its SDK headers, endpoint objects, command queue, HTTP handlers and strings. The existing individual control defines also remove their Matter endpoint implementations. Use `ARDPORTAL_ENABLE_CONTROLS 0` and enable the controls you actually use; the example does so.

Thread requires ESP32-C5/C6/H2, a Thread-enabled SDK and a border router. H2 defaults to Thread and has no Wi-Fi portal. C5 additionally needs the matching **Matter Network** board menu (`wifi` or `thread`). C6 supports selection in the adapter. Matter and Zigbee cannot be enabled in the same firmware. Wi-Fi and Thread are alternative builds, not simultaneous Matter networks.

## Lifecycle and commissioning

Pages with no Matter-compatible controls are valid. After registration settles,
Matter reports `no_endpoints`, an empty `error`, and informational text in `info`.
It does not start the SDK or repeat startup attempts. This state permits later
page registration; adding a supported control resumes staging. An actual page
registration failure remains an error rather than being hidden by this state.

Use one global `ArdPortal`. Register all pages/entities during startup. After configuration loads and registration remains idle for one second, the adapter freezes the inventory, creates at most one endpoint per loop iteration, then starts Matter. Late registration is rejected. Declare every field deterministically, in the same order on every boot.

Open **Matter** in the main WWW menu (`/matter`) for status, a locally rendered QR code, a manual pairing code, opening a 300-second commissioning window and confirmed Matter reset. Serial-only applications can use:

```cpp
portal.matter().state();
portal.matter().manualPairingCode();
portal.matter().onboardingUrl();
portal.matter().openCommissioningWindow();
portal.matter().reset(); // queued; resets Matter and restarts, preserves ArdFS
```

In the development profile, setup codes are available after the Matter stack starts, including when the commissioning window is closed. The code alone does not open commissioning; use **Open pairing window** when needed. Commissioning starts through BLE by default. Wi-Fi credentials supplied by the commissioner are owned by the Matter SDK; once Matter starts, ArdPortal does not run automatic Wi-Fi association/timeouts against them. Configure portal Wi-Fi before Matter starts if you need its AP/IP functions. While Wi-Fi Matter is running, changing portal station credentials and manual station connect/disconnect calls are rejected; update the network through the Matter controller or reset/recommission. Ordinary application saves do not disconnect the SDK network. Thread credentials belong to the Matter SDK. A border router alone does not commission the device; use a Matter controller.

The example uses SDK development credentials (passcode `20202021`, discriminator `3840`). In the development profile, applications can call `portal.matter().configure(options)` with `ArdMatter::Options` before startup to override the setup PIN/discriminator or disable BLE for on-network commissioning. Production uses provisioned factory data instead of these development overrides. Provision unique production credentials and the SDK manufacturing data required by your product. This adapter does not confer Matter certification.

### Home Assistant: development certificates and Test DCL

The example uses the SDK's test/development device attestation certificate. A Home Assistant Matter Server enforcing production certificate trust can discover the device, establish a pairing session and then reject it during **OperationalCredentials.DeviceAttestation**. Successful Wi-Fi connectivity or an accepted pairing-window request does not bypass this check.

The confirmed failure in Matter Server **9.2.0** was:

```text
TrustedAsTestCertificate
Attestation rejected: This device uses a test/development certificate.
To commission it, enable the "Test DCL" option in the settings.
```

To allow this development firmware through the Home Assistant web interface:

1. Open **Settings → Apps → Matter Server**. Older Home Assistant versions call Apps **Add-ons**.
2. Select **Configuration**.
3. Enable **Test DCL** (`enable_test_net_dcl`). If it is hidden, enable the display of optional configuration options.
4. Click **Save**, then **Restart** Matter Server.
5. Open the device's **Matter** page and select **Open pairing window**, then retry adding it in Home Assistant with the QR or manual pairing code shown by the portal.

The relevant application configuration option is:

```yaml
enable_test_net_dcl: true
```

This setting permits development/test certificates for the Matter Server; enable it when testing firmware you trust. A production product needs its own production attestation credentials and the applicable certification. Changing the setup PIN or discriminator does not turn a development certificate into a production certificate.

If using the same phone for the portal and Home Assistant, first read or copy the manual pairing code from the ArdUI setup AP. Then switch the phone back to the **home Wi-Fi network before adding the device in Home Assistant**. The setup AP at `192.168.4.1` is for accessing the portal, not the target network for Matter. The portal renders its QR locally, so reading it does not require Internet access. **Open pairing window** enables commissioning for five minutes; the acceptance message does not mean that Home Assistant has added the device.

If pairing still fails, inspect **Matter Server → Logs** for the latest commissioning failure. A `TrustedAsTestCertificate`/`DeviceAttestation` rejection identifies certificate trust as the blocker; diagnose other errors from their actual failing step rather than assuming the same cause. **Reset Matter** clears fabrics/network credentials and restarts the device while preserving ArdFS; use it when deliberately starting pairing again from a clean state, not as a prerequisite for enabling Test DCL.

### Android: pairing completes but HA has no device

On dual-band targets such as ESP32-C5, the source SDK project overlays the
pinned CHIP Wi-Fi commissioning driver to advertise both 2.4 and 5 GHz and
classify scan results by their channel. The original driver reports only 2.4
GHz, including for 5 GHz scan results. The overlay is compiled only when Matter
Wi-Fi and the target's 5 GHz capability are enabled; single-band and Thread
profiles retain the original driver. This corrects capability metadata, but
does not bypass router channel, security or regulatory restrictions.

Start commissioning from the **Home Assistant Companion app → Settings →
Connectivity → Matter → Add device → No, it is new**. If starting from an
Android QR notification, select Home Assistant as the target app. Finish the
Google commissioning screen and return to HA; completion of the Android stage
alone does not confirm that HA has added its own fabric.

Give the Companion app location permission **Allow all the time**, as required
by the [HA Matter prerequisites](https://www.home-assistant.io/integrations/matter/).
While Google's commissioning screen is in front, Companion runs in the
background; a while-in-use permission can prevent it from reading the Wi-Fi
SSID and choosing its HA connection URL. Verify that the configured HA URL is
reachable from the phone on home Wi-Fi. If the handoff still fails, inspect the
Companion app debug logs for `MatterCommissioningService`,
`matter/commission_on_network` and WebSocket connection failures, alongside the
Matter Server logs. Do not infer a firmware failure or an app failure from the
absence of entities alone.

If Android/Google completes Wi-Fi or Thread setup but Home Assistant still lists no
device, add HA as another Matter controller without resetting the device:

1. In ArdPortal, select **Matter → Open pairing window**.
2. In the HA web interface, open **Settings → Devices & services → Matter → Add device**.
3. Select **Yes, it is already in use → Other controllers**.
4. Enter the manual setup code shown by ArdPortal and finish adding the device.

ArdPortal opens a basic commissioning window, which uses the device's original
setup code. The existing Thread credentials and controller fabric are preserved.
Wait for HA to display the device and its entities before treating its addition
to HA as complete; mesh attachment alone does not establish an HA fabric.

A fingerprint of ordered native endpoint IDs, kinds and thermostat capabilities is persisted in NVS. Defaults, labels, MQTT topics and unsupported fields are excluded. A changed layout stops startup instead of attaching saved fabrics to different endpoints. Reset Matter **using the old firmware before installing a changed layout**, or erase NVS and commission again. An interrupted reset can require repeating the NVS erase. Erasing ArdFS alone does not erase fabrics. Portal factory reset also requests SDK Matter reset when the stack is running.

## Mappings

Each supported field creates one endpoint. Classes remain optional; a class only supplies missing semantics, never forces an incompatible mapping.

| Control | Class / supported shape | Native Matter endpoint |
| --- | --- | --- |
| `switch` | omitted, `switch`, `outlet` | On/off plug |
| `switch` | `light` | On/off light |
| `slider` | `brightness`, min 0, positive max | Dimmable plug; 0..254 scaled to declared max/step |
| `text` | `temperature` | Temperature, Celsius, 0.01 resolution |
| `text` | `humidity` | Relative humidity, %, 0.01 resolution |
| `text` | `pressure`, `atmospheric_pressure` | Pressure, hPa, integer resolution |
| `text` | `illuminance` | Illuminance, lux, logarithmic Matter encoding |
| `binary_sensor` | `door`, `window`, `opening` | Contact sensor; portal true=open is inverted |
| `binary_sensor` | `motion`, `occupancy`, `presence` | Occupancy sensor |
| `binary_sensor` | `moisture` | Water leak detector |
| `light` | power + brightness + RGB controls | Color light, HS and XY commands; no color temperature |
| `fan` | power + percentage controls | Fan power/speed; direction/oscillation remain portal-only |
| `climate` | `off`, `heat`, `cool`, `heat_cool` modes only | Thermostat; Celsius, heat 7..30, cool 16..32; shared range 16..30 and explicit `heat_cool` when both supported |
| `cover` | omitted, `blind`, `curtain`, `shade`, `shutter`, `awning`; position + tilt | Lift-and-tilt window covering; percentages inverted |
| `select`, `edit`, `date`, `time`, `datetime`, `button`, `scene`, `notify`, `valve`, `humidifier`, `water_heater`, `lock`, `siren`, `vacuum`, `lawn_mower`, `alarm_control_panel`, `event`, `device_trigger`, `tag`, `infrared`, `device_tracker`, `update` | all | Unsupported; remain available through existing integrations |

Other classes/shapes are unsupported and counted in status. Writable endpoint mappings reject explicitly read-only definitions. Thermostat fan modes/actions, fan oscillation/direction and cover state labels are not native Matter attributes. The thermostat requires declared heat/cool capability; a dual-capability definition must include `heat_cool` and retain a 2.5 °C setpoint deadband even while off; SDK defaults limit its accepted setpoints. Out-of-range/unknown numeric sensor readings are reported as Matter null, never as zero or a textual unit string. Binary readings are booleans. Resolution/quantization follows the native endpoint.

Cover OPEN/CLOSE/STOP use the existing action callback. Position/tilt commands and other writable state commands use normal value validation, persistence and `ChangeSource::Matter`. The application drives actuators and reports measured state; the adapter is not a hardware driver.

## Memory and concurrency

SDK callbacks enqueue at most eight compact commands (8 bytes each). Builds with only sensor control families omit the command queue and its indices. Application callbacks, JSON parsing and ArdFS writes run from `portal.loop()`. A full queue rejects new commands. Accepted newer application/configuration intents supersede queued commands for that field. Reports are coalesced per endpoint and serviced incrementally; lock contention or newer queued commands for that endpoint postpone a report. Commands from Matter merge during the save debounce interval; an active writer or a pending save from another source still delays processing. Rejected commands trigger reconciliation with application state. For a brightness slider, zero means off in the application model, while Matter retains its separate remembered level. Updating that level while off does not switch the control on; switching on restores the SDK level.

Matter has substantial SDK flash/static RAM and runtime heap costs; it is not suitable for ESP8266. The endpoint limit bounds adapter allocations, not total SDK heap. SDK endpoints must live until restart; do not create/destroy portals dynamically. Automatic sleep is blocked while this integration is enabled: this is an always-on implementation, without Matter ICD support.

## HTTP API

`GET /api/matter/status` returns state, configured transport, credential profile (`development` or `production`), commissioned/connected flags, endpoint/unsupported counts, error and commissioning code/URL. `GET /api/matter/qr` returns the QR matrix as a size and hexadecimal bit string for local rendering. Thread builds additionally report `threadProvisioned` (a stored Thread dataset), `threadConnected` (attached to Thread), and `wifiConnected` (the Matter SDK Wi-Fi connection flag). The configured `transport` alone does not prove a Thread attachment; commissioning over a different interface can create a fabric without provisioning Thread. Connectivity snapshots are refreshed once per second. `commissioningInProgress` indicates an armed SDK fail-safe: a temporary fabric and network attachment do not yet prove completed commissioning. Thread builds with WWW also report `threadRole`, `threadSrpHost`, `threadSrpRunning`, `threadCommissionableRegistered` and `threadOperationalRegistered`; these distinguish mesh attachment from registered discovery services. SRP snapshots use a nonblocking Thread lock and three bytes of cached state. `POST /api/matter/pair` opens the pairing window. `POST /api/matter/reset` requires body `confirm=RESET-MATTER` and queues reset/restart. These routes use the portal's existing access model; keep portal access on a trusted network.

Native SDK compilation and host mapping tests do not replace commissioning and subscription tests with physical devices/controllers. Validate the chosen board, Thread border router, actuator behavior and controller before deployment.

Matter is available in the main WWW menu. Its page shows transport, state, endpoints, a locally rendered QR code and the manual pairing code, and offers pairing-window and confirmed factory-reset actions.

The source SDK reserves one NimBLE peripheral connection and smaller packet pools (12/8 MSYS blocks, 8 incoming ACL buffers, 12/4 event buffers). BLE commissioning stays enabled; central and observer roles are unused by this endpoint adapter. Existing builds must apply these defaults to their saved sdkconfig or start from a fresh build directory; the SDK verification script checks the resolved values. These pools remain above the [ESP-Matter low-memory recommendations](https://docs.espressif.com/projects/esp-matter/en/latest/esp32c3/optimizations.html).

The QR image works without Internet access, including on the setup AP. Scan it with a second device, or enter the manual code when the portal and Matter controller are on the same phone. Opening the pairing window enables commissioning; the acceptance message does not itself pair a controller.

### Control names on Matter endpoints

Each supported control publishes its `name` (or `id` when no name is supplied) in the endpoint Fixed Label cluster. The `name` label is also exposed as `ha_entitylabel`, recognized by Home Assistant for test/DIY vendor IDs. [HA 2026.10](https://github.com/home-assistant/core/blob/2026.10.2/homeassistant/components/matter/entity.py) uses this as a name suffix, for example **Switch (Outlet)** or **Light (Brightness)**, rather than replacing its translated entity type. Other vendor/product IDs follow the controller’s labeling policy. Values are limited to 16 UTF-8 bytes by Matter and truncated at a character boundary. Labels do not alter endpoint IDs, control capabilities, factory identity or the saved layout fingerprint. Existing HA entities may require a Matter integration reload to rediscover names; user-assigned entity names remain under HA control.

## Power management

Matter uses the existing `portal.power()` API. The default remains `AlwaysOn`.

| CMake setting | SDK configuration | Transport |
| --- | --- | --- |
| `-DARDPORTAL_POWER=off` (default) | No additional PM/ICD code | All |
| `-DARDPORTAL_POWER=connected` | PM and tickless idle; connected modem/automatic light sleep | Wi-Fi |
| `-DARDPORTAL_POWER=icd` | PM, tickless idle, IEEE 802.15.4 sleep and Short Idle Time ICD server | Thread, Thread + Wi-Fi portal |

Use a fresh build directory when changing SDK power options. For example:

```sh
idf.py -C idf/MatterPortal -B build-matter-icd \
  -DARDPORTAL_PROFILE=thread -DARDPORTAL_POWER=icd \
  -DIDF_TARGET=esp32c5 build
```

Configure the application before `portal.begin()`:

```cpp
ArdPower::Options options;
options.policy = ArdPower::Policy::EventDriven;
options.sleep = ArdPower::SleepType::ConnectedSleep;
portal.power().configure(options);
```

The MatterPortal example enables this configuration when SDK PM support is compiled in.
`Manual` and `Periodic` policies also use the existing ArdPower API. `AlwaysOn`,
application locks, startup, AP, HTTP/WebSocket activity, storage, OTA and restart
block CPU sleep. Connected CPU sleep uses bounded cooperative waits (`maxIdleMs`, 20 ms
by default), even without an application timer/GPIO; SDK tasks and timers retain
their own wakeups. Disable the AP and close persistent portal connections to allow
sleep. In Thread + Wi-Fi builds both radios must permit sleep.

Wi-Fi keeps its connection through modem sleep and, when PM/tickless idle is
available, automatic CPU light sleep. Without that SDK support ArdPower reports
`ModemSleepFallback`. The SDK continues handling Matter messages and timers.

ICD is available only over Thread in the pinned SDK. It advertises the standard
ICD Management cluster; the SDK selects sleepy Thread operation and owns polling,
active windows and reports. This profile uses Short Idle Time: 2 s idle interval,
1 s active window, 200 ms fast polling and 5 s slow polling. It does not enable
Long Idle Time. While the application is busy or reports/commands are pending,
ArdPortal requests a bounded active window, refreshed every 500 ms. Commissioning
and exchanges have additional SDK-managed active requirements. These are protocol
intervals, not guaranteed measured sleep times or battery consumption.

Forced `LightSleep` and `DeepSleep` are blocked with `Network` while Matter is
selected: neither may suspend the running stack. Use `ConnectedSleep`; its CPU
sleep is coordinated by the RTOS and SDK. Non-ICD Thread builds retain their
always-awake behavior. No extra ICD queue or task is allocated by ArdPortal;
ICD-specific adapter state is compiled out when ICD is disabled.

See the [ESP-IDF sleep documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32c5/api-reference/system/sleep_modes.html)
for the distinction between forced sleep and connected automatic light sleep.
Hardware current consumption and Thread ICD commissioning still require testing
on the target board and controller.

## Wi-Fi and Thread radio coexistence


In mixed builds, ArdPortal binds Matter's IPv6 UDP port to the Thread network
interface after SDK startup. ESP-IDF's socket implementation does not apply
IPv6 packet-info interface selection on send; an unbound Matter socket can
otherwise follow the Wi-Fi default route when replying to a commissioner on
the home LAN. The binding uses the SDK's actual UDP port and lwIP's synchronized
`udp_bind_netif` API. It does not change global routes or HTTP/MQTT sockets.
`/api/matter/status` reports `threadUdpBound: true` after successful startup;
a missing Thread interface/socket or failed synchronization prevents Matter
startup with `Matter Thread UDP binding failed`. No persistent binding state is
added, and Wi-Fi-only/Thread-only builds omit this helper.

When Matter uses Thread and portal Wi-Fi is enabled, ArdPortal calls the SDK's `esp_coex_wifi_i154_enable()` after Matter/OpenThread starts. This explicitly enables arbitration of the shared Wi-Fi/IEEE 802.15.4 radio, matching the ESP-IDF OpenThread example. Enabling the coexistence Kconfig alone does not make this call. SDKs without the required support, or a failed initialization, report a startup error instead of silently running both radios without arbitration. Wi-Fi-only and Thread-only builds exclude this call. In mixed builds, `/api/matter/status` reports `wifiThreadCoexistence: true` after successful startup. Switching between Thread-only and Thread with the portal preserves the same Matter transport and endpoint layout; an existing Thread fabric need not be reset.

### Thread startup memory admission

Thread SDK initialization is deferred while portal Wi-Fi association or pending configuration writes are active. Before starting, ArdPortal reserves 40 KiB of temporary startup work (60 KiB when the Wi-Fi portal is compiled) with an 8 KiB contiguous block in addition to the normal network heap/DMA reserves. If memory is insufficient, status reports `Waiting for Matter startup memory` and the portal stays available. On constrained mixed Thread/Wi-Fi devices, configure station Wi-Fi and let the automatic fallback AP stop before retrying commissioning. This admission check does not remove an already commissioned fabric.

The mixed profile initializes an uninitialized portal Wi-Fi driver with four static RX buffers, eight dynamic RX/TX buffers and a receive BA window of six. An application-owned, already initialized driver keeps its configuration.


The source-built `thread-wifi` profile uses 32 OpenThread message buffers while retaining standard SDK event-log sizes. The lower message-buffer count leaves more memory for concurrent Wi-Fi, Thread and BLE commissioning and reduces queued network traffic. ArdPortal Messages and its optional Matter traffic history keep their own limits.


The mixed source profile also enables the SDK's `CHIP_SYSTEM_CONFIG_POOL_USE_HEAP`: Matter object pools allocate entries on demand. Their counts can grow until heap is exhausted rather than being bounded by the preallocated pool sizes; actual runtime savings depend on active sessions/subscriptions. On builds without PSRAM where the SDK enables RTC fast RAM as heap, CPU-owned ArdPortal allocations and the SDK’s CPU-only data-model allocator prefer that heap and fall back to the ordinary heap. Radio/AES DMA allocations retain DMA-capable SRAM. Neither policy depends on application field names or values.

## Commissioning while the portal uses AP mode

The setup AP can display the locally generated development QR/manual code before
the ESP joins home Wi-Fi. The phone should return to the home Wi-Fi before HA
commissioning; remaining connected only to the ArdUI AP does not provide the
usual path to HA and the Thread border router.

| Build | Network needed by the ESP | Provisioning |
| --- | --- | --- |
| `wifi` | Home Wi-Fi for operational Matter traffic | Configure it before Matter startup, or let the commissioner provide credentials through BLE. |
| `thread` | Thread; no ESP Wi-Fi | BLE commissioning with a Thread border router and the correct network credentials on the phone. |
| `thread-wifi` | Thread for Matter; Wi-Fi for portal access | Portal Wi-Fi settings do not supply Thread credentials. The phone/controller provisions Thread separately through BLE. |

In mixed Thread/Wi-Fi builds, a saved home station connection with the fallback
AP stopped leaves more memory for commissioning than an active AP. BLE, Thread
and Wi-Fi coexistence still depends on the target and available heap; an AP
being reachable alone does not prove commissioning can finish. Keep the portal
active throughout commissioning if continuous web access is required.

After provisioning, an application may call `portal.suspendPortal()` to stop the
Wi-Fi portal while Matter over Thread continues. `portal.resumePortal()` restores
the previous AP/reconnect policy, and `portal.portalSuspended()` reports the
state. These operations require Wi-Fi compiled in, are called from the application
loop, and do not change Thread credentials or ICD selection. Matter-over-Wi-Fi
builds reject suspension. See [Portal API](Portal-API#suspending-portal-networking).
