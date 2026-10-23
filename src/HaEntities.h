// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ControlModel.h"
#include "HaEntityData.h"
#include "HaClassData.h"
namespace ArdHa {
using V = ArdJSON::JSONVar;
inline String component(const V& field) {
  auto model = ArdControl::describe(field);
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
#if ARDPORTAL_ENABLE_HA
  for (size_t i = 0; i < ARD_HA_SPEC_COUNT; ++i) {
    ArdHaSpec spec;
    memcpy_P(&spec, &ARD_HA_SPECS[i], sizeof(spec));
    if (strcmp_P(type.c_str(), spec.name) == 0) return ArdJSON::JSON.parse(String(FPSTR(spec.json)));
  }
#else
  (void)type;
#endif
  return V::object();
}
inline void classification(const V& field, V& out) {
  auto model = ArdControl::describe(field);
  auto metadata = ArdControl::classInfo(model.classId);
  bool numericValue = model.type == ArdControl::Type::Text || model.type == ArdControl::Type::Slider;
  if (numericValue && metadata.unit && strlen_P(metadata.unit)) out["unit_of_measurement"] = String(FPSTR(metadata.unit));
  const char* name = className(model);
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
  if (model.type == ArdControl::Type::Text && ArdControl::numeric(model.classId)) {
    out["value_template"] = "{{ value if is_number(value) else 'None' }}";
    if (metadata.measurement) out["state_class"] = "measurement";
  }
#endif
}
}
