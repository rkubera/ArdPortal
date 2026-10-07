// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
/*
 * Register two dynamic pages cooperatively, without building a whole-page DOM.
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

unsigned registeredPages = 0;
bool startupApplied = false;

void setup() {
  Serial.begin(115200);
  portal.onAppConfigPageRegistrationFinished([](bool success) {
    if (!success) {
      const auto& error = portal.appConfigRegistrationError();
      Serial.printf("Registration: %s field=%s reason=%s\n",
                    error.stage, error.fieldId.c_str(), error.reason.c_str());
      return;
    }
    ++registeredPages;
    if (registeredPages == 1 && !portal.startAppConfigPageRegistration(FPSTR(SETTINGS_PAGE)))
      Serial.println(portal.appConfigRegistrationError().reason);
  });
  portal.onAppConfigValueChanged([](const String& key,
                                   const ArdJSON::JSONVar& value,
                                   ArdPortal::ChangeSource) {
    // Apply user changes to hardware without blocking.
    Serial.print(key); Serial.print(" = ");
    Serial.println(ArdJSON::JSON.stringify(value));
  });
  if (!portal.startAppConfigPageRegistration(FPSTR(CONTROL_PAGE)))
    Serial.println(portal.appConfigRegistrationError().reason);
  if (!portal.begin()) Serial.println("Could not start the portal.");
}

void loop() {
  portal.loop(); // One registration unit; UART/application work can run next.
  if (!startupApplied && registeredPages == 2 && portal.portalAndAppConfigReady()) {
    startupApplied = true;
    // Config loading may finish before page registration. Wait for both.
    Serial.println(portal.getAppConfigValue("enabled").asBool()
                   ? "Enabled at startup" : "Disabled at startup");
  }
}
