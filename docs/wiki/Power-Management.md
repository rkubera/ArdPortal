# Power Management

The power manager is an integral part of ArdPortal and is always available through `portal.power()`. Its default policy is `AlwaysOn`; configure a sleep policy explicitly to enable sleeping. Configure **when** the device may sleep independently from **how** it sleeps:

```cpp
#include <ArdPortal.h>

ArdPortal portal;

void setup() {
  ArdPower::Options power;
  power.policy = ArdPower::Policy::EventDriven;
  power.sleep = ArdPower::SleepType::ConnectedSleep;
  power.connectedSleep.allowCpuSleep = true;
  power.sleepDelayMs = 1000;
  power.maxIdleMs = 20;
  portal.power().configure(power);
  portal.begin();
}

void loop() { portal.loop(); }
```

Call configuration and application methods from the Arduino task together with `portal.loop()`. Only `signalFromISR(source)` is an ISR API. The manager owns the device-wide Wi-Fi/PM policy; do not run competing power managers.

## Policy: when to sleep

| Policy | Behaviour |
|---|---|
| `AlwaysOn` | Default. Forces the effective sleep type to `None`, adds no idle wait and refuses `requestSleep()`, regardless of the selected type. GPIO/ISR events and software timers are still dispatched. |
| `EventDriven` | Applies the selected type automatically after the idle grace period, when blockers are clear. CPU idle waits are limited by the time until the application next needs to run (`wakeIn()`) and registered timers. `DeepSleep` automatically restarts the device after its selected wake interval. |
| `Periodic` | Schedules application cycles at `periodic.intervalMs`. Sleeps between cycles after `sleepDelayMs`, subject to all blockers. RAM-retaining modes deliver a `Periodic` event; deep sleep restarts `setup()`. |
| `Manual` | Applies the selected radio policy, but adds no CPU wait/light/deep sleep until `requestSleep()` is accepted. The same admission checks still apply. |

## Sleep type: how to sleep

| Sleep type | Behaviour and platform support |
|---|---|
| `None` | Default. Disables Wi-Fi power saving, adds no idle wait and refuses explicit sleep requests. |
| `ConnectedSleep` | Keeps Wi-Fi or Zigbee end-device connectivity and saves radio power. Its `connectedSleep.allowCpuSleep` option additionally controls permission to sleep the CPU; see below. |
| `LightSleep` | Timer/GPIO-wake light sleep on ESP32, with RAM retained. Zigbee end devices use native stack-coordinated automatic light sleep; without Zigbee the manager enters standalone light sleep. The Wi-Fi radio must already be off; active Wi-Fi adds the `Network` blocker. The manager does not silently disconnect it. Not implemented on ESP8266: selecting it returns false; use `ConnectedSleep` there when Wi-Fi is connected. |
| `DeepSleep` | Timed deep sleep, optionally interrupted by one configured ESP32 GPIO. RAM and network sessions are lost; execution restarts in `setup()`. Automatic under `EventDriven`/`Periodic`, explicitly requested under `Manual`, suppressed under `AlwaysOn`. |

`ConnectedSleep` has its own options:

```cpp
power.sleep = ArdPower::SleepType::ConnectedSleep;
power.connectedSleep.allowCpuSleep = false; // Save radio power without enabling CPU light-sleep.
```

`allowCpuSleep` defaults to **true**, preserving connected CPU-sleep behaviour. With **false**, Wi-Fi uses modem-sleep and the manager adds no automatic CPU idle waits. An explicit `requestSleep()` may still add a cooperative wait, but does not enable CPU light-sleep. With **true**, ESP8266 uses `WIFI_LIGHT_SLEEP`; ESP32 requires `CONFIG_PM_ENABLE` and `CONFIG_FREERTOS_USE_TICKLESS_IDLE`, otherwise reports `ModemSleepFallback`. With false, `state()` reports `ModemSleep`, which is now a diagnostic state rather than a selectable sleep type.

The option applies only to `ConnectedSleep`; it does not change `None`, `LightSleep` or `DeepSleep`. Policies and all CPU-sleep admission checks still apply when it is true.

