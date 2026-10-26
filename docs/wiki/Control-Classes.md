# Control classes and shared model

[Control catalogue](Controls) · [Feature flags](Defines) · [Zigbee](Zigbee)

`type` selects a control and its value/command structure. Optional `class` describes its meaning. The pair is shared by the portal, MQTT/Home Assistant and Zigbee adapters. The optional [Matter adapter](Matter) consumes the same normalized model and is compiled only when enabled.

Every control accepts omission of `class`. Unknown, empty, non-string and type-incompatible classes are registration errors. The class catalogue is removed with `ARDPORTAL_ENABLE_CLASSES=0`; ordinary controls continue to work. Class names, units and mapping metadata are compiled only when a corresponding `ARDPORTAL_ENABLE_CONTROL_*` is enabled.

## Presets and protocol adapters

Built-in descriptor presets have one protocol-independent authoring catalogue:
`src/tools/control_models.json`. It defines initial state, named actions/properties,
command suffixes, payloads, options and ranges. `class` remains an optional field
property, selected by the application.

`src/tools/generate_state_schemas.py` compiles that catalogue into the existing
control arrays and HA Discovery defaults. The HA adapter in
`src/tools/control_protocols.py` derives action payloads, topic suffixes, property
templates, option lists and supported capabilities from the same preset. Semantic
names used by the generator are removed before emitting firmware resources. No
additional model is parsed on the microcontroller. Control and HA defines still
exclude their corresponding resources.

Presence vocabulary (`states.present` / `states.absent`) is compiler metadata,
independent of the preset's initial `default`; it is omitted from runtime JSON.
Fan power/oscillation and humidifier power payloads follow their shared `on`/`off`
values. Default protocol payloads are omitted from Discovery to keep resources
compact; customized payloads are emitted automatically.

For example, the button's `press` action defines `payload: "PRESS"` once. The
portal/Zigbee control descriptor receives that action, while the HA adapter emits
`payload_press` and its command topic. The optional [Matter adapter](Matter) consumes this same catalogue.

This compilation applies to the library's built-in presets. An application's
runtime JSON can still replace `controls`; it is not passed through the Python
generator. If that customization changes the wire protocol, keep any explicit
`ha` overrides consistent with it. HA-specific overrides remain available for
external topics, custom templates or other integration-specific behavior. They
are not required when using the standard presets.

## All 30 controls

| Type | Optional classes | Without class |
| --- | --- | --- |
| `slider` | See the numeric setting catalogue below | Generic numeric setting |
| `text` | See the reading catalogue below | Plain text (strings or finite numbers) |
| `select` | None; omit `class` | Existing type preset |
| `edit` | None; omit `class` | Existing type preset |
| `switch` | `switch`, `outlet`, `light`, `pump` | Generic On/Off output |
| `climate` | None; omit `class` | Existing type preset |
| `binary_sensor` | `moisture`, `carbon_monoxide`, `power`, `gas`, `light`, `opening`, `door`, `window`, `garage_door`, `lock`, `tamper`, `motion`, `occupancy`, `presence`, `moving`, `smoke`, `heat`, `cold`, `safety`, `problem`, `plug`, `battery_low`, `battery_charging`, `connectivity`, `running`, `update`, `sound`, `vibration` | Generic boolean sensor |
| `date` | None; omit `class` | Existing type preset |
| `time` | None; omit `class` | Existing type preset |
| `datetime` | None; omit `class` | Existing type preset |
| `button` | `update`, `identify`, `restart` | Existing type preset |
| `scene` | None; omit `class` | Existing type preset |
| `notify` | None; omit `class` | Existing type preset |
| `infrared` | `transmitter`, `receiver`, `transceiver` | Transmitter |
| `event` | `motion`, `button`, `doorbell` | Existing type preset |
| `device_trigger` | `motion`, `button`, `doorbell` | Existing type preset |
| `tag` | None; omit `class` | Existing type preset |
| `device_tracker` | None; omit `class` | Existing type preset |
| `light` | None; omit `class` | Existing type preset |
| `fan` | None; omit `class` | Existing type preset |
| `cover` | `door`, `window`, `awning`, `blind`, `curtain`, `damper`, `garage`, `gate`, `shade`, `shutter` | Existing type preset |
| `valve` | `water`, `gas`, `irrigation` | Existing type preset |
| `lock` | None; omit `class` | Existing type preset |
| `alarm_control_panel` | None; omit `class` | Existing type preset |
| `humidifier` | `humidifier`, `dehumidifier` | Humidifier |
| `siren` | None; omit `class` | Existing type preset |
| `vacuum` | None; omit `class` | Existing type preset |
| `lawn_mower` | None; omit `class` | Existing type preset |
| `water_heater` | None; omit `class` | Existing type preset |
| `update` | `firmware`, `software` | Existing type preset |

