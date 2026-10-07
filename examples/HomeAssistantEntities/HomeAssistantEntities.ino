// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
// HA entities without dynamic portal pages. Wi-Fi/MQTT settings stay in the portal.
#include <ArdPortal.h>

ArdPortal portal;
constexpr uint8_t IndicatorPin = 2;
uint32_t lastReport = 0;
const char UPTIME_ENTITY[] PROGMEM = R"JSON({
  "id":"uptime", "name":"Uptime", "type":"text", "default":0,
  "unit_of_measurement":"s", "state_class":"measurement"
})JSON";
const char INDICATOR_ENTITY[] PROGMEM = R"JSON({
  "id":"indicator", "name":"Indicator", "type":"switch", "default":false
})JSON";

void setup() {
  Serial.begin(115200);
  pinMode(IndicatorPin, OUTPUT);
  digitalWrite(IndicatorPin, LOW);
  if (!portal.addAppConfigEntity(FPSTR(UPTIME_ENTITY)) ||
      !portal.addAppConfigEntity(FPSTR(INDICATOR_ENTITY))) {
    Serial.println(F("Invalid HA entity definition."));
  }
  portal.onAppConfigValueChanged([](const String& key,
      const ArdJSON::JSONVar& value, ArdPortal::ChangeSource) {
    if (key == "indicator") digitalWrite(IndicatorPin, value.asBool() ? HIGH : LOW);
  });
  portal.onPortalAndAppConfigReady([](bool) {
    digitalWrite(IndicatorPin, portal.getAppConfigValue("indicator").asBool() ? HIGH : LOW);
  });
  ArdPortal::Options options;
  options.haWorkBudgetMs = 2;
  options.haMaxOperationsPerLoop = 1;
  portal.begin(options);
}

void loop() {
  portal.loop();
  // UART, sensors and other application work get control after every portal pass.
  if (portal.portalAndAppConfigReady() && uint32_t(millis() - lastReport) >= 10000) {
    lastReport = millis();
    portal.setAppConfigStateValue("uptime", millis() / 1000);
  }
}
