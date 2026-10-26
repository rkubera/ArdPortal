// SPDX-License-Identifier: MIT

/*
 * UsbDiagnostics
 *
 * Purpose: Forward ArdPortal Messages to USB/UART Serial without serving WWW.
 *
 * Demonstrates: onLog() and LogChannel with web histories, MQTT and OTA compiled out.
 *
 * Requirements: ESP8266/ESP32; Serial Monitor at 115200 baud and an appropriate USB/UART port.
 *
 * What to expect: Read portal diagnostics in Serial. This is not a bridge for arbitrary SDK
 * logs; Wi-Fi remains compiled in.
 */

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