## Reading catalogue (`text`)

All entries below are valid for `text`. `number` and physical numeric classes accept finite JSON numbers or `null` (unknown); numeric strings are rejected. Plain `text` keeps its original string/number behavior. `enum` needs a nonempty `options` array of strings or value/title objects. `date` is YYYY-MM-DD; `timestamp` includes a time zone. `uptime` is a nonnegative duration in seconds, mapped to HA duration rather than its boot-timestamp uptime class.

| Class | Default unit | Valid for slider |
| --- | --- | --- |
| `text` | — | No |
| `number` | — | Yes |
| `enum` | — | No |
| `temperature` | °C | Yes |
| `temperature_delta` | °C | Yes |
| `humidity` | % | Yes |
| `absolute_humidity` | g/m³ | Yes |
| `moisture` | % | Yes |
| `pressure` | hPa | Yes |
| `atmospheric_pressure` | hPa | Yes |
| `illuminance` | lx | Yes |
| `irradiance` | W/m² | Yes |
| `sound_pressure` | dB | Yes |
| `aqi` | — | Yes |
| `carbon_dioxide` | ppm | Yes |
| `carbon_monoxide` | ppm | Yes |
| `nitrogen_dioxide` | µg/m³ | Yes |
| `nitrogen_monoxide` | µg/m³ | Yes |
| `nitrous_oxide` | µg/m³ | Yes |
| `ozone` | µg/m³ | Yes |
| `sulphur_dioxide` | µg/m³ | Yes |
| `radon` | Bq/m³ | Yes |
| `volatile_organic_compounds` | µg/m³ | Yes |
| `volatile_organic_compounds_parts` | ppm | Yes |
| `pm1` | µg/m³ | Yes |
| `pm25` | µg/m³ | Yes |
| `pm4` | µg/m³ | Yes |
| `pm10` | µg/m³ | Yes |
| `voltage` | V | Yes |
| `current` | A | Yes |
| `power` | W | Yes |
| `apparent_power` | VA | Yes |
| `reactive_power` | var | Yes |
| `power_factor` | — | Yes |
| `frequency` | Hz | Yes |
| `energy` | kWh | Yes |
| `reactive_energy` | varh | Yes |
| `energy_storage` | kWh | Yes |
| `energy_distance` | kWh/100km | Yes |
| `battery` | % | Yes |
| `flow` | m³/h | Yes |
| `volume` | m³ | Yes |
| `volume_storage` | m³ | Yes |
| `water` | m³ | Yes |
| `gas` | m³ | Yes |
| `speed` | m/s | Yes |
| `wind_speed` | m/s | Yes |
| `wind_direction` | ° | Yes |
| `distance` | m | Yes |
| `area` | m² | Yes |
| `precipitation` | mm | Yes |
| `precipitation_intensity` | mm/h | Yes |
| `weight` | kg | Yes |
| `conductivity` | µS/cm | Yes |
| `ph` | — | Yes |
| `blood_glucose_concentration` | mg/dL | Yes |
| `duration` | s | Yes |
| `data_size` | B | Yes |
| `data_rate` | B/s | Yes |
| `signal_strength` | dBm | Yes |
| `percentage` | % | Yes |
| `monetary` | — | Yes |
| `date` | — | No |
| `timestamp` | — | No |
| `uptime` | s | No |

## Numeric setting catalogue (`slider`)

All rows marked Yes above are supported numeric settings. Additional slider classes: `brightness`, `position`, `tilt`, `fan_speed`, `volume_level`, `color_temperature`, `angle`.

`min`, `max` and `step` remain explicit. Percentage-based settings use the declared range; class alone does not rescale values or create a complete light, thermostat or cover. Use the corresponding composite control when a complete device model is needed. `color_temperature` uses kelvins; `angle` uses degrees. `monetary` needs an explicit currency unit when exposed to HA.

## Examples