`setPolicy(policy)` and `setSleepType(type)` retain the other settings. `policy()` and `sleepType()` return the selections. `configure(options)` validates the whole configuration and returns false without changing it if invalid. Configuration changes cancel pending requests and restart the idle grace period.

`sleepDelayMs` is the inactivity delay before automatic sleep, not its duration. New activity or an active blocker restarts the delay; explicit `requestSleep()` skips it. It must be at most `INT32_MAX`; `maxIdleMs` must be 1–100. `ConnectedSleep` and standalone `LightSleep` use bounded slices so portal/network work continues between them. Without a connected Wi-Fi or Zigbee end device, `ConnectedSleep` only yields; choose `LightSleep` on ESP32 to enter standalone hardware sleep.

When connected light-sleep is supported on ESP32, the manager temporarily enables automatic PM light-sleep while preserving the current CPU frequency. Previous PM and Wi-Fi settings are restored when the manager is destroyed. A sketch define cannot rebuild missing precompiled SDK capabilities. `ConnectedSleep` reports permission to sleep, not proof of physical sleep entry: traffic, other tasks and SDK locks also matter.

## Periodic application cycles

```cpp
ArdPower::Options power;
power.policy = ArdPower::Policy::Periodic;
power.sleep = ArdPower::SleepType::ConnectedSleep;
power.periodic.intervalMs = 60000; // Scheduled work every minute.
power.sleepDelayMs = 1000;        // Remain active for 1 s after activity.
portal.power().configure(power);
portal.power().onWake([](const ArdPower::Event& event, void*) {
  if (event.kind == ArdPower::EventKind::Periodic) {
    // Read sensors / queue work. Use a power lock for asynchronous work/ACKs.
  }
});
```

The first interval starts at `configure()`. Perform or schedule the first application cycle in `setup()`; the first `Periodic` event arrives after `intervalMs`. Later events keep the original cadence. `notifyActivity()`, `wakeIn()` and blockers do not move that cadence. Activity restarts the awake grace period, so a busy application can stay awake through several intervals. A periodic callback does not imply that the network/storage is ready; defer network work and hold an application power lock until it finishes.

`intervalMs` defaults to 60000 and must be 1–`INT32_MAX`, greater than `sleepDelayMs`. Validation applies when `Periodic` is selected. With `DeepSleep`, the interval must also fit the hardware timer limit. Invalid configurations leave the old configuration in place.

`None` still delivers events but does not sleep. `ConnectedSleep` and `LightSleep` use bounded waits ending no later than the next cycle, an earlier software timer or `wakeIn()` deadline. Native Zigbee, active Wi-Fi, router and SDK capability restrictions remain unchanged. `connectedSleep.allowCpuSleep=false` still permits radio saving only; periodic events run from the normal loop.

If `loop()` is late, one `Periodic` event is delivered with `id=0` and `missed` equal to the number of additional skipped cycles. There is no burst of catch-up callbacks. As with other events, it clears an older explicit sleep request and restarts `sleepDelayMs`. It uses its own fixed slot and does not consume any of the eight application timers.

With `DeepSleep`, the automatic duration is the **remaining time until the next cycle**, replacing `deepSleepMs`; explicit requests and `wakeIn()` may shorten it further. Delayed network/storage work shortens the available sleep. Deep sleep restarts `setup()` and rebases the schedule when the sketch calls `configure()` again, so it is not a wall-clock schedule and shutdown/boot overhead can lengthen cycles. There is no RAM `Periodic` callback carried across a deep-sleep reboot. Prepare each cycle again from `setup()` and wait for required network acknowledgements before allowing sleep. GPIO/early timer wake also starts a new boot cycle.

`setPolicy()` / `setSleepType()` call `configure()` and restart the periodic schedule. `cancelSleep()` cancels an explicit request and restarts the awake grace period; automatic periodic sleep resumes afterwards. Sticky sleep errors are rearmed by the next periodic event just as by other activity. See the [periodic example](../../examples/PowerManagementPeriodic/PowerManagementPeriodic.ino).

## Automatic deep-sleep

```cpp
ArdPower::Options power;
power.policy = ArdPower::Policy::EventDriven;
power.sleep = ArdPower::SleepType::DeepSleep;
power.deepSleepMs = 60000;
power.sleepDelayMs = 1000;
portal.power().configure(power);
```

