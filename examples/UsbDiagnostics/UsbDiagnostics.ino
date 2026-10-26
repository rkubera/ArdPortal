// SPDX-License-Identifier: MIT
#define ARDPORTAL_ENABLE_WWW 0
#define ARDPORTAL_ENABLE_LOG_CALLBACK 1
#define ARDPORTAL_ENABLE_MQTT 0
#define ARDPORTAL_ENABLE_HA 0
#define ARDPORTAL_ENABLE_OTA 0
#include <ArdPortal.h>

ArdPortal portal;
void setup() {
  Serial.begin(115200);
  portal.onLog([](ArdPortal::LogChannel channel, const String& message) {
    // USB/UART is owned by the sketch. Do not wait for a host or a full buffer.
    const size_t bytes = message.length();
    if (Serial.availableForWrite() < int(bytes + 5)) return;
    Serial.print(unsigned(channel));
    Serial.print(": ");
    Serial.println(message);
  });
  portal.begin();
}
void loop() {
  portal.loop();
}
