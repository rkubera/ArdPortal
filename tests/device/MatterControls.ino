// SPDX-License-Identifier: MIT
// Hardware validation fixture; deliberately excluded from normal example builds.
#define ARDPORTAL_ENABLE_MQTT 0
#define ARDPORTAL_ENABLE_HA 0
#define ARDPORTAL_ENABLE_OTA 0
#define ARDPORTAL_ENABLE_CONTROLS 0
#define ARDPORTAL_ENABLE_CONTROL_SWITCH 1
#define ARDPORTAL_ENABLE_CONTROL_SLIDER 1
#define ARDPORTAL_ENABLE_CONTROL_TEXT 1
#define ARDPORTAL_ENABLE_CONTROL_BINARY_SENSOR 1
#define ARDPORTAL_ENABLE_CONTROL_LIGHT 1
#define ARDPORTAL_ENABLE_CONTROL_FAN 1
#define ARDPORTAL_ENABLE_CONTROL_CLIMATE 1
#define ARDPORTAL_ENABLE_CONTROL_COVER 1
#define ARDPORTAL_ENABLE_MATTER_CONSOLE 1
#define ARDPORTAL_ENABLE_CONSOLE_MESSAGES 1
#include <ArdPortal.h>
#include <nvs.h>

ArdPortal portal;
int pendingSnapshot = -1;
uint32_t snapshotAt = 0;
void applySnapshot(int selected);
void setup() {
  Serial.begin(115200);
  // Test fixture only: a reset old image can recreate its old layout hash before
  // upload. The caller must reset Matter before installing this new inventory.
  // Clear only that hash once, preserving SDK fabrics and all portal settings.
  nvs_handle_t layout;
  if (nvs_open("ard-matter", NVS_READWRITE, &layout) == ESP_OK) {
    uint32_t fixtureVersion = 0;
    if (nvs_get_u32(layout, "control_test", &fixtureVersion) == ESP_ERR_NVS_NOT_FOUND || fixtureVersion != 0x1601) {
      esp_err_t erased = nvs_erase_key(layout, "layout");
      if (erased == ESP_OK || erased == ESP_ERR_NVS_NOT_FOUND) {
        if (nvs_set_u32(layout, "control_test", 0x1601) == ESP_OK) nvs_commit(layout);
      }
    }
    nvs_close(layout);
  }
  portal.onAppConfigPageRegistrationFinished([](bool ok) {
    if (!ok) Serial.println(portal.appConfigRegistrationError().reason);
  });
  portal.startAppConfigPageRegistration(F(R"JSON({"id":"matter_test","name":"Matter validation","fields":[
    {"id":"outlet","type":"switch","class":"outlet","name":"T01 Outlet","persist":false,"default":false},
    {"id":"onoff","type":"switch","class":"light","name":"T02 Light","persist":false,"default":false},
    {"id":"dimmer","type":"slider","class":"brightness","name":"T03 Dimmer","min":0,"max":100,"step":1,"persist":false,"default":25},
    {"id":"rgb","type":"light","name":"T04 RGB","persist":false},
    {"id":"temp","type":"text","class":"temperature","name":"T05 Temperature","persist":false,"default":21.5},
    {"id":"humidity","type":"text","class":"humidity","name":"T06 Humidity","persist":false,"default":45.5},
    {"id":"pressure","type":"text","class":"pressure","name":"T07 Pressure","persist":false,"default":1013},
    {"id":"lux","type":"text","class":"illuminance","name":"T08 Illuminance","persist":false,"default":100},
    {"id":"contact","type":"binary_sensor","class":"door","name":"T09 Contact","persist":false,"default":false},
    {"id":"occupancy","type":"binary_sensor","class":"occupancy","name":"T10 Occupancy","persist":false,"default":false},
    {"id":"leak","type":"binary_sensor","class":"moisture","name":"T11 Leak","persist":false,"default":false},
    {"id":"fan","type":"fan","name":"T12 Fan","persist":false},
    {"id":"heat","type":"climate","name":"T13 Heating","min":7,"max":30,"step":0.5,"modes":["off","heat"],"persist":false},
    {"id":"cool","type":"climate","name":"T14 Cooling","min":16,"max":32,"step":0.5,"modes":["off","cool"],"persist":false},
    {"id":"dual","type":"climate","name":"T15 Heat cool","min":16,"max":30,"step":0.5,"modes":["off","heat","cool","heat_cool"],"persist":false},
    {"id":"cover","type":"cover","class":"blind","name":"T16 Cover","persist":false},
    {"id":"last_command","type":"text","name":"Last simulated drive action","readonly":true,"persist":false,"default":""},
    {"id":"sample","type":"slider","name":"Test snapshot 0 or 1","min":0,"max":1,"step":1,"persist":false,"default":0}
  ]})JSON"));
  portal.onAppConfigValueChanged([](const String& id, const ArdJSON::JSONVar& value, ArdPortal::ChangeSource source) {
    Serial.printf("%s source=%u value=%s\n", id.c_str(), unsigned(source), ArdJSON::JSON.stringify(value).c_str());
    if (id != "sample") return;
    pendingSnapshot = int(value.asDouble());
    snapshotAt = millis();
  });
  portal.onAppConfigCommand([](const String& id, const String&, const ArdJSON::JSONVar& value, ArdPortal::ChangeSource) {
    if (id != "cover") return false;
    String action = value.asString();
    if (action != "OPEN" && action != "CLOSE" && action != "STOP") return false;
    auto state = portal.getAppConfigValue("cover");
    if (action == "OPEN") {
      state["position"] = 100;
      state["state"] = "open";
    }
    if (action == "CLOSE") {
      state["position"] = 0;
      state["state"] = "closed";
    }
    if (action == "STOP") state["state"] = state["position"].asDouble() > 0 ? "open" : "closed";
    return portal.setAppConfigStateValue("last_command", value, false) && portal.setAppConfigStateValue("cover", state, false);
  });
  portal.begin();
}
void applySnapshot(int selected) {
  // Real application state API, including read-only sensors. Stable snapshots
  // let HA reports be compared without autonomous timers fighting commands.
  auto snapshot = ArdJSON::JSON.parse(selected > 0 ? R"JSON({
      "outlet":true,"onoff":true,"dimmer":75,
      "rgb":{"state":"ON","brightness":180,"color":{"r":0,"g":0,"b":255},"color_mode":"rgb"},
      "temp":27.25,"humidity":67.5,"pressure":1001,"lux":1000,
      "contact":true,"occupancy":true,"leak":true,
      "fan":{"state":"ON","percentage":66,"oscillation":"oscillate_off","direction":"forward"},
      "heat":{"mode":"heat","temperature":23,"current_temperature":20,"action":"heating"},
      "cool":{"mode":"cool","temperature":25,"current_temperature":28,"action":"cooling"},
      "dual":{"mode":"heat_cool","temperature":22,"temperature_low":20,"temperature_high":26,"current_temperature":23,"action":"idle"},
      "cover":{"state":"open","position":75,"tilt":25}
    })JSON"
                                                   : R"JSON({
      "outlet":false,"onoff":false,"dimmer":0,
      "rgb":{"state":"OFF","brightness":64,"color":{"r":255,"g":0,"b":0},"color_mode":"rgb"},
      "temp":-5.5,"humidity":25,"pressure":1025,"lux":0,
      "contact":false,"occupancy":false,"leak":false,
      "fan":{"state":"OFF","percentage":0,"oscillation":"oscillate_off","direction":"forward"},
      "heat":{"mode":"off","temperature":19,"current_temperature":18,"action":"off"},
      "cool":{"mode":"off","temperature":24,"current_temperature":21,"action":"off"},
      "dual":{"mode":"off","temperature":22,"temperature_low":19,"temperature_high":25,"current_temperature":21,"action":"off"},
      "cover":{"state":"closed","position":0,"tilt":0}
    })JSON");
  auto keys = snapshot.keys();
  for (size_t i = 0; i < keys.length(); ++i) {
    String key = keys[i].asString();
    if (!portal.setAppConfigStateValue(key.c_str(), snapshot[key], false)) Serial.printf("Snapshot rejected: %s\n", key.c_str());
  }
}
void loop() {
  portal.loop();
  // Leave the HTTP command callback before allocating the full test snapshot.
  if (pendingSnapshot >= 0 && uint32_t(millis() - snapshotAt) >= 100) {
    int selected = pendingSnapshot;
    pendingSnapshot = -1;
    applySnapshot(selected);
  }
}