After the idle grace period and completion of portal work, the manager automatically closes MQTT, stops Wi-Fi and enters deep sleep. The period is `deepSleepMs` (default 60000), shortened when `wakeIn(ms)` says the application needs to run sooner. Once that time arrives, the manager stays awake until the application does its work and calls `wakeIn()` again or clears the limit with `notifyActivity()`. A valid ESP32 deep-wake pin may wake the device earlier than the timer.

The period must be nonzero and fit the platform's hardware timer limit. `setTimer()` software timers live in RAM and would be restarted on every boot, so they cannot be combined with `DeepSleep`. Use `deepSleepMs` or `wakeIn()` and perform work again after the reboot. Switching to deep sleep with active software timers or incompatible GPIO registrations returns false. Cancel/remove them first.

ESP8266 timer wake requires GPIO16 connected to RST. Arbitrary GPIO deep wake is unsupported there. ESP32 allows one `LightAndDeepSleep` GPIO; its level and the wake timer are both active. `addWakePin(pin, level)` chooses this scope automatically when the selected sleep type is `DeepSleep`.

See the [automatic deep-sleep example](../../examples/PowerManagementDeepSleep/PowerManagementDeepSleep.ino). It disables Wi-Fi to demonstrate battery-node operation without a provisioning AP keeping the device awake.

## Cause of the current boot

```cpp
const ArdPower::WakeCause cause = portal.power().wakeCause();
const bool resumed = portal.power().wokeFromDeepSleep();
```

Both methods are available from `setup()`, before `begin()` or `configure()`. The platform captures the SDK boot/reset cause when the manager is constructed. It remains unchanged by configuration, later light sleep, GPIO interrupts and periodic events; it is not the reason for the most recent light-sleep return. Use `onWake()` for registered GPIO/ISR/timer events during the current run.

| `WakeCause` | Meaning / support |
|---|---|
| `PowerOn` | SDK reports a power-on/default start. |
| `Timer` | ESP32 reports a deep-sleep timer wake. |
| `Gpio` | ESP32 reports a deep-sleep EXT0, EXT1 or GPIO wake. Does not identify the GPIO number. |
| `DeepSleep` | Deep-sleep wake with no more specific source available; used on ESP8266. |
| `ExternalReset` | SDK reports an external reset. |
| `SoftwareReset` | SDK reports a software restart. |
| `Watchdog` | SDK reports a hardware/software/task/interrupt watchdog reset. |
| `Panic` | SDK reports an exception/panic reset. |
| `Brownout` | ESP32 reports a brownout reset. |
| `OtherReset` | Another ESP32 reset reason, such as USB/JTAG/SDIO. |
| `OtherWake` | Another ESP32 deep-sleep wake source, such as touch or ULP. |
| `Unknown` | SDK cannot provide a mapped cause. |

`wokeFromDeepSleep()` is true for `Timer`, `Gpio`, `DeepSleep` and `OtherWake`. It reflects the SDK report, including wakes from sleep entered outside ArdPortal. It requires no RTC marker or flash writes. ESP8266 shares RST between timer wiring and external circuitry; this API deliberately does not label an ambiguous deep-sleep wake as `Timer` or `Gpio`. Reset diagnostics and power-loss classification are limited to what each SDK reports. These methods are always available.

## Manual sleep requests

```cpp
ArdPower::Options power;
power.policy = ArdPower::Policy::Manual;
power.sleep = ArdPower::SleepType::DeepSleep;
power.deepSleepMs = 60000;
portal.power().configure(power);
// After application work has been prepared:
portal.power().requestSleep();
// Continue calling portal.loop() while storage/network blockers clear.
```

`requestSleep(ms)` always requests the **selected** sleep type. It refuses `AlwaysOn`, `None`, an invalid duration/source combination or a second pending request. Passing/omitting zero selects `deepSleepMs` for deep sleep (`periodic.intervalMs` under `Periodic`), otherwise `maxIdleMs`. An explicit request can also be used under `EventDriven`/`Periodic` to skip the idle grace period.

