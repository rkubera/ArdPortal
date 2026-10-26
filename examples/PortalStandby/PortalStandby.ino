// SPDX-License-Identifier: MIT
// ESP32-C6, Zigbee ED board mode. Connect a button from GPIO4 to GND.
#define ARDPORTAL_ENABLE_ZIGBEE 1
#include <ArdPortal.h>

ArdPortal portal;
constexpr uint8_t buttonPin = 4;  // Adapt to your board.
constexpr uint32_t portalWindowMs = 120000;
bool paired = false;
bool previousButton = false;
uint32_t buttonChangedAt = 0;
uint32_t portalOpenedAt = 0;

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  portal.begin();
}

void loop() {
  portal.loop();  // Keep Zigbee, storage and power management running.
  const uint32_t now = millis();
  const bool pressed = digitalRead(buttonPin) == LOW;
  if (pressed != previousButton) {
    previousButton = pressed;
    buttonChangedAt = now;
  }
  static bool handled = false;
  if (!pressed) handled = false;
  if (pressed && !handled && uint32_t(now - buttonChangedAt) >= 40) {
    // Retry if configuration/OTA temporarily prevents resuming.
    if (portal.resumePortal()) {
      portalOpenedAt = now;
      handled = true;
    }
  }
  if (!paired && portal.zigbee().state() == ArdZigbee::State::Connected) {
    paired = true;
    // Close after pairing; a button can reopen the portal for two minutes.
    portalOpenedAt = now - portalWindowMs;
  }
  if (paired && !pressed && !portal.portalSuspended() && uint32_t(now - portalOpenedAt) >= portalWindowMs) {
    portal.suspendPortal();  // Retry on the next loop if storage/OTA is busy.
  }
}
