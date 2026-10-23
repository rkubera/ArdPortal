// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ArdJSON.h"
#include "ControlClassData.h"
#include "ControlTime.h"
#include <cmath>
namespace ArdControl {
// A borrowed, allocation-free description. State remains owned by ArdAppControls.
enum Capability : uint16_t {
  OnOff = 1,
  Level = 2,
  Color = 4,
  Position = 8,
  Tilt = 16,
  Measurement = 32,
  Setpoint = 64,
  Options = 128,
  Action = 256,
  Event = 512,
  Text = 1024,
  Temperature = 2048,
  Speed = 4096
};
struct Model {
  Type type = Type::Invalid;
  Class classId = Class::None;
  uint16_t capabilities = 0;
  bool has(Capability capability) const {
    return (capabilities & capability) != 0;
  }
};
static_assert(sizeof(Model) <= 4, "Control model must stay compact");
inline Model describe(const ArdJSON::JSONVar& field) {
  Model result{ typeOf(field), classOf(field), 0 };
  switch (result.type) {
    case Type::Switch: result.capabilities = OnOff; break;
    case Type::Slider: result.capabilities = Setpoint; break;
    case Type::Text: result.capabilities = numeric(result.classId) ? Measurement : Text; break;
    case Type::Select: result.capabilities = Options; break;
    case Type::Light: result.capabilities = OnOff | Level | Color; break;
    case Type::Cover: result.capabilities = Position | Tilt | Action; break;
    case Type::Climate: result.capabilities = Temperature | Setpoint | Options; break;
    case Type::Fan: result.capabilities = OnOff | Speed | Options; break;
    case Type::Valve: result.capabilities = Position | Action; break;
    case Type::Humidifier: result.capabilities = OnOff | Setpoint | Options; break;
    case Type::WaterHeater: result.capabilities = Temperature | Setpoint | Options; break;
    case Type::BinarySensor:
    case Type::DeviceTracker: result.capabilities = Measurement; break;
    case Type::Edit:
    case Type::Date:
    case Type::Time:
    case Type::Datetime: result.capabilities = Text | Setpoint; break;
    case Type::Siren: result.capabilities = OnOff | Action | Options; break;
    case Type::Event:
    case Type::DeviceTrigger:
    case Type::Tag: result.capabilities = Event; break;
    case Type::Button:
    case Type::Scene:
    case Type::Notify:
    case Type::Infrared:
    case Type::Lock:
    case Type::AlarmControlPanel:
    case Type::Vacuum:
    case Type::LawnMower:
    case Type::Update: result.capabilities = Action; break;
    default: break;
  }
#if ARDPORTAL_ENABLE_CONTROL_INFRARED
  if (result.type == Type::Infrared) {
    if (result.classId == Class::Receiver) result.capabilities = Event;
    else if (result.classId == Class::Transceiver) result.capabilities |= Event;
  }
#endif
  // Custom control arrays replace presets; derive their actual capabilities.
  if (field["controls"].type() == ArdJSON::JSONVar::Type::Array) {
    result.capabilities &= Measurement | Event | Text | Temperature;
    for (size_t i = 0; i < field["controls"].length(); ++i) {
      const auto& control = field["controls"][i];
      const String* type = control["type"].stringValue();
      if (!type) continue;
#if ARDPORTAL_CONTROL_SUPPORT_SWITCH
      if (*type == "switch") result.capabilities |= OnOff;
#endif
#if ARDPORTAL_CONTROL_SUPPORT_SLIDER
      if (*type == "slider") {
        result.capabilities |= Setpoint;
        const String* key = control["key"].stringValue();
        if (key) {
#if ARDPORTAL_ENABLE_CONTROL_LIGHT
          if (*key == "brightness") result.capabilities |= Level;
#endif
#if ARDPORTAL_ENABLE_CONTROL_FAN
          if (*key == "percentage") result.capabilities |= Speed;
#endif
#if ARDPORTAL_ENABLE_CONTROL_COVER || ARDPORTAL_ENABLE_CONTROL_VALVE
          if (*key == "position") result.capabilities |= Position;
#endif
#if ARDPORTAL_ENABLE_CONTROL_COVER
          if (*key == "tilt") result.capabilities |= Tilt;
#endif
        }
      }
#endif
#if ARDPORTAL_ENABLE_CONTROL_LIGHT
      if (*type == "color") result.capabilities |= Color;
#endif
#if ARDPORTAL_CONTROL_SUPPORT_SELECT
      if (*type == "select") result.capabilities |= Options;
#endif
#if ARDPORTAL_CONTROL_SUPPORT_ACTIONS
      if (type->indexOf("action") == 0) result.capabilities |= Action;
#endif
    }
  }
  return result;
}
// Shared read-only value rules, independent of protocol discovery metadata.
inline bool validReading(const ArdJSON::JSONVar& field, const ArdJSON::JSONVar& value) {
  using V = ArdJSON::JSONVar;
  Class id = classOf(field);
  if (!validClass(field)) return false;
  if (value.isNull() && id != Class::None && id != Class::Text) return true;
  if (numeric(id)) {
    double n;
    return value.type() == V::Type::Number && value.toDouble(n) && std::isfinite(n) && std::isfinite(float(n)) && (id != Class::Uptime || n >= 0);
  }
  if (id == Class::Enum) {
    const String* name = value.stringValue();
    if (!name) return false;
    for (size_t i = 0; i < field["options"].length(); ++i) {
      const auto& option = field["options"][i];
      if ((option.type() == V::Type::String ? option.asString() : option["value"].asString()) == *name) return true;
    }
    return false;
  }
  const String* text = value.stringValue();
#if ARDPORTAL_ENABLE_CLASSES
  if (id == Class::Date) return text && date(*text);
  if (id == Class::Timestamp) return text && datetime(*text);
#endif
  if (id == Class::None || id == Class::Text) {
    double n;
    if (value.type() == V::Type::Number) return value.toDouble(n) && std::isfinite(n);
  }
  return text && text->length() <= 512 && strlen(text->c_str()) == text->length();
}
}
