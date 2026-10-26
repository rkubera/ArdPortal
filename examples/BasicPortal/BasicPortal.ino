// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT

/*
 * BasicPortal
 *
 * Purpose: Start a general-purpose device configuration portal with default modules.
 *
 * Demonstrates: The minimal begin()/loop() lifecycle; automatic settings storage and network
 * servicing.
 *
 * Requirements: Wi-Fi-capable ESP8266/ESP32 and a flash layout with ArdFS and OTA space.
 *
 * What to expect: Connect to the setup AP, configure Wi-Fi/MQTT and use the built-in pages. No
 * custom controls are added.
 */

/*
 * Basic portal with Wi-Fi, MQTT, Home Assistant discovery and OTA enabled.
 * ArdFS restores and saves settings automatically; loop() services all modules.
 */

#include <ArdPortal.h>

ArdPortal portal;

/**
 * @brief Initialize the example hardware, callbacks and portal.
 * @return No value.
 */
void setup() {
  Serial.begin(115200);
  // Defaults: ArdUI-<chip ID>, AP password 1234567890, timeout 30 seconds.
  if (!portal.begin()) Serial.println("Could not start the portal.");
}

/**
 * @brief Advance the component work; call repeatedly from the Arduino main loop.
 * @return No value.
 */
void loop() {
  portal.loop();  // Also performs background storage and captive DNS work.
}
