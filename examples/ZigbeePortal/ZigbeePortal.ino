// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
// ESP32-C5/C6 4 MiB: select Zigbee ED mode; keep the included partitions.csv.
// Arduino IDE: select Minimal SPIFFS (1.9MB APP with OTA) for its size check.
// For router: set ARDPORTAL_ZIGBEE_ROUTER=1, select ZCZR mode.
#ifndef ARDPORTAL_ENABLE_ZIGBEE
#define ARDPORTAL_ENABLE_ZIGBEE 1
#endif
#ifndef ARDPORTAL_ZIGBEE_ROUTER
#define ARDPORTAL_ZIGBEE_ROUTER 0
#endif
#ifndef ARDPORTAL_ENABLE_MQTT
#define ARDPORTAL_ENABLE_MQTT 0
#endif
#ifndef ARDPORTAL_ENABLE_HA
#define ARDPORTAL_ENABLE_HA 0
#endif
#ifndef ARDPORTAL_ENABLE_CONSOLE_MESSAGES
#define ARDPORTAL_ENABLE_CONSOLE_MESSAGES 1
#endif
#ifndef ARDPORTAL_ENABLE_ZIGBEE_CONVERTER
#define ARDPORTAL_ENABLE_ZIGBEE_CONVERTER 1
#endif
#include <ArdPortal.h>
ArdPortal portal;
/** @brief Install diagnostics and start portal. @return No value. */
void setup() {
  Serial.begin(115200);
  portal.onZigbeeOperationFinished([](ArdPortal::ZigbeeOperation operation, bool success, const String& error) {
    Serial.printf("Zigbee operation=%u result=%s error=%s\n", unsigned(operation), success ? "OK" : "FAILED", error.c_str());
  });
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  // Register all pages before Zigbee freezes its endpoints. Use flash definitions.
  portal.onAppConfigValueChanged([](const String& id, const ArdJSON::JSONVar& value, ArdPortal::ChangeSource source) {
    Serial.printf("Field %s source=%u value=%s\n", id.c_str(), unsigned(source), ArdJSON::JSON.stringify(value).c_str());
    // Drive real hardware here. Report actual sensor/actuator state with
    // portal.setAppConfigStateValue(id.c_str(), state).
  });
#if ARDPORTAL_CONTROL_SUPPORT_ACTIONS
  portal.onAppConfigCommand([](const String& id, const String&, const ArdJSON::JSONVar& value, ArdPortal::ChangeSource) {
    Serial.printf("Action %s: %s\n", id.c_str(), ArdJSON::JSON.stringify(value).c_str());
    // This example has no lock/cover hardware, so do not acknowledge movement.
    return false;
  });
#endif
  portal.onAppConfigPageRegistrationFinished([](bool success) {
    if (success) Serial.println("Native page registered");
    else {
      const auto& error = portal.appConfigRegistrationError();
      Serial.printf("Registration failed: stage=%s field=%s reason=%s\n", error.stage, error.fieldId.c_str(), error.reason.c_str());
    }
  });
  bool accepted = portal.startAppConfigPageRegistration(F(R"JSON({"id":"native","name":"Native Zigbee","order":0,"fields":[
    {"id":"relay","type":"switch","name":"Relay","default":false},
    {"id":"setpoint","type":"slider","name":"Setpoint","min":0,"max":100,"step":1},
    {"id":"profile","type":"select","name":"Profile","options":[{"value":"normal","name":"Normal"},{"value":"eco","name":"Eco"}]},
    {"id":"temperature","type":"text","name":"Temperature","textType":"temperature","default":20.5},
    {"id":"humidity","type":"text","name":"Humidity","textType":"humidity","default":50},
    {"id":"contact","type":"binary_sensor","name":"Contact","default":false},
    {"id":"rgb","type":"light","name":"RGB light"},
    {"id":"thermostat","type":"climate","name":"Thermostat","min":10,"max":30,"step":0.5,"modes":["off","heat","cool","auto"]},
    {"id":"blind","type":"cover","name":"Blind"},
    {"id":"door","type":"lock","name":"Door lock"},
    {"id":"alarm","type":"siren","name":"Siren"},
    {"id":"fan","type":"fan","name":"Fan"},
    {"id":"valve","type":"valve","name":"Valve"},
    {"id":"humidifier","type":"humidifier","name":"Humidifier"},
    {"id":"water_heater","type":"water_heater","name":"Water heater"},
    {"id":"alarm_panel","type":"alarm_control_panel","name":"Alarm panel"},
    {"id":"vacuum","type":"vacuum","name":"Vacuum"},
    {"id":"mower","type":"lawn_mower","name":"Mower"},
    {"id":"action","type":"button","name":"Action"},
    {"id":"scene","type":"scene","name":"Scene"},
    {"id":"presence","type":"device_tracker","name":"Presence"},
    {"id":"firmware","type":"update","name":"Firmware"}
  ]})JSON"));
  if (!accepted) Serial.println(portal.appConfigRegistrationError().reason);
#endif
  if (!portal.begin()) Serial.println("Could not start portal");
}
/** @brief Advance networking and application callbacks. @return No value. */
void loop() {
  portal.loop();
}
