// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
/*
 * Show and hide dynamic fields using visibleWhen single-field and grouped AND/OR dependencies.
 * With HA enabled, discovery is removed/recreated as field visibility changes.
 */

#include <ArdPortal.h>

ArdPortal portal;

static const char CONTROL_PAGE[] PROGMEM = R"JSON({
  "id": "controls",
  "name": "Controls",
  "order": 10,
  "fields": [
    {
      "id": "enabled",
      "type": "switch",
      "name": "Enabled",
      "default": false
    },
    {
      "id": "level",
      "type": "slider",
      "name": "Level",
      "min": 0,
      "max": 100,
      "step": 1,
      "default": 50,
      "visibleWhen": {
        "field": "enabled",
        "equals": true
      }
    },
    {
      "id": "alternative",
      "type": "switch",
      "name": "Alternative",
      "default": false
    },
    {
      "id": "grouped_note",
      "type": "edit",
      "name": "Grouped note",
      "default": "",
      "visibleWhen": {
        "and": [
          {"field": "enabled", "equals": true},
          {"or": [
            {"field": "level", "equals": 50},
            {"field": "alternative", "equals": true}
          ]}
        ]
      }
    },
    {
      "id": "note",
      "type": "edit",
      "name": "Note",
      "default": "",
      "visibleWhen": {
        "field": "enabled",
        "equals": true
      }
    }
  ]
})JSON";

bool pageRegistered = false;
bool startupApplied = false;

/**
 * @brief Initialize the example hardware, callbacks and portal.
 * @return No value.
 */
void setup() {
  Serial.begin(115200);
  portal.onAppConfigPageRegistrationFinished([](bool success) {
    pageRegistered = success;
    if (!success) Serial.println(portal.appConfigRegistrationError().reason);
  });
  // Queue definitions on every boot; loop() completes registration.
  if (!portal.startAppConfigPageRegistration(FPSTR(CONTROL_PAGE))) {
    Serial.println(portal.appConfigRegistrationError().reason);
  }
  portal.onAppConfigValueChanged([](const String& key,
                                   const ArdJSON::JSONVar& value,
                                   ArdPortal::ChangeSource) {
    // Apply the value to your hardware here without blocking.
    Serial.print(key); Serial.print(" = ");
    Serial.println(ArdJSON::JSON.stringify(value));
  });
  portal.onPortalAndAppConfigReady([](bool loaded) {
    // Storage is ready; page registration may still be pending.
    Serial.println(loaded ? "Configuration loaded" : "Using defaults");
  });
  if (!portal.begin()) Serial.println("Could not start the portal.");
}

/**
 * @brief Advance the component work; call repeatedly from the Arduino main loop.
 * @return No value.
 */
void loop() {
  portal.loop(); // Services registration, live controls, MQTT and storage.
  if (!startupApplied && pageRegistered && portal.portalAndAppConfigReady()) {
    startupApplied = true;
    Serial.println(portal.getAppConfigValue("enabled").asBool()
                   ? "Enabled at startup" : "Disabled at startup");
  }
}
