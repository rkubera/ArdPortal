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

/**
 * @brief Initialize the example hardware, callbacks and portal.
 * @return No value.
 */
void setup() {
  Serial.begin(115200);
  portal.onPortalAndAppConfigReady([](bool) {
    ArdJSON::JSONVar value = portal.getAppConfigValue("reportInterval");
    int64_t interval;
    if (value.toInteger(interval) && interval >= 1000 && interval <= 60000) reportInterval = uint32_t(interval);
    else needsDefaults = true;
  });
  portal.onPortalConfigChanged([](const ArdPortal::PortalConfig& config, ArdPortal::ChangeSource source) {
    if (source != ArdPortal::ChangeSource::Portal) return;
    Serial.print("Portal change: "); Serial.println(config.deviceName);
  });
  portal.onPortalAndAppConfigSaved([](bool ok, const String& error) {
    Serial.println(ok ? "Configuration committed (or unchanged)." : error);
  });
  portal.begin();
}

/**
 * @brief Advance the component work; call repeatedly from the Arduino main loop.
 * @return No value.
 */
void loop() {
  portal.loop();
  if (needsDefaults && portal.portalAndAppConfigReady() && !portal.portalAndAppConfigBusy()) {
    if (portal.setAppConfigValue("reportInterval", reportInterval)) needsDefaults = false;
  }
  if (uint32_t(millis() - lastReport) >= reportInterval) {
    lastReport = millis(); Serial.println("The application is still running.");
  }
  // Setters accept data in RAM. Save completion is reported by onPortalAndAppConfigSaved.
  // To update portal settings: copy portal.getPortalConfig(), edit and call setPortalConfig().
}
