// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT

/*
 * HomeAssistantEntities
 *
 * Purpose: Expose an uptime sensor and GPIO indicator to HA without custom portal pages.
 *
 * Demonstrates: addAppConfigEntity(), runtime sensor updates and shared value-change callbacks.
 *
 * Requirements: ESP8266/ESP32, MQTT broker and HA MQTT integration; adapt GPIO2 to your board.
 *
 * What to expect: HA receives uptime updates every 10 seconds; the indicator switch controls
 * the GPIO.
 */

// HA entities without dynamic portal pages. Wi-Fi/MQTT settings stay in the portal.
#include <ArdPortal.h>

ArdPortal portal;
constexpr uint8_t IndicatorPin = 2;
uint32_t lastReport = 0;
const char UPTIME_ENTITY[] PROGMEM = R"JSON({
  "id":"uptime", "name":"Uptime", "type":"text", "class":"duration", "default":0
})JSON";
const char INDICATOR_ENTITY[] PROGMEM = R"JSON({
  "id":"indicator", "name":"Indicator", "type":"switch", "default":false
})JSON";

/**
 * @brief Initialize the example hardware, callbacks and portal.
 * @return No value.
 */
void setup() {
  Serial.begin(115200);
  pinMode(IndicatorPin, OUTPUT);
  digitalWrite(IndicatorPin, LOW);
  if (!portal.addAppConfigEntity(FPSTR(UPTIME_ENTITY)) || !portal.addAppConfigEntity(FPSTR(INDICATOR_ENTITY))) {
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

/**
 * @brief Advance the component work; call repeatedly from the Arduino main loop.
 * @return No value.
 */
void loop() {
  portal.loop();
  // UART, sensors and other application work get control after every portal pass.
  if (portal.portalAndAppConfigReady() && uint32_t(millis() - lastReport) >= 10000) {
    lastReport = millis();
    portal.setAppConfigStateValue("uptime", millis() / 1000);
  }
}
