// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
/*
 * Basic portal with Wi-Fi, MQTT, Home Assistant discovery and OTA enabled.
 * ArdFS restores and saves settings automatically; loop() services all modules.
 */

#include <ArdPortal.h>

ArdPortal portal;

void setup() {
  Serial.begin(115200);
  // Defaults: ArdUI-<chip ID>, AP password 1234567890, timeout 30 seconds.
  if (!portal.begin()) Serial.println("Could not start the portal.");
}

void loop() {
  portal.loop(); // Also performs background storage and captive DNS work.
}
