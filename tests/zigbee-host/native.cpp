#include "ZigbeeFields.h"
#define private public
#define ARDPORTAL_ENABLE_DYNAMIC_PAGES 1
#define ARDPORTAL_ZIGBEE_ROUTER 0
#include "ArdZigbeeImpl.h"
#undef private
#include <cassert>
uint32_t fakeMillis = 0;
time_t fakeUnixTime = 0;
EspStub ESP;
SerialStub Serial;
using namespace ArdZigbeeFields;
/** @brief Exercise actual SDK adapter with controlled allocation, callbacks and transport failures. @return Zero on success. */
int main() {
  assert(uint8_t(ArdZigbeeIdentity::Model[0]) == 12);
#if defined(CONFIG_IDF_TARGET_ESP32C5)
  assert(strcmp(ArdZigbeeIdentity::Model + 1, "ArdPortal-C5") == 0);
#else
  assert(strcmp(ArdZigbeeIdentity::Model + 1, "ArdPortal-C6") == 0);
#endif

  ArdZigbee z;
  z.setFirmwareVersion("\x05"
                       "2.3.4");
#if ARDPORTAL_ZIGBEE_DEBUG_SERIAL
  z.log("Serial diagnostic");
  assert(Serial.output.find("Serial diagnostic") != std::string::npos);
  auto serialBefore = Serial.output;
  Serial.capacity = 0;
  z.log("Console only");
  assert(Serial.output == serialBefore);
  assert(z.nextLine(1));
  Serial.capacity = 256;
#endif
  Spec spec;
  spec.kind = Kind::Switch;
  spec.field = 7;
  V f = V::object();
  auto initial = encode(spec, f, false);
  assert(z.addControl(spec, initial, "Relay"));
  assert(z.controls()->spec.endpoint == 2);
  V field = ArdJSON::JSON.parse(R"({"options":[{"value":"a"},{"value":"b"}]})");
  V state = ArdJSON::JSON.parse(R"({"state":"ON","brightness":255,"color":{"r":255,"g":0,"b":0},"temperature":22,"current_temperature":20,"mode":"heat","fan_mode":"auto","position":20,"tilt":70})");
  for (auto kind : { Kind::Action, Kind::State, Kind::Slider, Kind::Select, Kind::Number, Kind::Temperature, Kind::Humidity, Kind::Pressure, Kind::Illuminance, Kind::Flow, Kind::WindSpeed, Kind::CarbonDioxide, Kind::PM25, Kind::Voltage, Kind::Current, Kind::Power, Kind::Binary, Kind::Light, Kind::Climate, Kind::Cover, Kind::Lock, Kind::Siren }) {
    Spec mapping;
    mapping.kind = kind;
    mapping.min = 10;
    mapping.max = 30;
    mapping.step = 0.5;
    mapping.states = 2;
    mapping.fan = true;
    if (kind == Kind::Climate) mapping.sequence = 4;
    V value = kind == Kind::Select ? V("a") : kind == Kind::Lock                                                                         ? V("LOCKED")
                                            : kind == Kind::Binary                                                                       ? V(true)
                                            : kind == Kind::Light || kind == Kind::Climate || kind == Kind::Cover || kind == Kind::Siren ? state
                                                                                                                                         : V(22);
    auto attributes = encode(mapping, field, value);
    assert(z.addControl(mapping, attributes, kind == Kind::Number ? String() : String("Native")));
  }
  assert(z.controlCount() == 23);
  nativeFailure = true;
  assert(!z.createControls(&nativeEndpoints));
  nativeFailure = false;
  assert(z.createControls(&nativeEndpoints));
  assert(!z.controls()->initial && !z.controls()->label);
  for (const auto& endpoint : nativeEndpoints.endpoints) {
    auto* basic = endpoint.second->clusters[0];
    if (basic) {
      const auto& version = basic->attributes[0x4000].bytes;
      assert(version[0] == 5 && memcmp(version.data() + 1, "2.3.4", 5) == 0);
    }
  }
  float maximum = 0, minimum = 0;
  auto* slider = nativeEndpoints.endpoints[5]->clusters[13];
  memcpy(&maximum, slider->attributes[0x41].bytes.data(), 4);
  memcpy(&minimum, slider->attributes[0x45].bytes.data(), 4);
  assert(maximum == 30 && minimum == 10);
  assert(!z._startupError[0]);
  assert(z.begin(ArdZigbee::Options()));
  assert(!z.addControl(spec, initial, "Late"));
  z._ready = true;
  z._state = ArdZigbee::State::Connected;
  auto values = encode(spec, f, true);
  assert(z.publish(2, values));
  assert(reports == 1);
  assert(z.publish(2, values) && reports == 1);
  assert(z.publish(2, values, true) && reports == 2);
  reportFailure = true;
  assert(!z.publish(2, values, true));
  reportFailure = false;
  assert(z.publish(2, values, true));

  auto* light = z._controls.get();
  while (light && light->spec.kind != Kind::Light) light = light->next.get();
  assert(light);
  auto lightValues = encode(light->spec, field, state);
  auto* mode = esp_zb_zcl_get_attribute(light->spec.endpoint, 0x300, 0, 8);
  mode->access = ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY;
  unsigned before = reports;
  failedReportCluster = 0x300;
  failedReportAttribute = 1;
  for (unsigned i = 1; i <= 3; ++i) { assert(!z.publish(light->spec.endpoint, lightValues, true) && reports == before + i && light->reportIncomplete); }
  assert(!z.publish(light->spec.endpoint, lightValues, true) && reports == before + 4 && !light->reportIncomplete);
  assert(!z.publish(light->spec.endpoint, lightValues, true) && reports == before + 5);
  failedReportCluster = failedReportAttribute = -1;
  for (unsigned i = 6; i < 9; ++i) assert(!z.publish(light->spec.endpoint, lightValues, true) && reports == before + i && light->reportIncomplete);
  assert(z.publish(light->spec.endpoint, lightValues, true) && reports == before + 9);
  assert(z.publish(light->spec.endpoint, lightValues) && reports == before + 9);
  assert(!light->pendingReports);
  lockAvailable = false;
  assert(!z.publish(2, values, true));
  lockAvailable = true;

  z.queueReports(size_t(-1), true);
  assert(z.nextReport(100, 0)->spec.endpoint == 2);
  z.finishReport(2, false, 100);
  assert(z.nextReport(101, 0)->spec.endpoint == 3);
  z.finishReport(3, true, 101);
  assert(z.nextReport(102, 2)->spec.endpoint == 4);
  assert(z.nextReport(5099, 0)->spec.endpoint != 2);
  assert(z.nextReport(5100, 0)->spec.endpoint == 2);
  z.queueReports(7, true);
  assert(z.nextReport(103, 0)->spec.endpoint == 2);
  uint8_t data = 1;
  esp_zb_zcl_set_attr_value_message_t msg{};
  msg.info.status = 0;
  msg.info.dst_endpoint = 2;
  msg.info.cluster = 6;
  msg.attribute.id = 0;
  msg.attribute.data = { 0x10, 1, &data };
  assert(ArdZigbee::action(ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID, &msg) == ESP_OK);
  Command c;
  assert(z.nextCommand(c) && c.endpoint == 2 && c.value == 1 && c.cluster == 6);
  char payload[32];
  float decimal = 20.5f;
  ardZigbeePayload(payload, sizeof(payload), &decimal, 0x39, 4);
  assert(std::string(payload) == "20.5");
  int16_t negative = -125;
  ardZigbeePayload(payload, sizeof(payload), &negative, 0x29, 2);
  assert(std::string(payload) == "-125");
  ardZigbeePayload(payload, sizeof(payload), &data, 0x10, 1);
  assert(std::string(payload) == "true");
  for (unsigned i = 0; i < 8; ++i) z.loop(fakeMillis);
  bool sawPayload = false;
  for (uint32_t cursor = 0; const auto* line = z.nextLine(cursor);) {
    cursor = line->id;
    if (line->text.indexOf("RX applied ep=2 payload={\"cluster\":6,\"attr\":0,\"type\":16,\"value\":true}") >= 0) sawPayload = true;
  }
  assert(sawPayload);
  unsigned lifecycleLogs = 0;
  z.onLifecycleDiagnostic(&lifecycleLogs, [](void* owner, const String&) {
    ++*static_cast<unsigned*>(owner);
  });
  z.log("Zigbee link lost");
  assert(lifecycleLogs == 1);
  z.traffic("RX payload={\"cmd\":\"on\"}");
  for (unsigned i = 0; i < 8; ++i) z.loop(fakeMillis);
  assert(lifecycleLogs == 1);
  z.onLifecycleDiagnostic(nullptr, nullptr);

  uint16_t enhancedHue = uint16_t(std::lround(210.0 * 65535 / 360));
  esp_zb_zcl_set_attr_value_message_t colorMsg{};
  colorMsg.info.status = 0;
  colorMsg.info.dst_endpoint = light->spec.endpoint;
  colorMsg.info.cluster = 0x300;
  colorMsg.attribute.id = 0x4000;
  colorMsg.attribute.data = { 0x21, 2, &enhancedHue };
  assert(ArdZigbee::action(ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID, &colorMsg) == ESP_OK);
  Command colorCommand;
  assert(z.nextCommand(colorCommand) && colorCommand.cluster == 0x300 && colorCommand.id == 0 && colorCommand.hue >= 147 && colorCommand.hue <= 149);
  msg.attribute.data.type = 0x20;
  assert(ArdZigbee::action(ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID, &msg) == ESP_ERR_INVALID_ARG);
  msg.attribute.data.type = 0x10;
  for (unsigned i = 0; i < 16; ++i) assert(ArdZigbee::action(ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID, &msg) == ESP_OK);
  assert(ArdZigbee::action(ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID, &msg) == ESP_ERR_NO_MEM);
  unsigned count = 0;
  while (z.nextCommand(c)) ++count;
  assert(count == 16);
  // SDK thermostat attributes can be writable yet lack reporting descriptors.
  // Manual global reports must still carry exact types/values and remain paced.
  const ArdZigbee::Control* climate = z.controls();
  while (climate && climate->spec.kind != Kind::Climate) climate = climate->next.get();
  assert(climate);
  for (uint16_t id : { uint16_t(0), uint16_t(0x11), uint16_t(0x12), uint16_t(0x1c), uint16_t(0x29) }) esp_zb_zcl_get_attribute(climate->spec.endpoint, 0x201, 1, id)->access = 3;
  auto thermal = encode(climate->spec, field, state);
  size_t first = apsReports.size();
  bool completed = false;
  for (unsigned i = 0; i < 10 && !completed; ++i) completed = z.publish(climate->spec.endpoint, thermal, true);
  assert(completed && apsReports.size() - first == thermal.count);
  bool sawSetpoint = false, sawMode = false;
  for (size_t i = first; i < apsReports.size(); ++i) {
    const auto& packet = apsReports[i];
    assert(packet[0] == 0x18 && packet[2] == 0x0a);
    uint16_t id = packet[3] | packet[4] << 8;
    if (id == 0x12) {
      sawSetpoint = true;
      assert(packet[5] == 0x29 && packet[6] == 0x98 && packet[7] == 8);
    }
    if (id == 0x1c) {
      sawMode = true;
      assert(packet[5] == 0x30 && packet[6] == 4);
    }
  }
  assert(sawSetpoint && sawMode);
  for (double target : { 26.0, 12.0, 30.0, 10.0 }) {
    state["temperature"] = target;
    auto changed = encode(climate->spec, field, state);
    bool sent = false;
    for (unsigned retry = 0; retry < 10 && !sent; ++retry) sent = z.publish(climate->spec.endpoint, changed);
    assert(sent);
    int16_t heat = 0, cool = 0;
    memcpy(&heat, esp_zb_zcl_get_attribute(climate->spec.endpoint, 0x201, 1, 0x12)->data_p, 2);
    memcpy(&cool, esp_zb_zcl_get_attribute(climate->spec.endpoint, 0x201, 1, 0x11)->data_p, 2);
    assert(heat == int(target * 100) && cool == 3250);
    int16_t remote = 2200;
    assert(esp_zb_zcl_set_attribute_val(climate->spec.endpoint, 0x201, 1, 0x12, &remote, true) == ESP_ZB_ZCL_STATUS_SUCCESS);
  }
  // Reporting timers must scale with staged attributes before the stack starts.
  ArdZigbee scaled;
  Spec scaledSpec;
  scaledSpec.kind = Kind::Switch;
  for (unsigned i = 0; i < 48; ++i) assert(scaled.addControl(scaledSpec, encode(scaledSpec, f, false), "Scale"));
  ArdZigbee::_instance = nullptr;
  ESP.freeHeap = 128000;
  assert(scaled.begin(ArdZigbee::Options()));
  ArdZigbee::task(&scaled);
  assert(schedulerSlots == 80 + 48 * 2);
  assert(nativeEndpoints.endpoints[1]->clusters[0]->attributes[0x4000].bytes[0] == 3);
  puts("PASS: native SDK staging, startup-buffer release, late registration, state reports, retry/defer, typed commands, command queue saturation and reporting scheduler capacity");
}
