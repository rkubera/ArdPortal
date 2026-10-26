// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT

/*
 * MatterPortal
 *
 * Purpose: Expose a plug, temperature sensor and brightness control over native Matter.
 *
 * Demonstrates: Shared JSON-to-Matter mappings, value callbacks, pairing UI and traffic
 * diagnostics.
 *
 * Requirements: Supported Matter ESP32 SDK; default Arduino target ESP32-C6, 4 MB/Huge APP;
 * keep partitions.csv.
 *
 * What to expect: Commission from the controller app over BLE. Wi-Fi is default; Thread needs a
 * border router. Values are illustrative; add hardware in callbacks.
 */

// ESP32-C6, 4 MB flash, Huge APP board menu. Keep partitions.csv beside this sketch.
// Select Wi-Fi (0) or Thread (1); Thread requires a border router.
#ifndef ARDPORTAL_ENABLE_MATTER
#define ARDPORTAL_ENABLE_MATTER 1
#endif
#ifndef ARDPORTAL_MATTER_THREAD
#define ARDPORTAL_MATTER_THREAD 0
#endif
#define ARDPORTAL_ENABLE_MQTT 0
#define ARDPORTAL_ENABLE_HA 0
#define ARDPORTAL_ENABLE_OTA 0
#ifndef ARDPORTAL_ENABLE_MATTER_CONSOLE
#define ARDPORTAL_ENABLE_MATTER_CONSOLE 1
#endif
#ifndef ARDPORTAL_ENABLE_CONSOLE_MESSAGES
#define ARDPORTAL_ENABLE_CONSOLE_MESSAGES 1
#endif
#ifndef ARDPORTAL_ENABLE_CONTROLS
#define ARDPORTAL_ENABLE_CONTROLS 0
#endif
#define ARDPORTAL_ENABLE_CONTROL_SWITCH 1
#define ARDPORTAL_ENABLE_CONTROL_TEXT 1
#define ARDPORTAL_ENABLE_CONTROL_SLIDER 1
#include <ArdPortal.h>
#include <esp_heap_caps.h>

ArdPortal portal;
void setup() {
  Serial.begin(115200);
#if ARDPORTAL_SDK_DIAGNOSTICS
  ardportalSdkDiagnosticsBegin();
#endif
  portal.onAppConfigPageRegistrationFinished([](bool success) {
    if (!success) Serial.printf("Matter page: %s\n", portal.appConfigRegistrationError().reason.c_str());
  });
#if ARDPORTAL_ENABLE_WIFI
  portal.onWifiStateChanged([](ArdPortal::WifiState state) {
    Serial.printf("Wi-Fi state=%u STA=%s AP=%s\n", unsigned(state), portal.localIP().toString().c_str(), portal.apIP().toString().c_str());
  });
#endif
  if (!portal.startAppConfigPageRegistration(F(R"JSON({
    "id":"matter_demo","name":"Controls","order":0,"fields":[
      {"id":"outlet","type":"switch","class":"outlet","name":"Outlet","default":false},
      {"id":"temperature","type":"text","class":"temperature","name":"Temperature","default":21.5,"persist":false},
      {"id":"brightness","type":"slider","class":"brightness","name":"Brightness","min":0,"max":100,"step":1,"default":50}
    ]
  })JSON"))) Serial.printf("Matter page: %s\n", portal.appConfigRegistrationError().reason.c_str());
  portal.onAppConfigValueChanged([](const String& id, const ArdJSON::JSONVar& value, ArdPortal::ChangeSource source) {
    Serial.printf("%s source=%u value=%s\n", id.c_str(), unsigned(source), ArdJSON::JSON.stringify(value).c_str());
    // Drive hardware here; report measured values with setAppConfigStateValue().
  });
#if ARDPORTAL_ENABLE_POWER_MANAGEMENT && defined(CONFIG_PM_ENABLE) && CONFIG_PM_ENABLE
  ArdPower::Options power;
  power.policy = ArdPower::Policy::EventDriven;
  power.sleep = ArdPower::SleepType::ConnectedSleep;
  if (!portal.power().configure(power)) Serial.println("Matter power configuration rejected");
#endif
  portal.begin();
}
void loop() {
  portal.loop();
#if ARDPORTAL_SDK_DIAGNOSTICS
  ardportalSdkDiagnosticsDrain();
#endif
  static uint32_t lastStatus = 0, lastMemory = 0;
#if ARDPORTAL_ENABLE_MATTER
  static ArdMatter::State previous = ArdMatter::State::Waiting;
#endif
  if (uint32_t(millis() - lastStatus) < 3000) return;
  lastStatus = millis();
#if ARDPORTAL_ENABLE_MATTER
  if (previous != portal.matter().state()) {
    previous = portal.matter().state();
    Serial.printf("Matter state=%u endpoints=%u unsupported=%u %s\n", unsigned(previous), portal.matter().endpointCount(), portal.matter().unsupportedCount(), portal.matter().error());
  }
#endif
  if (!lastMemory || uint32_t(millis() - lastMemory) >= 30000) {
    lastMemory = millis();
    Serial.printf("Heap=%u DMA=%u DMA-largest=%u\n", unsigned(ESP.getFreeHeap()), unsigned(heap_caps_get_free_size(MALLOC_CAP_DMA)), unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_DMA)));
  }
#if ARDPORTAL_ENABLE_MATTER
  String code = portal.matter().manualPairingCode();
  if (code.length()) Serial.printf("Matter pairing: %s\n", code.c_str());
#endif
}
