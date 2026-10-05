// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
/*
 * Register two dynamic pages from JSON definitions stored in flash.
 * Controls contains a switch and slider; Settings contains text and a select.
 * Values are saved, restored and reported through an application change callback.
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
      "default": 50
    }
  ]
})JSON";

static const char SETTINGS_PAGE[] PROGMEM = R"JSON({
  "id": "settings",
  "name": "Settings",
  "order": 20,
  "fields": [
    {
      "id": "label",
      "type": "edit",
      "name": "Label",
      "default": "My device"
    },
    {
      "id": "profile",
      "type": "select",
      "name": "Profile",
      "default": "normal",
      "options": [
        {"value": "eco", "name": "Eco"},
        {"value": "normal", "name": "Normal"}
      ]
    }
  ]
})JSON";

void setup() {
  Serial.begin(115200);
  // Register definitions on every boot, before starting the portal.
  if (!portal.addPortalPage(FPSTR(CONTROL_PAGE))) {
    Serial.println("Invalid dynamic page definition.");
  }
  if (!portal.addPortalPage(FPSTR(SETTINGS_PAGE))) {
    Serial.println("Invalid settings page definition.");
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
