// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ControlModel.h"
#include "HaEntityData.h"
#include "HaClassData.h"
namespace ArdHa {
using V = ArdJSON::JSONVar;
inline String component(const V& field) {
  ArdControl::Model model{ ArdControl::typeOf(field), ArdControl::classOf(field), 0 };
#if ARDPORTAL_ENABLE_CONTROL_SWITCH
  if (model.type == ArdControl::Type::Switch && model.classId == ArdControl::Class::Light) return "light";
#endif
  switch (model.type) {
#if ARDPORTAL_ENABLE_CONTROL_SLIDER
    case ArdControl::Type::Slider: return "number";
#endif
#if ARDPORTAL_ENABLE_CONTROL_TEXT
    case ArdControl::Type::Text: return "sensor";
#endif
#if ARDPORTAL_ENABLE_CONTROL_EDIT
    case ArdControl::Type::Edit: return "text";
#endif
#if ARDPORTAL_ENABLE_CONTROL_DEVICE_TRIGGER
    case ArdControl::Type::DeviceTrigger: return "device_automation";
#endif
    default: return field["type"].asString();
  }
}
inline V defaults(const String& type) {
#if ARDPORTAL_ENABLE_HA && ARDPORTAL_CONTROL_SUPPORT_EXTENDED
  const uint8_t id = uint8_t(ArdControl::typeOf(type));
  for (size_t i = 0; i < ARD_HA_SPEC_COUNT; ++i) {
    if (pgm_read_byte(ARD_HA_SPEC_TYPES + i) != id) continue;
    const char* json;
    memcpy_P(&json, ARD_HA_SPECS + i, sizeof(json));
    return ArdJSON::JSON.parse(String(FPSTR(json)));
  }
#else
  (void)type;
#endif
  return V::object();
}
inline void classification(const V& field, V& out) {
  ArdControl::Model model{ ArdControl::typeOf(field), ArdControl::classOf(field), 0 };
  auto metadata = ArdControl::classInfo(model.classId);
  bool numericValue = model.type == ArdControl::Type::Text || model.type == ArdControl::Type::Slider;
  if (numericValue && metadata.unit && strlen_P(metadata.unit)) out["unit_of_measurement"] = String(FPSTR(metadata.unit));
  const char* name = className(model, metadata);
  if (name) out["device_class"] = String(FPSTR(name));
#if ARDPORTAL_ENABLE_CONTROL_TEXT
  if (model.type == ArdControl::Type::Text && model.classId == ArdControl::Class::Enum) {
    V options = V::array();
    for (size_t i = 0; i < field["options"].length(); ++i) {
      const V& option = field["options"][i];
      options.push(option.type() == V::Type::String ? option : option["value"]);
    }
    out["options"] = std::move(options);
  }
  if (model.type == ArdControl::Type::Text && metadata.numeric) {
    out["value_template"] = "{{ value if is_number(value) else 'None' }}";
    if (metadata.measurement) out["state_class"] = "measurement";
  }
#endif
}
}
