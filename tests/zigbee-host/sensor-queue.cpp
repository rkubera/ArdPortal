// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
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

void release(ArdZigbee& z) {
  vQueueDelete(z._events);
  vQueueDelete(z._traffic);
  vQueueDelete(z._commands);
  ArdZigbee::_instance = nullptr;
}

int main() {
  for (auto kind : { Kind::None, Kind::State, Kind::Number, Kind::Temperature, Kind::Humidity, Kind::Pressure, Kind::Illuminance, Kind::Flow, Kind::CarbonDioxide, Kind::PM25, Kind::WindSpeed, Kind::Voltage, Kind::Current, Kind::Power, Kind::Binary, Kind::Update }) {
    Spec s;
    s.kind = kind;
    assert(!acceptsCommands(s));
    s.part = 0;
    assert(!acceptsCommands(s));
  }
  for (auto kind : { Kind::Switch, Kind::Slider, Kind::Select, Kind::Light, Kind::Climate, Kind::Cover, Kind::Lock, Kind::Siren }) {
    Spec s;
    s.kind = kind;
    assert(acceptsCommands(s));
  }
  Spec action;
  action.kind = Kind::Action;
  assert(!acceptsCommands(action));
  action.part = 0;
  assert(acceptsCommands(action));

  ArdZigbee sensors;
  Spec temperature;
  temperature.kind = Kind::Temperature;
  auto values = encode(temperature, V::object(), V(21.5));
  assert(sensors.addControl(temperature, values, "Temperature"));
  Spec state;
  state.kind = Kind::State;
  state.part = 0;
  assert(sensors.addControl(state, Values{}, "Read-only part"));
  queueBudget = 2;  // Only event and traffic queues may be allocated.
  assert(sensors.begin(ArdZigbee::Options()));
  assert(!sensors._commands && queueBudget == 0);
  assert(sensors.createControls(&nativeEndpoints));
  sensors._ready = true;
  sensors._state = ArdZigbee::State::Connected;
  assert(sensors.publish(2, values, true));
  Command command;
  assert(!sensors.nextCommand(command));
  sensors.queueReports(size_t(-1), true);
  assert(sensors.nextReport(100, 0));
  // Ignore irrelevant writes to a measurement endpoint without requiring a queue.
  uint8_t data = 1;
  esp_zb_zcl_set_attr_value_message_t message{};
  message.info.dst_endpoint = 2;
  message.info.cluster = 6;
  message.attribute.id = 0;
  message.attribute.data = { 0x10, 1, &data };
  assert(ArdZigbee::action(ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID, &message) == ESP_OK);
  release(sensors);

  // One writable compound part is enough, regardless of sensor endpoints.
  for (int budget : { 2, 3 }) {
    ArdZigbee mixed;
    assert(mixed.addControl(temperature, values, "Sensor"));
    assert(mixed.addControl(action, Values{}, "Button part"));
    queueBudget = budget;
    bool ok = mixed.begin(ArdZigbee::Options());
    assert(ok == (budget == 3));
    assert(bool(mixed._commands) == ok);
    if (ok) {
      assert(mixed._commands->capacity == 16);
      message.info.dst_endpoint = 3;
      assert(ArdZigbee::action(ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID, &message) == ESP_OK);
      assert(mixed.nextCommand(command) && command.endpoint == 3 && command.value == 1);
    } else {
      assert(!mixed._events && !mixed._traffic);  // Failed allocation releases earlier queues.
    }
    release(mixed);
  }
  queueBudget = -1;
  puts("PASS: Zigbee sensor-only/read-only endpoints omit command queue; reports, mixed controls, compound actions and allocation failure preserved");
}
