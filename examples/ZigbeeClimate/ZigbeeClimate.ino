// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
// ESP32-C5/C6, at least 4 MiB flash. Keep partitions.csv with this sketch.
// Select Zigbee ED and Minimal SPIFFS (1.9MB APP with OTA) in Arduino IDE.
// Router alternative: ARDPORTAL_ZIGBEE_ROUTER=1 and Zigbee ZCZR board mode.
#ifndef ARDPORTAL_ENABLE_ZIGBEE
#define ARDPORTAL_ENABLE_ZIGBEE 1
#endif
#ifndef ARDPORTAL_ZIGBEE_ROUTER
#define ARDPORTAL_ZIGBEE_ROUTER 0
#endif
// This example communicates over Zigbee without an MQTT broker or HA Discovery.
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
bool pageRegistered = false;
bool heating = false;
uint32_t lastRefresh = 0;

/**
 * @brief Run a simulated heating thermostat and report its actual state.
 * @param None; reads the registered slider and climate values from AppConfig.
 * @return No value. Invalid/unavailable values defer the update.
 */
void refreshThermostat() {
  if (!pageRegistered || !portal.portalAndAppConfigReady()) return;
  double room, target;
  const auto reading = portal.getAppConfigValue("room_temperature");
  auto state = portal.getAppConfigValue("thermostat");
  if (!reading.toDouble(room) || !state.isValid() || !state["temperature"].toDouble(target)) return;
  const String mode = state["mode"].asString();
  if (mode == "off") heating = false;
  else if (mode == "heat") {
    if (room < target - 0.2) heating = true;
    else if (room > target + 0.2) heating = false;
  } else return;
  // No GPIO is driven. Replace this simulation with your sensor/relay driver.
  state["current_temperature"] = room;
  state["action"] = mode == "off" ? "off" : heating ? "heating"
                                                    : "idle";
  // RAM-only telemetry: no ArdFS write. Zigbee is notified even with MQTT off.
  if (state.isValid() && !portal.setAppConfigStateValue("thermostat", state, false))
    Serial.println("Thermostat state temporarily rejected");
}

/**
 * @brief Register slider/climate definitions and start the portal with automatic pairing.
 * @param None.
 * @return No value; registration/network results are reported through callbacks.
 */
void setup() {
  Serial.begin(115200);
  portal.onZigbeeOperationFinished([](ArdPortal::ZigbeeOperation operation, bool ok, const String& error) {
    Serial.printf("Zigbee operation=%u result=%s error=%s\n", unsigned(operation), ok ? "OK" : "FAILED", error.c_str());
  });
  portal.onAppConfigPageRegistrationFinished([](bool ok) {
    pageRegistered = ok;
    if (ok) Serial.println("Slider and climate registered");
    else {
      const auto& e = portal.appConfigRegistrationError();
      Serial.printf("Page failed: stage=%s field=%s reason=%s\n", e.stage, e.fieldId.c_str(), e.reason.c_str());
    }
  });
  portal.onAppConfigValueChanged([](const String& id, const ArdJSON::JSONVar& value, ArdPortal::ChangeSource source) {
    Serial.printf("%s source=%u value=%s\n", id.c_str(), unsigned(source), ArdJSON::JSON.stringify(value).c_str());
    // Portal and Zigbee edits use this same callback, with their actual source.
  });
  if (!portal.startAppConfigPageRegistration(F(R"JSON({
    "id":"climate_demo","name":"Zigbee climate demo","order":0,
    "fields":[
      {"id":"room_temperature","type":"slider","name":"Simulated room temperature",
       "min":15,"max":30,"step":0.5,"default":20.5,"persist":false},
      {"id":"thermostat","type":"climate","name":"Thermostat",
       "min":15,"max":30,"step":0.5,"modes":["off","heat"],"persist":true,
       "default":{"temperature":22,"current_temperature":20.5,"mode":"off","action":"off"}}
    ]
  })JSON"))) Serial.println(portal.appConfigRegistrationError().reason);
  if (!portal.begin()) Serial.println("Portal startup failed");
}

/**
 * @brief Advance the portal/Zigbee stack and refresh simulation every 250 ms.
 * @param None.
 * @return No value.
 */
void loop() {
  portal.loop();
  const uint32_t now = millis();
  if (uint32_t(now - lastRefresh) >= 250) {
    lastRefresh = now;
    refreshThermostat();
  }
}
