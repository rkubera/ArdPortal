// SPDX-License-Identifier: MIT
#define ARDPORTAL_ENABLE_WIFI 0
#include <ArdPortal.h>

ArdPortal portal;

void setup() {
  Serial.begin(115200);
  const auto cause=portal.power().wakeCause(); // Valid before configure()/begin().
  if(cause==ArdPower::WakeCause::Timer)Serial.println(F("Deep-sleep timer wake"));
  else if(cause==ArdPower::WakeCause::Gpio)Serial.println(F("Deep-sleep GPIO wake"));
  else if(portal.power().wokeFromDeepSleep())Serial.println(F("Deep-sleep wake; source unavailable"));
  // Timer wake reboots the application: perform the battery-node work here.
  // Register application fields before begin(), and queue changes once ready.
  ArdPower::Options options;
  options.policy=ArdPower::Policy::EventDriven;
  options.sleep=ArdPower::SleepType::DeepSleep;
  options.deepSleepMs=60000;
  options.sleepDelayMs=1000;
  portal.power().configure(options);
  portal.begin();
  // ESP32 can additionally register one suitable GPIO with addWakePin().
  // The default scope follows SleepType::DeepSleep.
  // ESP8266 timer wake requires GPIO16 connected to RST.
}

void loop() {
  portal.loop(); // Automatically enters deep sleep after startup/saves finish.
  // Do not register RAM software timers with DeepSleep; use deepSleepMs/wakeIn().
}
