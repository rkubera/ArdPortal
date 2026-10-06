// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
/*
 * Show and hide dynamic fields using visibleWhen dependencies on another field.
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

void setup() {
  Serial.begin(115200);
  // Register definitions on every boot, before starting the portal.
  if (!portal.addAppConfigPage(FPSTR(CONTROL_PAGE))) {
    Serial.println("Invalid dynamic page definition.");
  }
  portal.onAppConfigValueChanged([](const String& key,
                                   const ArdJSON::JSONVar& value,
                                   ArdPortal::ChangeSource) {
    // Apply the value to your hardware here without blocking.
    Serial.print(key); Serial.print(" = ");
    Serial.println(ArdJSON::JSON.stringify(value));
  });
  portal.onPortalAndAppConfigReady([](bool loaded) {
    // Read restored values here; change callbacks do not run for startup loading.
    Serial.println(loaded ? "Configuration loaded" : "Using defaults");
    bool enabled = portal.getAppConfigValue("enabled").asBool();
    Serial.println(enabled ? "Enabled at startup" : "Disabled at startup");
  });
  if (!portal.begin()) Serial.println("Could not start the portal.");
}

void loop() {
  portal.loop(); // Services live controls, MQTT and journaled configuration.
}
