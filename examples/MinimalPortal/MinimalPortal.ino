// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
/*
 * Minimal portal with OTA enabled; MQTT, Console and Home Assistant compiled out.
 * Keeps Wi-Fi/AP settings, device settings, captive DNS, NTP and ArdFS storage.
 * Feature defines below compile optional modules out of this sketch.
 * Disabling MQTT automatically disables HA and Console.
 * No separate MQTT, HA or OTA loop is required.
 * Dynamic forms and all control types are compiled out.
 * Conditional fields (visibleWhen dependencies) are also compiled out.
 */
#define ARDPORTAL_ENABLE_OTA 1
#define ARDPORTAL_ENABLE_MQTT 0
#define ARDPORTAL_ENABLE_DYNAMIC_PAGES 0
#include <ArdPortal.h>

ArdPortal portal;

/**
 * @brief Initialize the example hardware, callbacks and portal.
 * @return No value.
 */
void setup() {
  Serial.begin(115200);
  if (!portal.begin()) Serial.println("Could not start the portal.");
}

/**
 * @brief Advance the component work; call repeatedly from the Arduino main loop.
 * @return No value.
 */
void loop() {
  portal.loop();
}
