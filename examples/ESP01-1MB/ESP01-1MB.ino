// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
/*
 * ESP-01 portal with plain MQTT, OTA and Home Assistant enabled.
 * TLS, Console and field dependencies are compiled out.
 * Select Generic ESP8266 Module, the actual flash size, and a filesystem layout:
 * recommended for a 1 MB ESP-01: 1MB (FS:64KB OTA:~470KB).
 * The reserved filesystem area is used by ArdFS, not LittleFS.
 * Configure Wi-Fi and the MQTT broker in the web portal. Settings are restored
 * after reboot. No additional libraries or separate MQTT loop are required.
 * Conditional fields (visibleWhen dependencies) are also compiled out.
 */
#define ARDPORTAL_ENABLE_DEPENDENCIES 0
#define ARDPORTAL_ENABLE_OTA 1
#define ARDPORTAL_ENABLE_MQTT 1
#define ARDPORTAL_ENABLE_MQTT_TLS 0
#define ARDPORTAL_ENABLE_HA 1
#define ARDPORTAL_ENABLE_CONSOLE 0
// Only these four field types are compiled into this profile.
#define ARDPORTAL_ENABLE_CONTROLS 0
#define ARDPORTAL_ENABLE_CONTROL_SLIDER 1
#define ARDPORTAL_ENABLE_CONTROL_TEXT 1
#define ARDPORTAL_ENABLE_CONTROL_SELECT 1
#define ARDPORTAL_ENABLE_CONTROL_EDIT 1
#include <ArdPortal.h>

ArdPortal portal;

void setup() {
  Serial.begin(115200);
  if (!portal.begin()) Serial.println("Could not start the portal.");
}

void loop() {
  portal.loop();
}
