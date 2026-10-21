// SPDX-License-Identifier: MIT
// ESP32-C6/H2, Zigbee ED board mode. Keep partitions.csv beside the sketch.
#define ARDPORTAL_ENABLE_ZIGBEE 1
#define ARDPORTAL_ENABLE_WIFI 0
#include <ArdPortal.h>

ArdPortal portal;

void setup() {
  ArdPower::Options options;
  options.policy=ArdPower::Policy::EventDriven;
  options.sleep=ArdPower::SleepType::DeepSleep;
  options.deepSleepMs=60000;
  options.sleepDelayMs=1000;
  portal.power().configure(options);
  // Read sensors/register application fields here. Submit network reports only
  // after the connection is restored; hold a power lock for required ACKs.
  portal.begin(); // Initial pairing requires joining enabled on the coordinator.
}

void loop() {
  portal.loop();
  // After wake setup() runs again; saved Zigbee identity is restored normally.
  // No RAM software timers with DeepSleep. Use deepSleepMs or a deep-wake GPIO.
  // Sleep waits for connection, native readiness, queued reports and storage.
}
