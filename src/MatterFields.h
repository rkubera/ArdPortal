// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ControlModel.h"
#include <algorithm>

// Only semantics and bounded wire values live here. No Zigbee or SDK types.
namespace ArdMatterFields {
using V = ArdJSON::JSONVar;
enum class Kind : uint8_t { Unsupported,
                            Outlet,
                            OnOffLight,
                            Dimmer,
                            Light,
                            Temperature,
                            Humidity,
                            Pressure,
                            Illuminance,
                            Contact,
                            Occupancy,
                            Leak,
                            Fan,
                            Climate,
                            Cover };
enum class Operation : uint8_t { OnOff,
                                 Level,
                                 Hue,
                                 Saturation,
                                 X,
                                 Y,
                                 Speed,
                                 FanMode,
                                 Mode,
                                 Heat,
                                 Cool,
                                 Lift,
                                 Tilt,
                                 Open,
                                 Close,
                                 Stop };
struct Command {
  uint16_t field;
  Operation operation;
  uint32_t value;
};
static_assert(sizeof(Command) <= 8, "Matter commands must remain bounded");

inline Kind kind(const V& field) {
  using T = ArdControl::Type;
#if ARDPORTAL_ENABLE_CONTROL_SWITCH || ARDPORTAL_ENABLE_CONTROL_SLIDER || ARDPORTAL_ENABLE_CONTROL_TEXT || ARDPORTAL_ENABLE_CONTROL_BINARY_SENSOR || ARDPORTAL_ENABLE_CONTROL_COVER
  using C = ArdControl::Class;
#endif
  auto model = ArdControl::describe(field);
  if (field["readonly"].asBool() && model.type != T::Text && model.type != T::BinarySensor) return Kind::Unsupported;
  switch (model.type) {
#if ARDPORTAL_ENABLE_CONTROL_SWITCH
    case T::Switch:
      if (model.classId == C::Light) return Kind::OnOffLight;
      if (model.classId == C::None || model.classId == C::Switch || model.classId == C::Outlet) return Kind::Outlet;
      break;
#endif
#if ARDPORTAL_ENABLE_CONTROL_SLIDER
    case T::Slider:
      if (model.classId == C::Brightness && field["min"].asDouble() == 0 && field["max"].asDouble() > 0) return Kind::Dimmer;
      break;
#endif
#if ARDPORTAL_ENABLE_CONTROL_TEXT
    case T::Text:
      switch (model.classId) {
        case C::Temperature: return Kind::Temperature;
        case C::Humidity: return Kind::Humidity;
        case C::Pressure:
        case C::AtmosphericPressure: return Kind::Pressure;
        case C::Illuminance: return Kind::Illuminance;
        default: break;
      }
      break;
#endif
#if ARDPORTAL_ENABLE_CONTROL_BINARY_SENSOR
    case T::BinarySensor:
      switch (model.classId) {
        case C::Door:
        case C::Window:
        case C::Opening: return Kind::Contact;
        case C::Motion:
        case C::Occupancy:
        case C::Presence: return Kind::Occupancy;
        case C::Moisture: return Kind::Leak;
        default: break;
      }
      break;
#endif
#if ARDPORTAL_ENABLE_CONTROL_LIGHT
    case T::Light:
      if (model.has(ArdControl::OnOff) && model.has(ArdControl::Level) && model.has(ArdControl::Color)) return Kind::Light;
      break;
#endif
#if ARDPORTAL_ENABLE_CONTROL_FAN
    case T::Fan:
      if (model.has(ArdControl::OnOff) && model.has(ArdControl::Speed)) return Kind::Fan;
      break;
#endif
#if ARDPORTAL_ENABLE_CONTROL_CLIMATE
    case T::Climate:
      {
        bool heat = false, cool = false;
        auto modes = field["modes"];
        for (size_t i = 0; i < modes.length(); ++i) {
          auto mode = modes[i].asString();
          if (mode == "heat" || mode == "heat_cool") heat = true;
          else if (mode != "off" && mode != "cool") return Kind::Unsupported;
          if (mode == "cool" || mode == "heat_cool") cool = true;
        }
        bool dual = false;
        for (size_t i = 0; i < modes.length(); ++i) dual |= modes[i].asString() == "heat_cool";
        if (heat && cool && !dual) return Kind::Unsupported;
        // Arduino's native thermostat fixes its setpoint limits to these ranges.
        double minimum = field["min"].asDouble(), maximum = field["max"].asDouble();
        if ((heat || cool) && (!heat || (minimum >= 7 && maximum <= 30)) && (!cool || (minimum >= 16 && maximum <= 32))) return Kind::Climate;
        break;
      }
#endif
#if ARDPORTAL_ENABLE_CONTROL_COVER
    case T::Cover:
      if (model.has(ArdControl::Position) && model.has(ArdControl::Tilt) && (model.classId == C::None || model.classId == C::Blind || model.classId == C::Curtain || model.classId == C::Shade || model.classId == C::Shutter || model.classId == C::Awning)) return Kind::Cover;
      break;
#endif
    default: break;
  }
  return Kind::Unsupported;
}

inline String state(const V& field, bool on) {
  auto controls = field["controls"];
  for (size_t i = 0; i < controls.length(); ++i)
    if (controls[i]["key"].asString() == "state" && controls[i]["type"].asString() == "switch") return controls[i][on ? "on" : "off"].asString();
  return on ? "ON" : "OFF";
}

inline uint8_t level(double value, double maximum) {
  return maximum > 0 ? uint8_t(std::max(0.0, std::min(254.0, round(value * 254.0 / maximum)))) : 0;
}
inline double fromLevel(uint32_t value, const V& field) {
  double maximum = field["max"].asDouble(), step = field["step"].isUndefined() ? 1 : field["step"].asDouble();
  double result = double(value) * maximum / 254.0;
  return step > 0 ? std::min(maximum, round(result / step) * step) : result;
}
inline const char* mode(uint32_t value) {
  switch (value) {
    case 0: return "off";
    case 1: return "heat_cool";
    case 3: return "cool";
    case 4: return "heat";
    default: return nullptr;
  }
}
inline bool contains(const V& values, const char* text) {
  for (size_t i = 0; i < values.length(); ++i)
    if (values[i].asString() == text) return true;
  return false;
}
}
