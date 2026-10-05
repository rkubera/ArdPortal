// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
/*
 * Read and change application configuration values through the portal API.
 * Demonstrates configuration readiness, change callbacks and durable save results.
 */

#include <ArdPortal.h>

ArdPortal portal;
uint32_t reportInterval = 5000, lastReport = 0;
bool needsDefaults = false;

void setup() {
  Serial.begin(115200);
  portal.onPortalAndAppConfigReady([](bool) {
    ArdJSON::JSONVar value = portal.getAppConfigValue("reportInterval");
    int64_t interval;
    if (value.toInteger(interval) && interval >= 1000 && interval <= 60000) reportInterval = uint32_t(interval);
    else needsDefaults = true;
  });
  portal.onPortalConfigChanged([](const ArdPortal::Config& config) {
    Serial.print("Portal change: "); Serial.println(config.deviceName);
  });
  portal.onConfigSaved([](bool ok, const String& error) {
    Serial.println(ok ? "Configuration committed (or unchanged)." : error);
  });
  portal.begin();
}

void loop() {
  portal.loop();
  if (needsDefaults && portal.configurationReady() && !portal.storageBusy()) {
    if (portal.setAppConfigValue("reportInterval", reportInterval)) needsDefaults = false;
  }
  if (uint32_t(millis() - lastReport) >= reportInterval) {
    lastReport = millis(); Serial.println("The application is still running.");
  }
  // Setters accept data in RAM. Save completion is reported by onConfigSaved.
  // To update portal settings: copy portal.getPortalConfig(), edit and call setPortalConfig().
}
