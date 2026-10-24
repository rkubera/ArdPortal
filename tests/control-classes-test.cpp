#include <cassert>
#include "ControlClasses.h"
#include "ZigbeeFields.h"
uint32_t fakeMillis = 0;
int main() {
  using V = ArdJSON::JSONVar;
  using namespace ArdControl;
  auto field = ArdJSON::JSON.parse(R"({"type":"text"})");
  assert(validClass(field) && classOf(field) == Class::None);
  assert(validReading(field, "ready") && validReading(field, 42));
  V text = "ready", number = 42;
  assert(text.stringValue() && !number.stringValue());
  for (auto name : { "number", "temperature", "humidity", "pressure", "illuminance", "flow", "carbon_dioxide", "pm25", "wind_speed", "voltage", "current", "power", "energy" }) {
    field["class"] = name;
#if ARDPORTAL_ENABLE_CLASSES
    assert(validClass(field));
    assert(validReading(field, 12) && validReading(field, nullptr) && !validReading(field, "bad"));
    assert(ArdZigbeeFields::describe(field, 12).kind != ArdZigbeeFields::Kind::None);
#else
    assert(!validClass(field));
#endif
  }
  field.remove("class");
  assert(validClass(field));
  field["textType"] = "temperature";
  assert(!validClass(field));
  field.remove("textType");
  field["device_class"] = "temperature";
  assert(!validClass(field));
  field.remove("device_class");
#if ARDPORTAL_ENABLE_CLASSES
  field["class"] = "outlet";
  assert(!validClass(field));
  field["type"] = "switch";
  assert(validClass(field) && describe(field).has(OnOff));
  auto spec = ArdZigbeeFields::describe(field, false);
  assert(spec.classId == Class::Outlet);
  field["type"] = "slider";
  assert(!validClass(field));
  field["class"] = "temperature";
  assert(validClass(field) && describe(field).has(Setpoint));
  field["class"] = "timestamp";
  assert(!validClass(field));
  field["type"] = "text";
  field["class"] = "enum";
  field["options"] = ArdJSON::JSON.parse(R"(["idle","running"])");
  assert(validReading(field, "idle") && !validReading(field, "broken"));
#endif
  for (auto bad : { V(true), V(42), V(nullptr), V(""), V("unknown"), V(String(std::string("temperature\0bad", 15))) }) {
    field["class"] = bad;
    assert(!validClass(field));
  }
}