For non-deep types, the requested interval starts when blockers first clear and is serviced in bounded slices. Portal services continue between slices. The time supplied through `wakeIn()` and software timers can shorten a slice. New events cancel the old request. Deep sleep is one irreversible entry followed by a reboot.

`cancelSleep()` cancels a pending request and resets the idle grace period while preserving the time previously supplied through `wakeIn()`. Under automatic policy it does not permanently disable future sleep; use a counted `lock()` or change policy to stay awake. A failed sleep attempt reports `Error` and is not retried on every loop; configuration, new activity/events or a new explicit request can rearm it.

## Application work and admission

`wakeIn(ms)` tells the manager **how soon your application needs to run its own code again**. For example:

```cpp
portal.power().wakeIn(5000); // I need to run my code in 5 seconds; do not sleep longer.
```

This only limits sleep. It does not call your function, invoke `onWake()` or perform a sensor reading. With light/connected sleep, your code must check the time in `loop()` and do the work. With deep sleep, the device reboots and your code runs again from `setup()`. Use `setTimer()` if you want a timer callback while RAM is retained.

Once those 5 seconds have passed, the manager stops scheduling sleep until you update the limit with another `wakeIn()` call or clear it with `notifyActivity()`. Each call counts from **now** and replaces the old limit: calling `wakeIn(5000)` on every loop keeps moving the target forward. Set it once after doing the work, or pass the remaining time until your next task. Values above `INT32_MAX` are rejected. This is a sleep limit, not a guarantee that application/SDK work will execute at an exact time.

`notifyActivity()` means "I have just done some work": it restarts `sleepDelayMs` and clears the old `wakeIn()` limit. `lock()` / `unlock()` are counted application locks, checked from portal loop; they are not ESP-IDF PM locks and do not disable Wi-Fi modem power saving.

CPU/deep-sleep blockers cover startup, pending storage/retries, OTA, restart, active AP, Wi-Fi connection/scan work, HTTP and WebSocket clients, pending MQTT work, application registration, active wake pins, incompatible edge interrupts and application locks. The manager does not automatically close the provisioning AP. MQTT QoS 0 handed to TCP does not prove broker receipt; applications requiring confirmation must keep a power lock until acknowledged.

See the [Zigbee example](../../examples/ZigbeePowerManagement/ZigbeePowerManagement.ino).

Zigbee end devices support `ConnectedSleep`: before stack startup the manager selects the sleepy end-device receiver profile, then toggles native Zigbee sleep under the stack lock. The native `ESP_ZB_COMMON_SIGNAL_CAN_SLEEP` callback calls `esp_zb_sleep_now()`; Arduino code never forces standalone light sleep beneath the stack. The existing 3000 ms keep-alive remains in effect; commands buffered by the parent can arrive later than with continuous reception. Parent buffering limits still apply.

Configure the sleep policy/type before `portal.begin()`. The SDK ignores `RxOnWhenIdle` changes after joining; both startup paths stage it before initialization/start. Since NVRAM can restore an older receiver capability, the startup path reapplies the staged ZBOSS receiver capability and node descriptor after `esp_zb_start(false)`, before the scheduler/BDB rejoin. This uses the same node-descriptor workaround as Arduino-ESP32 ZigbeeCore; runtime enabling verifies the SDK receiver state. A device started with `AlwaysOn`/`None` uses the full-receiver profile and blocks a later sleep request until restarted with a sleep profile selected. No leave or pairing reset is needed.

`AlwaysOn` and `None` disable native radio sleep. On a device started with the sleepy profile, its advertised receiver capability remains sleepy until restart: parent polling/buffering semantics are retained even while native sleep is disabled. `EventDriven`, `Periodic` and `Manual` enable radio saving once the end device is connected; `Manual` still requires `requestSleep()` for application idle waits. Pairing, rejoining and Zigbee OTA suppress radio saving. SDK lock contention defers the transition until a later loop.

`connectedSleep.allowCpuSleep=false` saves radio power without enabling automatic CPU light sleep. With `true`, CPU light sleep additionally requires `CONFIG_PM_ENABLE` and `CONFIG_FREERTOS_USE_TICKLESS_IDLE`; otherwise the manager reports `ModemSleepFallback`. In mixed Wi-Fi/Zigbee operation both connections must be ready, portal blockers must be clear, and the SDKs coordinate access to the radio. The scheduler and native PM locks decide whether the CPU can actually sleep. GPIO/ISR/timer events continue to work, but they do not promise immediate cancellation of the Zigbee task's native radio wait.

