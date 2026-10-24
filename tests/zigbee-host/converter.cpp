#include <cassert>
#include <vector>
#include <memory>
#include "ZigbeeFields.h"
#include "JsonCodec.h"
#include "ZigbeeConverterLayout.h"
uint32_t fakeMillis = 0;
using V = ArdJSON::JSONVar;
namespace ArdHeap {
bool allowed = true;
bool permits(size_t, size_t) {
  return allowed;
}
}
namespace ArdControl {
bool normalize(V&) {
  return true;
}
}
struct ArdZigbee {
  struct Control {
    ArdZigbeeFields::Spec spec;
    String label;
    std::unique_ptr<Control> next;
  };
};
struct ArdPortal {
  struct Dynamic {
    std::vector<V> fields;
    V at(size_t i) {
      return fields.at(i);
    }
    size_t fieldWork(size_t) {
      return 0;
    }
    size_t fieldBlock(size_t) {
      return 0;
    }
  } _dynamic;
  ArdJsonCodec _json;
  String _response;
  const char* _page = nullptr;
  size_t _pageLength = 0, _pageOffset = 0;
  const ArdZigbee::Control* _converterControl = nullptr;
  bool _converterDownload = true, _converterPython = false;
  uint8_t _converterStage = 0;
  String zigbeeConverterControlJson(const ArdZigbee::Control&);
  bool prepareZigbeeConverterPart();
};
#undef ARDPORTAL_ZIGBEE_HTTP_EXPORT_SUPPORT
#define ARDPORTAL_ZIGBEE_HTTP_EXPORT_SUPPORT 1
#undef ARDPORTAL_ZIGBEE_CONVERTER_SUPPORT
#define ARDPORTAL_ZIGBEE_CONVERTER_SUPPORT 1
#undef ARDPORTAL_ZHA_QUIRK_SUPPORT
#define ARDPORTAL_ZHA_QUIRK_SUPPORT 1
#include "ZigbeeConverterImpl.h"
/** @brief Verify generated metadata, deterministic layout identity and retry-safe output cursors. @return Zero on success. */
int main() {
  ArdPortal p;
  p._dynamic.fields.push_back(ArdJSON::JSON.parse(R"({"id":"mode","name":"Mode \"quoted\"","type":"select","options":[{"value":"off"},{"value":"eco"},{"value":"comfort"}]})"));
  ArdZigbee::Control control;
  control.spec.field = 0;
  control.spec.endpoint = 3;
  control.spec.kind = ArdZigbeeFields::Kind::Select;
  String first = p.zigbeeConverterControlJson(control);
  assert(first.length());
  V parsed = p._json.parse(first);
  assert(parsed["options"][2].asString() == "comfort");
  assert(parsed["endpoint"].asString() == "mode_ep3");
  assert(parsed["label"].asString() == "Mode \"quoted\"");
  control.label = String();
  assert(p.zigbeeConverterControlJson(control) == first);
  uint32_t a = 2166136261UL, b = 0x9e3779b9UL, a2 = a, b2 = b;
  ArdZigbeeConverterLayout::append(a, b, first.c_str(), first.length());
  String second = p.zigbeeConverterControlJson(control);
  ArdZigbeeConverterLayout::append(a2, b2, second.c_str(), second.length());
  assert(a == a2 && b == b2);
  p._dynamic.fields[0]["options"][2]["value"] = "boost";
  second = p.zigbeeConverterControlJson(control);
  a2 = 2166136261UL;
  b2 = 0x9e3779b9UL;
  ArdZigbeeConverterLayout::append(a2, b2, second.c_str(), second.length());
  assert(a != a2 || b != b2);
  p._converterControl = &control;
  ArdHeap::allowed = false;
  assert(!p.prepareZigbeeConverterPart());
  assert(p._converterControl == &control && p._converterStage == 0);
  ArdHeap::allowed = true;
  assert(p.prepareZigbeeConverterPart());
  assert(!p._converterControl && p._converterStage == 1);
  assert(p.prepareZigbeeConverterPart());
  assert(p._page && !p._converterDownload && p._pageLength);
  control.spec.kind = ArdZigbeeFields::Kind::State;
  control.spec.states = 2;
  control.spec.part = 0xfffe;
  p._dynamic.fields[0] = ArdJSON::JSON.parse(R"({"id":"presence","type":"device_tracker"})");
  parsed = p._json.parse(p.zigbeeConverterControlJson(control));
  assert(parsed["options"][1].asString() == "home");
  p._converterPython = true;
  p._converterControl = &control;
  p._converterStage = 0;
  assert(p.prepareZigbeeConverterPart() && p._response.indexOf("json.loads(") == 0);
  assert(p.prepareZigbeeConverterPart() && p._page == ARD_ZHA_QUIRK_TAIL);
  puts("PASS: converter metadata, escaping, named options/status, shared layout identity and memory-pressure retry");
}
