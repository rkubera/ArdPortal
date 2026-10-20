# Zigbee slider and climate demo

Use an ESP32-C5/C6 with at least 4 MiB flash and Arduino-ESP32 3.3.12.
Open **File → Examples → ArdPortal → ZigbeeClimate**. Keep the included
`partitions.csv`, select **Zigbee ED**, and select **Minimal SPIFFS (1.9MB APP with OTA)**
for the IDE application-size check. Upload over USB when first applying this
partition layout. A router build requires `ARDPORTAL_ZIGBEE_ROUTER=1` and **Zigbee ZCZR**;
configure Wi-Fi before relying on router operation because router/SoftAP coexistence
is unsupported by the bundled stack.

Enable joining on your coordinator. ArdPortal automatically pairs after startup,
or rejoins its saved network. The `/zigbee` page provides pairing controls and logs.
MQTT and HA Discovery are disabled in this example; the Wi-Fi portal remains available.

The example registers one page with two fields:

- `room_temperature`: slider 15–30 °C, step 0.5. It simulates a sensor reading and
  is RAM-only. Zigbee uses an Analog Output endpoint; gateway support for this
  generic cluster is required to display/edit it.
- `thermostat`: native Thermostat endpoint, target 15–30 °C, step 0.5, off/heat.
  Target/mode changes persist through ArdFS. The sketch reports current temperature
  and heating activity from a simulation with ±0.2 °C hysteresis. No physical relay
  or temperature sensor is driven.

Set the thermostat to heat, then lower/raise the simulated room temperature with
the slider. The reported current temperature and heating state change accordingly.
You can use the portal slider if the gateway does not expose Analog Output.
Native Zigbee activity distinguishes heating from no active heating; idle/off
share zero relay bits, while the selected system mode distinguishes off.

Telemetry refresh uses `setAppConfigStateValue()` and never schedules an ArdFS write.
A persistent thermostat edit saves that field’s complete climate object, including
its latest temperature/activity values. The same value-change callback logs
Portal and Zigbee changes. Physical gateway interoperability still needs testing.

CLI compilation with the included partition table (replace `esp32c6` with `esp32c5` for C5):

```sh
arduino-cli compile \
  --fqbn esp32:esp32:esp32c6:ZigbeeMode=ed,PartitionScheme=zigbee \
  --build-property upload.maximum_size=1835008 examples/ZigbeeClimate
```

[Zigbee documentation](https://github.com/rkubera/ArdPortal/wiki/Zigbee)