| Sleep type | Zigbee end device | Wi-Fi + Zigbee end device | Zigbee router |
|---|---|---|---|
| `None` | No native radio/CPU sleep. Startup receiver capability stays fixed. | Both networks active without managed sleep; Zigbee startup capability stays fixed. | Continuous reception. |
| `ConnectedSleep` | Native radio saving; CPU permission follows `allowCpuSleep`. | Both stacks coordinate radio access; CPU sleep requires both connections ready. | Blocks CPU sleep; Wi-Fi may save radio power. |
| `LightSleep` | Native Zigbee radio/task sleep with automatic CPU light-sleep permission. RAM and network identity retained. PM + tickless idle are required; otherwise `ModemSleepFallback`. | Blocked while Wi-Fi is active. Disable Wi-Fi first, or use `ConnectedSleep`. | Blocked. |
| `DeepSleep` | Waits for readiness, then sleeps and reboots with saved network data. | Same readiness checks, then closes MQTT and stops Wi-Fi before deep sleep. | Blocked. |

`AlwaysOn` suppresses every sleep type. `EventDriven` and `Periodic` enter sleep after `sleepDelayMs` and admission checks; `Manual` requires `requestSleep()`. For native Zigbee `LightSleep`, application waits use bounded scheduler-friendly slices rather than calling `esp_light_sleep_start()` beneath a live stack. `state()` reports permission/configuration, not a measurement of physical sleep.

Deep sleep waits at least 5 seconds after a successful connection, a fresh successful SDK `CAN_SLEEP` signal (within 100 ms), no pending lifecycle/application commands or reports, no outstanding Time request, and no pairing/rejoin/OTA. The common portal/storage/network/application blockers still apply. At final admission the manager acquires the native stack lock without waiting, rechecks readiness and holds it through sleep entry; contention defers the request. If sleep entry fails, the native lock is released and the manager reports `Error` until rearmed. Already closed MQTT/Wi-Fi may need normal reconnection.

The SDK owns persistent network state and writes it through its native storage backend. This path does not call leave, factory reset, erase NVRAM, or rewrite pairing datasets on every sleep. After wake, normal startup restores the saved identity and reattaches/rejoins; it does not preserve a live RAM session. A parent can age out a device during a long absence (the configured end-device timeout is 64 minutes, subject to parent support). Rejoin and additional traffic may therefore be necessary. Queues being empty is not proof of coordinator/application receipt: hold an application power lock until any required acknowledgement arrives.

See the [Zigbee deep-sleep example](../../examples/ZigbeeDeepSleep/ZigbeeDeepSleep.ino).

## GPIO, interrupts and timers

```cpp
void onPowerEvent(const ArdPower::Event& event, void*) {
  if (event.kind == ArdPower::EventKind::Timer && event.id == 1) {
    // Read a sensor. event.missed counts skipped periods if loop was delayed.
  } else if (event.kind == ArdPower::EventKind::Gpio) {
    // event.id is the GPIO number.
  } else if (event.kind == ArdPower::EventKind::Interrupt) {
    // event.id is an application ISR source, 0..15.
  }
}

// From setup(), after configuring the manager:
portal.power().onWake(onPowerEvent);
pinMode(4, INPUT_PULLUP);
portal.power().addWakePin(4, ArdPower::WakeLevel::Low);
portal.power().setTimer(1, 10000, true); // Repeating, every 10 seconds.
portal.power().setTimer(2, 3000);       // One-shot, in 3 seconds.
```

The manager supports four owned GPIO registrations, sixteen external ISR signal IDs and eight software timers. It uses fixed event/timer storage and no callback allocation. ESP32's GPIO driver can allocate its own ISR service during registration. Pin numbers are native GPIO numbers. Configure input direction and pulls before registration, and use GPIOs available on the actual board; do not share their interrupt handlers with another library.

