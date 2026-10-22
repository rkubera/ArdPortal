// SPDX-License-Identifier: MIT
// ESP32-C6/H2, Zigbee ED board mode. Keep partitions.csv beside the sketch.
#define ARDPORTAL_ENABLE_ZIGBEE 1
#define ARDPORTAL_ENABLE_WIFI 0
#include <ArdPortal.h>

ArdPortal portal;

void onPowerEvent(const ArdPower::Event& event, void*) {
  if (event.kind == ArdPower::EventKind::Timer) {
    // Read sensors and queue application reports here, from the Arduino task.
  }
}

void setup() {
  ArdPower::Options options;
  options.policy = ArdPower::Policy::EventDriven;
  options.sleep = ArdPower::SleepType::LightSleep;  // Native Zigbee sleep with RAM retained.
  options.sleepDelayMs = 0;
  portal.power().configure(options);
  portal.power().onWake(onPowerEvent);
  portal.power().setTimer(1, 10000, true);
  portal.begin();  // Enable joining on the coordinator for the initial pairing.
}

void loop() {
  portal.loop();
  // Saved pairing is preserved. Parent polling uses the existing 3-second interval.
  // CPU light sleep requires PM + tickless idle support in the compiled ESP32 core.
  // Router blocks CPU sleep. LightSleep requires Wi-Fi off; DeepSleep reboots.
  // ConnectedSleep additionally offers connectedSleep.allowCpuSleep=false.
}
