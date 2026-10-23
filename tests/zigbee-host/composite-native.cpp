#define ARDPORTAL_ENABLE_DYNAMIC_PAGES 1
#define ARDPORTAL_ENABLE_CONTROL_TEXT 0
#define ARDPORTAL_ENABLE_CONTROL_SWITCH 0
#define ARDPORTAL_ENABLE_CONTROL_SLIDER 0
#define ARDPORTAL_ENABLE_CONTROL_SELECT 0
#define ARDPORTAL_ENABLE_CONTROL_HUMIDIFIER 1
#define ARDPORTAL_ENABLE_CONTROL_WATER_HEATER 1
#define ARDPORTAL_ZIGBEE_ROUTER 0
#include "ZigbeeFields.h"
#define private public
#include "ArdZigbeeImpl.h"
#undef private
#include <cassert>
uint32_t fakeMillis = 0;
time_t fakeUnixTime = 0;
EspStub ESP;
using namespace ArdZigbeeFields;
/** @brief Verify composite primitives and measurements remain available when standalone controls are disabled. @return Zero on success. */
int main() {
  ArdZigbee z;
  V f = ArdJSON::JSON.parse(R"({"controls":[{"type":"switch","key":"state","on":"ON","off":"OFF"},{"type":"slider","key":"temperature","min":30,"max":80,"step":1},{"type":"select","key":"mode","options":["off","eco"]},{"type":"action","payload":"START"}]})");
  V state = ArdJSON::JSON.parse(R"({"state":"ON","temperature":50,"mode":"eco"})");
  for (size_t i = 0; i < 4; ++i) {
    Spec spec = describePart(f, i);
    assert(spec.kind != Kind::None && z.addControl(spec, encode(spec, f, state), "Composite part"));
  }
  for (auto kind : { Kind::Temperature, Kind::Humidity }) {
    Spec spec;
    spec.kind = kind;
    spec.part = 0xfffe;
    Spec scalar = spec;
    scalar.part = 0xffff;
    assert(z.addControl(spec, encode(scalar, f, 20), "Current reading"));
  }
  Spec status;
  status.kind = Kind::State;
  status.part = 0xfffe;
  status.states = 2;
  assert(z.addControl(status, Values(), "Status"));
  assert(z.createControls(&nativeEndpoints));
  assert(nativeEndpoints.endpoints.size() == 7);
  assert(nativeEndpoints.endpoints[2]->clusters[0]->attributes.count(0x0e));
  assert(nativeEndpoints.endpoints[6]->clusters.count(0x402) && nativeEndpoints.endpoints[7]->clusters.count(0x405));
  puts("PASS: composite SDK endpoints with standalone switch/slider/select/text disabled, standard labels and current measurement clusters");
}