- `addWakePin(pin, WakeLevel::Low/High, scope)` registers a **level** wake source and a GPIO event. It rejects invalid pins/scopes, duplicates and full capacity. The default scope is `LightAndDeepSleep` when the selected type is `DeepSleep`, otherwise `LightSleep`. Explicit light-only registrations and edge-only interrupt pins are rejected with `DeepSleep`. ESP32's optional `LightAndDeepSleep` scope requires a deep-wake-capable GPIO and is limited to one pin. ESP8266 accepts GPIO 0–15 except flash pins 6–11; GPIO16 does not support this interrupt mechanism.
- `addInterruptPin(pin, Interrupt::Rising/Falling/Change)` registers an ordinary edge interrupt, delivered as a `Gpio` event. Edge-only registrations block manager-controlled CPU/deep sleep because they do not establish a hardware sleep wake source. Wi-Fi modem-sleep can still operate.
- `removePin(pin)` removes the manager's handler and wake configuration. Only register pins exclusively owned by this manager. It does not change the configured pull resistors.
- `setTimer(id, ms, repeat)` adds or replaces a software timer. IDs are 0–255; intervals are 1–`INT32_MAX` ms. `cancelTimer(id)` removes it. These RAM timers are incompatible with `DeepSleep`: registration returns false, and switching to deep sleep with registered timers also returns false without changing the current configuration. Periodic timers keep their original cadence: a delayed loop produces one callback with a `missed` count instead of replaying every missed tick.
- `onWake(callback, argument)` installs a function pointer and optional caller-owned context. It receives events from `portal.loop()` under every policy, including `Manual`. Due events are snapshotted before callbacks; cancelling an already snapshotted event does not retract it. Future timers can be added/replaced/cancelled from the callback.

For an application-owned ISR, use the only ISR-safe manager API:

```cpp
void IRAM_ATTR dataReadyISR() {
  portal.power().signalFromISR(0);
}
```

The source must be 0–15; other IDs are ignored. Repeated signals from the same source coalesce before the next dispatch. This API records an event and wakes the manager's idle wait; it does not configure the peripheral's hardware wake source. Configure that separately, or hold an application power lock if that peripheral cannot wake the CPU. The manager instance must live in internal RAM and remain alive while its ISR is registered. Do not call callbacks, Serial, storage, timer registration or other manager methods from an ISR.

A held wake level prevents further sleep. Its level interrupt is masked until the pin is released, then rearmed from `loop()`; a held button generates one event rather than an interrupt storm. This is not mechanical-button debouncing. A new event resets the idle grace period and cancels an older deep-sleep request. For frequent timers, set `sleepDelayMs=0` if idle slices should still occur between ticks.

ESP32 uses a private static FreeRTOS semaphore for interruptible idle waits when static allocation is supported; it does not consume the application's task notifications. ESP8266 uses its core's interruptible cooperative delay. Registered GPIO wake sources work with the selected sleep type; they do not bypass router and active-Wi-Fi light-sleep restrictions.

Software timers use the monotonic `millis()` clock, handle its rollover, and are serviced when `portal.loop()` runs. They are not calendar/RTC alarms, real-time hardware interrupts or persistent deep-sleep schedules. Timers and callback registrations must be recreated after a deep-sleep restart; an application needing the boot wake cause can query the platform SDK.

## Diagnostics

- `policy()` / `sleepType()` report the independent selections.
- `state()` reports the last applied state, including `Awake`, `ModemSleep`, `ConnectedSleep`, `ModemSleepFallback`, `LightSleep` and `Error`. It starts as `Unmanaged` before service.
- `blockers()` returns a bit mask; `blockedBy(ArdPowerBlocker::Storage)` checks a reason.
- `sleepPending()` reports an explicit queued request; automatic sleep is evaluated directly from the selected policy/type.

The earlier mixed `Mode` enum, `setMode()` and `requestDeepSleep()` API have been replaced. The separate `SleepType::ModemSleep` has been replaced by `ConnectedSleep` with `connectedSleep.allowCpuSleep=false`. See the [GPIO/timer example](../../examples/PowerManagement/PowerManagement.ino). Hardware current consumption and wake reliability require testing on the target board; host tests and compilation do not measure them.
