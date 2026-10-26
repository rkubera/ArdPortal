// SPDX-License-Identifier: MIT

/*
 * PowerManagementDeepSleep
 *
 * Purpose: Run a node that restarts after a timed deep-sleep interval with Wi-Fi disabled.
 *
 * Demonstrates: DeepSleep configuration, wake-cause inspection and cooperative startup/storage
 * admission; optional native Zigbee end-device readiness and saved-pairing recovery.
 *
 * Requirements: ESP8266/ESP32; ESP8266 timed wake requires GPIO16 connected to RST.
 * Optional Zigbee: ESP32-C6/H2 with at least 4 MiB flash, Zigbee ED board mode and
 * zigbee-partitions.csv copied to partitions.csv beside the sketch before building.
 * Enable joining on the coordinator for first pairing; use an end device, not a router.
 *
 * What to expect: Serial shows the wake cause. Sleep is requested for 60 seconds after work
 * settles; setup() runs again on wake. Add actual sensor work. With Zigbee enabled,
 * sleep also waits for connection, native readiness, pending reports and storage.
 */

#ifndef ARDPORTAL_ENABLE_ZIGBEE
#define ARDPORTAL_ENABLE_ZIGBEE 0  // Set to 1 for the Zigbee end-device variant.
#endif
#define ARDPORTAL_ENABLE_WIFI 0
#include <ArdPortal.h>

ArdPortal portal;

void setup() {
  Serial.begin(115200);
  const auto cause = portal.power().wakeCause();  // Valid before configure()/begin().
  if (cause == ArdPower::WakeCause::Timer) Serial.println(F("Deep-sleep timer wake"));
  else if (cause == ArdPower::WakeCause::Gpio) Serial.println(F("Deep-sleep GPIO wake"));
  else if (portal.power().wokeFromDeepSleep()) Serial.println(F("Deep-sleep wake; source unavailable"));
  // Timer wake reboots the application: perform the battery-node work here.
  // Register application fields before begin(), and queue changes once ready.
  // Zigbee: submit reports after connection is restored and hold a power lock
  // until required acknowledgements/application work have completed.
  ArdPower::Options options;
  options.policy = ArdPower::Policy::EventDriven;
  options.sleep = ArdPower::SleepType::DeepSleep;
  options.deepSleepMs = 60000;
  options.sleepDelayMs = 1000;
  portal.power().configure(options);
  portal.begin();  // Zigbee restores saved pairing; initial joining needs a coordinator.
  // ESP32 can additionally register one suitable GPIO with addWakePin().
  // The default scope follows SleepType::DeepSleep.
  // ESP8266 timer wake requires GPIO16 connected to RST.
}

void loop() {
  portal.loop();  // Automatically enters deep sleep after startup/saves finish.
  // Do not register RAM software timers with DeepSleep; use deepSleepMs/wakeIn().
}
