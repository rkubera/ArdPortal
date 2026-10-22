// SPDX-License-Identifier: MIT
#include <ArdPortal.h>

ArdPortal portal;

void performCycleWork() {
  Serial.println(F("Periodic sensor reading"));
  // Read sensors / queue application work here. Keep a power lock until any
  // required asynchronous work or network acknowledgement has completed.
}

void onPowerEvent(const ArdPower::Event& event, void*) {
  if (event.kind == ArdPower::EventKind::Periodic) performCycleWork();
}

void setup() {
  Serial.begin(115200);
  ArdPower::Options options;
  options.policy = ArdPower::Policy::Periodic;
  options.sleep = ArdPower::SleepType::ConnectedSleep;
  options.periodic.intervalMs = 60000;
  options.sleepDelayMs = 1000;
  portal.power().configure(options);
  portal.power().onWake(onPowerEvent);
  performCycleWork();  // First cycle now; later callbacks follow every minute.
  portal.begin();
}

void loop() {
  portal.loop();
  // Provisioning AP, storage, OTA and other normal blockers can postpone sleep.
  // DeepSleep instead runs setup() again after wake and starts a new schedule.
  // LightSleep on ESP32 requires Wi-Fi off; ESP8266 uses ConnectedSleep here.
}
