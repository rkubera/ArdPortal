// SPDX-License-Identifier: MIT
#pragma once
#include "MatterFields.h"

// Human-facing application commands and state updates, not a CHIP packet dump.
namespace ArdMatterTraffic {
using V = ArdJSON::JSONVar;
inline String clip(String text, size_t limit) {
  if (text.length() <= limit) return text;
  while (limit && (uint8_t(text[limit]) & 0xC0) == 0x80) --limit;
  text.remove(limit);
  return text;
}
inline String name(const V& field) {
  String text = field["name"].asString();
  if (!text.length()) text = field["id"].asString();
  return clip(text, 80);
}
inline String summary(const V& field, const V& value) {
  if (value.type() == V::Type::Boolean) {
#if ARDPORTAL_ENABLE_CONTROL_BINARY_SENSOR
    auto kind = ArdMatterFields::kind(field);
    if (kind == ArdMatterFields::Kind::Contact) return value.asBool() ? "Open" : "Closed";
    if (kind == ArdMatterFields::Kind::Occupancy) return value.asBool() ? "Occupied" : "Clear";
    if (kind == ArdMatterFields::Kind::Leak) return value.asBool() ? "Leak detected" : "Dry";
#endif
    return value.asBool() ? "On" : "Off";
  }
  if (value.isNull() || value.isUndefined()) return "Unavailable";
  if ((value.type() == V::Type::Number)) {
    String text = ArdJSON::JSON.stringify(value);
    String unit = field["unit"].asString();
    if (!unit.length()) {
      auto kind = ArdMatterFields::kind(field);
      if (kind == ArdMatterFields::Kind::Temperature) unit = "°C";
      else if (kind == ArdMatterFields::Kind::Humidity) unit = "%";
    }
    if (unit.length()) text += " " + unit;
    return text;
  }
  if (value.type() != V::Type::Object) return clip(value.asString(), 160);
#if ARDPORTAL_ENABLE_CONTROL_LIGHT || ARDPORTAL_ENABLE_CONTROL_FAN || ARDPORTAL_ENABLE_CONTROL_CLIMATE || ARDPORTAL_ENABLE_CONTROL_COVER
  String text;
  auto add = [&](const char* label, const String& part) {
    if (!part.length() || text.length() >= 160) return;
    if (text.length()) text += "; ";
    text += label;
    text += part;
  };
#if ARDPORTAL_ENABLE_CONTROL_LIGHT || ARDPORTAL_ENABLE_CONTROL_FAN || ARDPORTAL_ENABLE_CONTROL_COVER
  if (value.hasOwnProperty("state")) add("", value["state"].type() == V::Type::Boolean ? (value["state"].asBool() ? "On" : "Off") : value["state"].asString());
#endif
#if ARDPORTAL_ENABLE_CONTROL_CLIMATE
  if (value.hasOwnProperty("mode")) add("Mode: ", value["mode"].asString());
#endif
#if ARDPORTAL_ENABLE_CONTROL_CLIMATE
  for (const char* key : { "temperature", "temperature_low", "temperature_high" })
    if (value.hasOwnProperty(key)) add(!strcmp(key, "temperature_low") ? "Heat: " : !strcmp(key, "temperature_high") ? "Cool: "
                                                                                                                     : "Target: ",
                                       ArdJSON::JSON.stringify(value[key]) + " °C");
#endif
#if ARDPORTAL_ENABLE_CONTROL_LIGHT || ARDPORTAL_ENABLE_CONTROL_FAN || ARDPORTAL_ENABLE_CONTROL_COVER
  auto percent = [&](const char* key, const char* label, bool brightness = false) {
    if (!value.hasOwnProperty(key)) return;
    String amount = brightness ? String(unsigned(round(value[key].asDouble() * 100.0 / 255.0))) : ArdJSON::JSON.stringify(value[key]);
    add(label, amount + "%");
  };
#if ARDPORTAL_ENABLE_CONTROL_FAN
  percent("percentage", "Speed: ");
#endif
#if ARDPORTAL_ENABLE_CONTROL_COVER
  percent("position", "Position: ");
  percent("tilt", "Tilt: ");
#endif
#if ARDPORTAL_ENABLE_CONTROL_LIGHT
  percent("brightness", "Brightness: ", true);
#endif
#endif
#if ARDPORTAL_ENABLE_CONTROL_LIGHT
  if (value.hasOwnProperty("color")) {
    auto color = value["color"];
    add("Color: ", String(unsigned(color["r"].asDouble())) + ", " + String(unsigned(color["g"].asDouble())) + ", " + String(unsigned(color["b"].asDouble())));
  }
#endif
  return text.length() ? clip(text, 192) : String("Changed");
#else
  return "Changed";
#endif
}
inline const char* action(ArdMatterFields::Operation operation, uint32_t value) {
#if ARDPORTAL_ENABLE_CONTROL_SWITCH || ARDPORTAL_ENABLE_CONTROL_SLIDER || ARDPORTAL_ENABLE_CONTROL_LIGHT || ARDPORTAL_ENABLE_CONTROL_FAN || ARDPORTAL_ENABLE_CONTROL_CLIMATE || ARDPORTAL_ENABLE_CONTROL_COVER
  using O = ArdMatterFields::Operation;
#endif
  (void)value;
  switch (operation) {
#if ARDPORTAL_ENABLE_CONTROL_SWITCH || ARDPORTAL_ENABLE_CONTROL_SLIDER || ARDPORTAL_ENABLE_CONTROL_LIGHT || ARDPORTAL_ENABLE_CONTROL_FAN
    case O::OnOff: return value == 0 ? "Off" : value == 1 ? "On"
                                                          : "Power";
#endif
#if ARDPORTAL_ENABLE_CONTROL_SLIDER || ARDPORTAL_ENABLE_CONTROL_LIGHT
    case O::Level: return "Brightness";
#endif
#if ARDPORTAL_ENABLE_CONTROL_LIGHT
    case O::Hue: return "Hue";
    case O::Saturation: return "Saturation";
    case O::X:
    case O::Y: return "Color";
#endif
#if ARDPORTAL_ENABLE_CONTROL_FAN
    case O::Speed: return "Fan speed";
    case O::FanMode: return "Fan mode";
#endif
#if ARDPORTAL_ENABLE_CONTROL_CLIMATE
    case O::Mode: return "Mode";
    case O::Heat: return "Heating target";
    case O::Cool: return "Cooling target";
#endif
#if ARDPORTAL_ENABLE_CONTROL_COVER
    case O::Lift: return "Position";
    case O::Tilt: return "Tilt";
    case O::Open: return "Open";
    case O::Close: return "Close";
    case O::Stop: return "Stop";
#endif
    default: return "Change";
  }
  return "Change";
}
inline String received(const V& field, const ArdMatterFields::Command& command, const V& value, bool accepted) {
  String text(F("Command: "));
  text += name(field);
  text += F(" — ");
  text += action(command.operation, command.value);
  text += F(" — ");
  text += accepted ? F("OK") : F("Rejected");
  if (accepted && command.operation != ArdMatterFields::Operation::OnOff) {
    text += F(": ");
    text += summary(field, value);
  }
  return clip(std::move(text), 300);
}
inline String reported(const V& field, const V& value) {
  String text(F("State: "));
  text += name(field);
  text += F(" — ");
  text += summary(field, value);
  return clip(std::move(text), 300);
}
}  // namespace ArdMatterTraffic
