// Select only LittleFS for PortalConfig, AppConfig and storage maintenance.
// The flag belongs before ArdPortal.h; no global compiler flags are required.
#define ARDPORTAL_USE_LITTLEFS 1
#include <ArdPortal.h>

ArdPortal portal;

void setup() {
  Serial.begin(115200);
  portal.begin();
}

void loop() {
  portal.loop();
}
