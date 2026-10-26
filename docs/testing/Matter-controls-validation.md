# Matter control hardware validation

The opt-in `ARDPORTAL_MATTER_CONTROL_VALIDATION=ON` build selects
`tests/device/MatterControls.ino` instead of the small example. Normal builds
exclude the fixture. Use the source SDK project's Wi-Fi/Thread profile options.
Changing this endpoint inventory requires resetting Matter with the old image
before flashing. Portal Wi-Fi configuration is retained.

The fixture registers 16 supported endpoints and two application-only fields
(`sample` and the simulated drive action record `last_command`). Expected status: `endpoints=16`, `unsupported=2`, no startup
error. Its page is `matter_test`.

| ID | Mapping | HA → application checks | Application → HA checks |
| --- | --- | --- | --- |
| T01 | Outlet | off/on | off/on |
| T02 | On/off light | off/on | off/on |
| T03 | Brightness slider | off/on, 0/intermediate/100%, remembered brightness | 0%, 75% |
| T04 | RGB light | off/on, brightness, red/green/blue | off/red, on/blue |
| T05 | Temperature | read-only | −5.5, 27.25 °C |
| T06 | Humidity | read-only | 25, 67.5% |
| T07 | Pressure | read-only | 1025, 1001 hPa |
| T08 | Illuminance | read-only | 0, 1000 lux |
| T09 | Contact | read-only | closed/open, polarity |
| T10 | Occupancy | read-only | clear/detected |
| T11 | Leak | read-only | dry/wet |
| T12 | Fan | off/on, low/medium/high or percentage | off, 66% |
| T13 | Heating thermostat | off/heat, setpoint | mode, setpoint, measured temperature |
| T14 | Cooling thermostat | off/cool, setpoint | mode, setpoint, measured temperature |
| T15 | Heating/cooling thermostat | off/heat/cool/heat_cool, low/high setpoints | mode, low/high setpoints, measured temperature |
| T16 | Lift/tilt cover | open/close/stop, intermediate lift/tilt | closed, 75% lift / 25% tilt |

Use `GET /api/app?page=matter_test` to verify application values after each HA
command. Change `sample` via the application API to invoke the real
`setAppConfigStateValue()` path for every endpoint, including read-only sensors:

```json
{"page":"matter_test","values":{"sample":1}}
```

POST that body to `/api/app` with JSON content type. Repeat with `sample=0`,
then `sample=1`; stable snapshots avoid timers overriding HA commands. Inspect
HA entity state/attributes and compare with the application API. The fixture
logs rejected snapshot updates over serial. Fan oscillation/direction are not
exposed by the Matter adapter; they are not part of the fan success criteria.

Local tests validate every fixture definition and both snapshots using the real
control validator and Matter mapping, plus semantic class aliases. Hardware
results must be recorded separately: passing compilation or local tests does
not prove discovery, presentation or bidirectional synchronization in HA.

## Hardware results — 2026-10-26

ESP32-C5 DevKit, source SDK Wi-Fi/development/connected profile, BLE retained,
Home Assistant Core 2026.10.x / Matter Server 9.2.0. All 16 endpoints were added
through HA's on-network commissioning UI. This run tests the shared control
adapter over Wi-Fi; it does not constitute a 16-endpoint Thread hardware test.
All actuator and sensor values in this fixture are simulated.

| Coverage | Observed result |
| --- | --- |
| Discovery | All 16 primary endpoints appeared, with appropriate control/sensor domains. |
| T01/T02 | Off/on from HA confirmed by application API; high/low application snapshots returned to HA. |
| T03 | HA off/on preserved remembered level 75; HA slider 40 reached application as 40; application 0/75 reported back. |
| T04 | HA brightness 70% reached application as 179/255; red/green/blue commands arrived with HS quantization. Application red/blue and off/on reported back. A settled off/on cycle restored 180/255; transition samples briefly approach zero. |
| T05/T06 | −5.5 and 27.25 °C (HA display 27.3); humidity 25 and 67.5%. |
| T07 | Application 1025/1001 hPa displayed as 102.50/100.10 kPa in HA, an equivalent unit conversion. |
| T08 | Positive 100/1000 lx report correctly. Raw Matter measured value 0 for an application 0 lx is rendered about 1 lx by [HA's logarithmic conversion](https://github.com/home-assistant/core/blob/dev/homeassistant/components/matter/sensor.py); this is a presentation limitation, not an exact 0-lx display pass. |
| T09–T11 | Both polarities checked: closed/open contact, clear/occupied, dry/leak. |
| T12 | Off/on, 50% and Low→33% commands confirmed in application; application off/66% returned to HA. |
| T13/T14 | HA setpoint changes 23→24 °C and 25→24 °C; off commands; application heat/cool modes, targets and measured temperatures returned to HA. |
| T15 | HA independently changed low/high from 20/26 to 20.5/25.5 °C. Selecting heat/cool chose the corresponding application target; application heat_cool/off and measured temperature returned to HA. |
| T16 | OPEN/CLOSE reached simulated drive handler and attained 100/0%. STOP confirmed at a middle position. Lift 25% / tilt 75% reached application and returned to HA without a stuck moving state. Configuration status became OK. STOP at an already reached end limit is a no-op in the SDK's target-attribute-based command handling. |
| Restart | Pairing survived firmware updates/restarts. The final version reconciled restored SDK attribute caches with application defaults, including dimmer, fan, thermostats and cover. |
| HTTP fixture | Moving bulk snapshot application out of the HTTP callback changed a timed-out request into HTTP 202 in 28 ms in the measured retry. |

### Corrections made during the run

- Initialize the SDK's public lux-based light-sensor API with its valid minimum
  (1 lx), then publish actual readings, including raw zero, through attributes.
- Mark the lift/tilt cover operational and position-aware. Compare configuration
  before writing: the SDK returns `ESP_ERR_NOT_FINISHED` for unchanged values,
  which its Arduino wrapper otherwise treats as failure on the next boot.
- Report cover motion/stopped state from application state, avoiding a stale
  moving indication after an attained position.
- Reconcile SDK C++ endpoint caches from restored attributes before publishing
  authoritative application state. Without this, matching constructor caches
  could suppress necessary writes after a reboot. No per-endpoint cache buffer
  was added; only compiled-in writable families perform reconciliation.

HA presents the single fan and cover using the root device name, despite their
endpoint labels appearing on diagnostic entities. HA's primary fan/cover
entities deliberately use a `None` name; its base naming path can suppress the
label postfix. See the official [entity naming implementation](https://github.com/home-assistant/core/blob/dev/homeassistant/components/matter/entity.py),
[fan description](https://github.com/home-assistant/core/blob/dev/homeassistant/components/matter/fan.py)
and [cover description](https://github.com/home-assistant/core/blob/dev/homeassistant/components/matter/cover.py).
No local HA renaming was used to hide this result.

The fixture clears only the adapter layout hash once using a versioned test
marker, because a reset old image may recreate its old hash before upload.
Reset Matter before installing this new inventory; this helper does not clear
existing fabrics and is excluded from ordinary firmware.

Final fixture image: 2,249,312 bytes. At the final sample, free heap was 40,676
bytes (minimum 33,192), DMA free 24,896 (minimum 17,412), uptime 370 seconds.
Those figures describe this full test fixture after a commissioned restart,
not the commissioning peak or the smaller normal example. The final low-snapshot
HTTP request returned 202 in 19 ms. All Python Matter regression tests, the full
host suite and the Wi-Fi SDK/link-map check passed on the final source state.
The device remains on the full validation fixture, paired to HA, with the low
snapshot selected (actuators off, binary sensors clear).