```json
{"id":"socket","name":"Socket","type":"switch","class":"outlet"}
{"id":"temperature","name":"Temperature","type":"text","class":"temperature"}
{"id":"limit","name":"Limit","type":"slider","class":"power","min":0,"max":2000,"step":10}
{"id":"door","name":"Door","type":"binary_sensor","class":"door"}
```

## Protocol boundaries

- **Shared model:** `ControlModel.h` owns presets, validation, persistence policy, actions and event vocabularies. `ControlClasses.h` exposes an allocation-free four-byte `Model` (type, class, capabilities). Values remain in the existing AppConfig state; adapters do not allocate parallel state trees. Custom control arrays replace preset capabilities.
- **HA:** `control_protocols.py` compiles preset Discovery defaults; `HaEntities.h`, `HaClassData.h` and `HaEntityData.h` map classes and resolve runtime topics. Protocol keys and topics are absent from shared presets. `class` supplies default units and measurement metadata where applicable. `ha` supplies explicit Discovery overrides; a measurement class cannot infer whether energy/volume values are totals or instantaneous readings. Choose such statistical overrides deliberately; `entity_category` is independent of physical meaning.
- **Zigbee:** `ZigbeeFields.h` owns native endpoint/cluster/wire conversion. `switch/outlet` uses the smart-plug endpoint identity; `switch/light` uses an On/Off light identity. Other scalar outputs remain On/Off. Cover classes select native awning/drapery/shutter/shade types where available; doors, gates, garage doors, dampers and windows use the general covering transport and are marked generic. Existing native measurements retain their canonical wire units. Additional numeric reading classes use generic Analog Input, settings use Analog Output, and binary classes use generic Binary Input.
- **Gateway exports:** schema 2 metadata includes class, canonical unit and native/generic mapping. Z2M and ZHA classify generic readings/settings/binary sensors without claiming a native occupancy, contact or IAS cluster. Grouped children do not inherit the parent device class. Z2M forwards outlet and cover metadata; ZHA supports the class enums exposed by the pinned gateway API. Unsupported gateway classes remain generic.
- **Matter:** see [supported mappings](Matter#mappings). A recognized class does not promise a native endpoint or certification.

## Actions, events and IR

`button` means a command to the application; `event` means an occurrence reported by the application. Classes identify meaning without implementing a restart, update or physical driver. Doorbell events default to `ring`, motion events to `motion`; explicit top-level `event_types` override the vocabulary. Trigger classes choose corresponding default payload/type values, and explicit `ha` overrides remain available. `device_trigger` keeps its HA automation configuration under `ha` instead of turning into an event entity.

IR `receiver` is read-only, has no send controls and accepts bounded signal objects through `emitAppConfigEvent()`: a nonempty array of nonzero signed microsecond `timings`, optionally positive `modulation` in Hz. HA publishes the receiver schema. `transmitter` preserves send behavior. `transceiver` permits send and receive at the application model level; the current single-entity HA adapter publishes its transmitter side. Use separate transmitter and receiver fields to discover both sides in HA. Zigbee has no native IR mapping.

## Breaking changes

- Replace `textType` with `class`; there is no fallback alias.
- Replace top-level `device_class` with the appropriate `class`, or put a deliberate HA-only override inside `ha.device_class`. The legacy top-level property is rejected.
- Replace `ARDPORTAL_ENABLE_TEXT_TYPES` with `ARDPORTAL_ENABLE_CLASSES`.
- Move `ha.event_types` to top-level `event_types`; state validation no longer depends on HA settings.
- Regenerate the layout-specific Z2M converter/ZHA quirk after upgrading. Schema 2 changes the layout fingerprint; a gateway may need a re-interview to refresh its cached model identity.

## Maintenance and validation

`src/tools/control_models.json` is the protocol-independent preset source; `src/tools/control_protocols.py` derives HA defaults from it. `src/tools/control_classes.json` defines semantic names, units and class mappings. Run `python3 src/tools/generate_state_schemas.py`, then the portal/language/gateway generators as applicable. `--check` verifies generated files without rewriting them.

Run `bash tests/run-host.sh`. The model suite covers all 30 controls and every allowed/disallowed class pair, optional classes, calendar/IR/event values, capabilities, HA-independent defaults and absence of disabled class names from compiled profiles. Arduino builds and the external ZHA/Z2M tests are described in [test instructions](../../tests/README.md).
