// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ArdJSON.h"
#include "TextTypes.h"
#include <cmath>
#include <cstring>
namespace ArdZigbeeFields {
using V = ArdJSON::JSONVar;
enum class Kind : uint8_t { None,
                            Switch,
                            Slider,
                            Select,
                            Action,
                            State,
                            Number,
                            Temperature,
                            Humidity,
                            Pressure,
                            Illuminance,
                            Flow,
                            CarbonDioxide,
                            PM25,
                            WindSpeed,
                            Voltage,
                            Current,
                            Power,
                            Binary,
                            Light,
                            Climate,
                            Cover,
                            Lock,
                            Siren,
                            Update };
struct Spec {
  uint16_t field = 0, part = 0xffff;
  uint8_t endpoint = 0;
  Kind kind = Kind::None;
  float min = 0, max = 100, step = 1;
  uint16_t states = 0;
  bool fan = false;
  uint8_t sequence = 4;
};
struct Attribute {
  uint16_t cluster = 0, id = 0;
  uint8_t type = 0;
  union {
    uint32_t u;
    int32_t i;
    float f;
  } data{};
};
struct Values {
  Attribute attributes[12];
  uint8_t count = 0;
};
// Only fixed-size data crosses from the SDK task into the application loop.
struct Command {
  uint8_t endpoint = 0, kind = 0;
  uint16_t cluster = 0, id = 0;
  uint32_t value = 0;
  uint16_t duration = 0, x = 0, y = 0;
  uint8_t volume = 0, hue = 0, saturation = 0, level = 0;
};
/** @brief Convert a field definition to native Zigbee metadata. @param f Registered definition. @param value Effective state. @return None for unsupported fields. */
inline Spec describe(const V& f, const V& value) {
  Spec s;
  String t = f["type"].asString();
  if (t == "switch") s.kind = Kind::Switch;
  else if (t == "slider") {
    s.kind = Kind::Slider;
    s.min = f["min"].asDouble();
    s.max = f["max"].asDouble();
    s.step = f.hasOwnProperty("step") ? f["step"].asDouble() : 1;
  } else if (t == "select") {
    s.kind = Kind::Select;
    s.states = f["options"].length();
  } else if (t == "text") {
    (void)value;
#if ARDPORTAL_ENABLE_TEXT_TYPES
    switch (ArdText::pattern(f).type) {
      case ArdText::Type::Number: s.kind = Kind::Number; break;
      case ArdText::Type::Temperature: s.kind = Kind::Temperature; break;
      case ArdText::Type::Humidity: s.kind = Kind::Humidity; break;
      case ArdText::Type::Pressure: s.kind = Kind::Pressure; break;
      case ArdText::Type::Illuminance: s.kind = Kind::Illuminance; break;
      case ArdText::Type::Flow: s.kind = Kind::Flow; break;
      case ArdText::Type::CarbonDioxide: s.kind = Kind::CarbonDioxide; break;
      case ArdText::Type::PM25: s.kind = Kind::PM25; break;
      case ArdText::Type::WindSpeed: s.kind = Kind::WindSpeed; break;
      case ArdText::Type::Voltage: s.kind = Kind::Voltage; break;
      case ArdText::Type::Current: s.kind = Kind::Current; break;
      case ArdText::Type::Power: s.kind = Kind::Power; break;
      default: break;
    }
#endif
  } else if (t == "binary_sensor") s.kind = Kind::Binary;
  else if (t == "light") s.kind = Kind::Light;
  else if (t == "climate") {
    s.kind = Kind::Climate;
    s.min = f["min"].asDouble();
    s.max = f["max"].asDouble();
    s.step = f.hasOwnProperty("step") ? f["step"].asDouble() : 0.5;
    s.fan = f["fan_modes"].length();
    bool heat = false, cool = false;
    for (size_t i = 0; i < f["modes"].length(); ++i) {
      String m = f["modes"][i].asString();
      heat |= m == "heat" || m == "heat_cool";
      cool |= m == "cool" || m == "heat_cool";
    }
    s.sequence = heat && cool ? 4 : heat ? 2
                                         : 0;
  } else if (t == "cover") s.kind = Kind::Cover;
  else if (t == "lock") s.kind = Kind::Lock;
  else if (t == "siren") s.kind = Kind::Siren;
#if ARDPORTAL_ZIGBEE_OTA_SUPPORT
  else if (t == "update") s.kind = Kind::Update;
#endif
  if (s.kind == Kind::Slider && (!std::isfinite(s.min) || !std::isfinite(s.max) || !std::isfinite(s.step) || s.step <= 0)) s.kind = Kind::None;
  if (s.kind == Kind::Climate && (s.min < -273.15 || s.max > 327.67)) s.kind = Kind::None;
  return s;
}
/** @brief Identify composites supported using separate standard endpoints. @param f Definition. @return True for supported presets; custom text/JSON actions remain excluded. */
inline bool composite(const V& f) {
  String t = f["type"].asString();
  return t == "fan" || t == "valve" || t == "humidifier" || t == "water_heater" || t == "alarm_control_panel" || t == "vacuum" || t == "lawn_mower" || t == "button" || t == "scene" || t == "device_tracker";
}
/** @brief Describe one existing preset control as a standard endpoint. @param f Normalized field. @param part Control index. @return None for unsupported/custom payloads. */
inline Spec describePart(const V& f, size_t part) {
  Spec s;
  if (part >= f["controls"].length() || part >= 0xfffe) return s;
  s.part = part;
  const V& c = f["controls"][part];
  String t = c["type"].asString();
  if (t == "switch") s.kind = Kind::Switch;
  else if (t == "slider") {
    s.kind = Kind::Slider;
    s.min = c["min"].asDouble();
    s.max = c["max"].asDouble();
    s.step = c.hasOwnProperty("step") ? c["step"].asDouble() : 1;
  } else if (t == "select") {
    s.kind = Kind::Select;
    s.states = c["options"].length();
  } else if (t == "action" && c["payload"].type() == V::Type::String) s.kind = Kind::Action;
  return s;
}
/** @brief Resolve read-only compound state names. @param f Normalized definition. @return Stable names separated by commas, or empty for no discrete state. */
inline const char* stateNames(const V& f) {
  String t = f["type"].asString();
  if (t == "device_tracker") return "not_home,home";
  if (t == "valve") return "closed,opening,open,closing";
  if (t == "alarm_control_panel") return "disarmed,armed_home,armed_away,armed_night,pending,arming,triggered";
  if (t == "vacuum") return "idle,cleaning,docked,paused,returning,error";
  if (t == "lawn_mower") return "docked,mowing,paused,error";
  return "";
}
/** @brief Look up a compound-state ordinal or its name without a retained JSON array. @param f Normalized field. @param state State name to find. @param wanted One-based ordinal to decode, or zero. @return Number for lookup or string for decoding; null for unknown. */
inline V stateOption(const V& f, const String& state, uint16_t wanted = 0) {
  const char* at = stateNames(f);
  uint16_t index = 1;
  while (*at) {
    const char* end = strchr(at, ',');
    size_t bytes = end ? size_t(end - at) : strlen(at);
    String name;
    name.reserve(bytes);
    for (size_t i = 0; i < bytes; ++i) name += at[i];
    if (wanted ? index == wanted : state == name) return wanted ? V(name) : V(index);
    if (!end) break;
    at = end + 1;
    ++index;
  }
  return V(nullptr);
}
/** @brief Count discrete status values. @param f Normalized definition. @return Number of states. */
inline uint16_t stateCount(const V& f) {
  const char* names = stateNames(f);
  if (!*names) return 0;
  uint16_t count = 1;
  while (*names)
    if (*names++ == ',') ++count;
  return count;
}
/** @brief Append an unsigned ZCL attribute. @param v Output list. @param c Cluster. @param id Attribute. @param type ZCL data type. @param n Value. @return No value. */
inline void integer(Values& v, uint16_t c, uint16_t id, uint8_t type, uint32_t n) {
  if (v.count < 12) {
    auto& a = v.attributes[v.count++];
    a.cluster = c;
    a.id = id;
    a.type = type;
    a.data.u = n;
  }
}
/** @brief Append a floating ZCL attribute. @param v Output list. @param c Cluster. @param n Value. @return No value. */
inline void number(Values& v, uint16_t c, float n) {
  integer(v, c, 0x55, 0x39, 0);
  v.attributes[v.count - 1].data.f = n;
}
/** @brief Encode temperature as signed hundredths of Celsius. @param v Source temperature. @return ZCL invalid value when unavailable or outside range. */
inline uint16_t temperature(double n) {
  if (!std::isfinite(n) || n < -273.15 || n > 327.67) return 0x8000;
  return uint16_t(int16_t(std::lround(n * 100)));
}
/** @brief Encode a JSON temperature without copying its numeric value. @param v Source reading. @return Signed hundredths or the ZCL unavailable sentinel. */
inline uint16_t temperature(const V& v) {
  double n;
  return v.toDouble(n) ? temperature(n) : 0x8000;
}
/** @brief Encode a bounded unsigned measurement. @param v Reading. @param scale Wire multiplier. @return Unknown sentinel when outside the wire range. */
inline uint16_t unsignedMeasurement(const V& v, double scale) {
  double n;
  if (!v.toDouble(n) || !std::isfinite(n) || n < 0 || n * scale > 65534) return 0xffff;
  return uint16_t(std::lround(n * scale));
}
/** @brief Append a floating measurement attribute. @param out Attribute list. @param cluster Standard measurement cluster. @param value Reading. @param scale Wire multiplier. @return No value. */
inline void measurement(Values& out, uint16_t cluster, const V& value, double scale = 1) {
  double n;
  float wire = value.toDouble(n) && std::isfinite(n) && n >= 0 && std::isfinite(float(n * scale)) ? float(n * scale) : NAN;
  integer(out, cluster, 0, 0x39, 0);
  out.attributes[out.count - 1].data.f = wire;
}
/** @brief Translate standard mode names. @param mode Application name. @param fan Select fan-mode mapping. @return ZCL mode or 255 for unsupported name. */
inline uint8_t mode(const String& mode, bool fan = false) {
  if (!fan && mode == "heat_cool") return 1;
  const char* names[] = { "off", fan ? "low" : "auto", fan ? "medium" : "", fan ? "high" : "cool", fan ? "on" : "heat", fan ? "auto" : "", fan ? "smart" : "", fan ? "" : "fan_only", fan ? "" : "dry" };
  for (uint8_t i = 0; i < 9; ++i)
    if (mode.length() && mode == names[i]) return i;
  return 255;
}
/** @brief Convert an RGB color to standard hue/saturation. @param color RGB object. @param h Output hue. @param s Output saturation. @return No value. */
inline void rgbToHs(const V& color, uint8_t& h, uint8_t& s) {
  double r = color["r"].asDouble() / 255, g = color["g"].asDouble() / 255, b = color["b"].asDouble() / 255;
  double hi = std::fmax(r, std::fmax(g, b)), lo = std::fmin(r, std::fmin(g, b)), d = hi - lo, x = 0;
  if (d > 0) {
    x = hi == r ? (g - b) / d : hi == g ? 2 + (b - r) / d
                                        : 4 + (r - g) / d;
    x /= 6;
    if (x < 0) x += 1;
  }
  h = std::lround(x * 254);
  s = std::lround((hi ? d / hi : 0) * 254);
}
/** @brief Convert hue/saturation back to full-scale RGB. @param hue ZCL hue. @param saturation ZCL saturation. @return RGB object. */
inline V hsToRgb(uint8_t hue, uint8_t saturation) {
  double h = hue * 6.0 / 254, s = saturation / 254.0, x = s * (1 - std::fabs(std::fmod(h, 2) - 1)), r = 0, g = 0, b = 0;
  if (h < 1) {
    r = s;
    g = x;
  } else if (h < 2) {
    r = x;
    g = s;
  } else if (h < 3) {
    g = s;
    b = x;
  } else if (h < 4) {
    g = x;
    b = s;
  } else if (h < 5) {
    r = x;
    b = s;
  } else {
    r = s;
    b = x;
  }
  V c = V::object();
  c["r"] = int(std::lround((r + 1 - s) * 255));
  c["g"] = int(std::lround((g + 1 - s) * 255));
  c["b"] = int(std::lround((b + 1 - s) * 255));
  return c;
}
/** @brief Convert sRGB to linear light. @param n Normalized channel. @return Linear channel. */
inline double rgbLinear(double n) {
  return n <= 0.04045 ? n / 12.92 : std::pow((n + 0.055) / 1.055, 2.4);
}
/** @brief Convert linear light to sRGB. @param n Linear channel. @return Clamped normalized channel. */
inline double rgbGamma(double n) {
  n = std::fmax(0, std::fmin(1, n));
  return n <= 0.0031308 ? 12.92 * n : 1.055 * std::pow(n, 1.0 / 2.4) - 0.055;
}
/** @brief Encode sRGB chromaticity as Zigbee XY. @param color RGB object. @param x Output X. @param y Output Y. @return No value. */
inline void rgbToXy(const V& color, uint16_t& x, uint16_t& y) {
  double r = rgbLinear(color["r"].asDouble() / 255), g = rgbLinear(color["g"].asDouble() / 255), b = rgbLinear(color["b"].asDouble() / 255);
  double X = 0.4124 * r + 0.3576 * g + 0.1805 * b, Y = 0.2126 * r + 0.7152 * g + 0.0722 * b, Z = 0.0193 * r + 0.1192 * g + 0.9505 * b, sum = X + Y + Z;
  x = std::lround((sum ? X / sum : 0.3127) * 65535);
  y = std::lround((sum ? Y / sum : 0.3290) * 65535);
}
/** @brief Decode Zigbee XY to full-scale sRGB. @param x X chromaticity. @param y Y chromaticity. @return RGB object, or invalid for impossible chromaticity. */
inline V xyToRgb(uint16_t x, uint16_t y) {
  if (!y || uint32_t(x) + y > 65536) return V();
  double X = double(x) / y, Z = std::fmax(0.0, 65535.0 - x - y) / y;
  double r = std::fmax(0, 3.2406 * X - 1.5372 - 0.4986 * Z), g = std::fmax(0, -0.9689 * X + 1.8758 + 0.0415 * Z), b = std::fmax(0, 0.0557 * X - 0.2040 + 1.0570 * Z), hi = std::fmax(r, std::fmax(g, b));
  if (!hi) return V();
  V color = V::object();
  color["r"] = int(std::lround(rgbGamma(r / hi) * 255));
  color["g"] = int(std::lround(rgbGamma(g / hi) * 255));
  color["b"] = int(std::lround(rgbGamma(b / hi) * 255));
  return color;
}
/** @brief Encode effective application state as standard attributes. @param s Mapping. @param f Definition. @param value Effective state. @return Fixed-size list without retained JSON copies. */
inline Values encode(const Spec& s, const V& f, const V& value) {
  Values out;
  if (s.part != 0xffff) {
    if (s.kind == Kind::State) {
      const V& state = value.type() == V::Type::Object ? value["state"] : value;
      V ordinal = stateOption(f, state.asString());
      if (!ordinal.isNull()) integer(out, 0x12, 0x55, 0x21, ordinal.asDouble());
      integer(out, 0x12, 0x6f, 0x18, ordinal.isNull() ? 2 : 0);
      return out;
    }
    const V& c = f["controls"][s.part];
    String key = c["key"].asString();
    const V& v = key.length() ? value[key] : value;
    if (s.kind == Kind::Action) {
      integer(out, 6, 0, 0x10, 0);
      return out;
    }
    if (s.kind == Kind::Switch) {
      integer(out, 6, 0, 0x10, c.hasOwnProperty("on") ? v.asString() == c["on"].asString() : v.asBool());
      return out;
    }
    if (s.kind == Kind::Slider) {
      number(out, 0xd, v.asDouble());
      return out;
    }
    if (s.kind == Kind::Select) {
      for (size_t i = 0; i < c["options"].length(); ++i)
        if (c["options"][i].asString() == v.asString()) {
          integer(out, 0x13, 0x55, 0x21, i + 1);
          return out;
        }
      return out;
    }
    // Read-only measurements use a projected value and native encoding below.
    Spec scalar = s;
    scalar.part = 0xffff;
    return encode(scalar, f, v);
  }
  switch (s.kind) {
    case Kind::Switch: integer(out, 6, 0, 0x10, value.asBool()); break;
    case Kind::Slider: number(out, 0x0d, value.asDouble()); break;
    case Kind::Number:
      {
        double n;
        bool valid = value.toDouble(n) && std::isfinite(n) && std::isfinite(float(n));
        if (valid) number(out, 0x0c, n);
        integer(out, 0x0c, 0x6f, 0x18, valid ? 0 : 2);
        break;
      }
    case Kind::Temperature: integer(out, 0x402, 0, 0x29, temperature(value)); break;
    case Kind::Humidity:
      {
        double n;
        integer(out, 0x405, 0, 0x21, value.toDouble(n) && std::isfinite(n) && n >= 0 && n <= 100 ? std::lround(n * 100) : 0xffff);
        break;
      }
    case Kind::Pressure:
      {
        double n;
        integer(out, 0x403, 0, 0x29, value.toDouble(n) && std::isfinite(n) && n >= -32767 && n <= 32767 ? uint16_t(int16_t(std::lround(n))) : 0x8000);
        break;
      }
    case Kind::Illuminance:
      {
        double n;
        uint16_t wire = 0xffff;
        if (value.toDouble(n) && std::isfinite(n) && n >= 0) {
          double encoded = n == 0 ? 0 : 10000 * std::log10(n) + 1;
          if (encoded >= 0 && encoded <= 65534) wire = std::lround(encoded);
        }
        integer(out, 0x400, 0, 0x21, wire);
        break;
      }
    case Kind::Flow: integer(out, 0x404, 0, 0x21, unsignedMeasurement(value, 10)); break;
    case Kind::WindSpeed: integer(out, 0x40b, 0, 0x21, unsignedMeasurement(value, 100)); break;
    case Kind::CarbonDioxide: measurement(out, 0x40d, value, 0.000001); break;
    case Kind::PM25: measurement(out, 0x42a, value); break;
    case Kind::Voltage:
    case Kind::Current:
    case Kind::Power:
      {
        bool power = s.kind == Kind::Power;
        uint16_t base = s.kind == Kind::Voltage ? 0x600 : s.kind == Kind::Current ? 0x602
                                                                                  : 0x604;
        integer(out, 0xb04, 0, 0x1b, power ? 9 : 8);
        integer(out, 0xb04, base, 0x21, 1);
        integer(out, 0xb04, base + 1, 0x21, power ? 1 : s.kind == Kind::Voltage ? 10
                                                                                : 1000);
        double n;
        uint16_t wire = power ? (value.toDouble(n) && std::isfinite(n) && n >= -32767 && n <= 32767 ? uint16_t(int16_t(std::lround(n))) : 0x8000) : unsignedMeasurement(value, s.kind == Kind::Voltage ? 10 : 1000);
        integer(out, 0xb04, s.kind == Kind::Voltage ? 0x505 : s.kind == Kind::Current ? 0x508
                                                                                      : 0x50b,
                power ? 0x29 : 0x21, wire);
        break;
      }
    case Kind::Select:
      for (size_t i = 0; i < f["options"].length(); ++i)
        if (f["options"][i]["value"].asString() == value.asString()) {
          integer(out, 0x13, 0x55, 0x21, i + 1);
          break;
        }
      break;
    case Kind::Binary: integer(out, 0xf, 0x55, 0x10, value.asBool()); break;
    case Kind::Light:
      {
        uint8_t h, sat;
        rgbToHs(value["color"], h, sat);
        integer(out, 6, 0, 0x10, value["state"].asString() == "ON");
        integer(out, 8, 0, 0x20, std::lround(value["brightness"].asDouble() * 254 / 255 * std::fmax(value["color"]["r"].asDouble(), std::fmax(value["color"]["g"].asDouble(), value["color"]["b"].asDouble())) / 255));
        integer(out, 0x300, 0, 0x20, h);
        integer(out, 0x300, 1, 0x20, sat);
        integer(out, 0x300, 8, 0x30, 0);
        integer(out, 0x300, 0x4000, 0x21, std::lround(h * 65535.0 / 254));
        integer(out, 0x300, 0x4001, 0x30, 0);
        uint16_t x, y;
        rgbToXy(value["color"], x, y);
        integer(out, 0x300, 3, 0x21, x);
        integer(out, 0x300, 4, 0x21, y);
        break;
      }
    case Kind::Climate:
      {
        integer(out, 0x201, 0, 0x29, temperature(value["current_temperature"]));
        double target = value["temperature"].asDouble();
        bool cooling = value["mode"].asString() == "cool";
        double band = s.sequence == 4 ? 2.5 : 0;
        integer(out, 0x201, 0x11, 0x29, temperature(value["mode"].asString() == "heat_cool" && value.hasOwnProperty("temperature_high") ? value["temperature_high"].asDouble() : (cooling ? target : (band ? s.max + band : target))));
        integer(out, 0x201, 0x12, 0x29, temperature(value["mode"].asString() == "heat_cool" && value.hasOwnProperty("temperature_low") ? value["temperature_low"].asDouble() : (cooling ? (band ? s.min - band : target) : target)));
        uint8_t m = mode(value["mode"].asString());
        integer(out, 0x201, 0x1c, 0x30, m == 255 ? 0 : m);
        String action = value["action"].asString();
        integer(out, 0x201, 0x29, 0x19, action == "heating" ? 1 : action == "cooling" ? 2
                                                                                      : 0);
        if (s.fan) {
          m = mode(value["fan_mode"].asString(), true);
          integer(out, 0x202, 0, 0x30, m == 255 ? 0 : m);
        }
        break;
      }
    case Kind::Cover:
      integer(out, 0x102, 8, 0x20, 100 - value["position"].asDouble());
      integer(out, 0x102, 9, 0x20, 100 - value["tilt"].asDouble());
      break;
    case Kind::Lock:
      {
        String state = value.asString();
        integer(out, 0x101, 0, 0x30, state == "LOCKED" ? 1 : state == "UNLOCKED" || state == "OPEN" ? 2
                                                                                                    : 0);
        break;
      }
    default: break;
  }
  return out;
}
/** @brief Decode an incoming command without executing application code. @param s Mapping. @param f Definition. @param current Current state. @param c Incoming command. @param next Output state/control value. @param action Output action payload or empty. @param property Output controlled property. @return True for a supported command and valid wire value. */
inline bool decode(const Spec& s, const V& f, const V& current, const Command& c, V& next, String& action, String& property) {
  action = String();
  property = String();
  if (s.part != 0xffff) {
    if (s.kind == Kind::State || s.part >= f["controls"].length()) return false;
    const V& control = f["controls"][s.part];
    property = control["key"].asString();
    if (s.kind == Kind::Action) {
      if (c.kind || c.cluster != 6 || c.id || c.value != 1) return false;
      action = control["payload"].asString();
      return action.length();
    }
    if (s.kind == Kind::Switch) {
      if (c.kind || c.cluster != 6 || c.id || c.value > 1) return false;
      next = control.hasOwnProperty("on") ? control[c.value ? "on" : "off"] : V(bool(c.value));
      return true;
    }
    if (s.kind == Kind::Slider) {
      if (c.kind || c.cluster != 0xd || c.id != 0x55) return false;
      float n;
      memcpy(&n, &c.value, 4);
      double lo = control["min"].asDouble(), hi = control["max"].asDouble(), step = control.hasOwnProperty("step") ? control["step"].asDouble() : 1;
      if (!std::isfinite(n) || n < lo || n > hi || step <= 0) return false;
      double snapped = lo + std::round((n - lo) / step) * step;
      if (std::fabs(n - snapped) > std::fmax(1e-5, std::fabs(n) * 1e-7)) return false;
      next = snapped;
      return true;
    }
    if (s.kind == Kind::Select) {
      if (c.kind || c.cluster != 0x13 || c.id != 0x55 || !c.value || c.value > control["options"].length()) return false;
      next = control["options"][size_t(c.value - 1)];
      return true;
    }
    return false;
  }
  if (c.kind == 1) {
    if (s.kind == Kind::Lock) {
      if (c.id > 1) return false;
      action = c.id == 0 ? "LOCK" : "UNLOCK";
      return true;
    }
    if (s.kind == Kind::Cover) {
      if (c.id < 3) {
        action = c.id == 0 ? "OPEN" : c.id == 1 ? "CLOSE"
                                                : "STOP";
        return true;
      }
      if ((c.id == 5 || c.id == 8) && c.value <= 100) {
        property = c.id == 5 ? "position" : "tilt";
        next = 100 - int(c.value);
        return true;
      }
      return false;
    }
    if (s.kind == Kind::Siren) {
      if (c.id > 6 || c.volume > 3) return false;
      next = current;
      next["state"] = c.id ? "ON" : "OFF";
      next["tone"] = "alarm";
      next["duration"] = int(std::fmax(1, std::fmin(300, c.duration)));
      next["volume_level"] = std::round(c.volume * 10.0 / 3) / 10;
      return true;
    }
    return false;
  }
  if (s.kind == Kind::Switch && c.cluster == 6 && c.id == 0 && c.value <= 1) {
    next = bool(c.value);
    return true;
  }
  if (s.kind == Kind::Slider && c.cluster == 0xd && c.id == 0x55) {
    float n;
    std::memcpy(&n, &c.value, 4);
    double min = f["min"].asDouble(), max = f["max"].asDouble(), step = f.hasOwnProperty("step") ? f["step"].asDouble() : 1;
    if (!std::isfinite(n) || n < min - 1e-5 || n > max + 1e-5) return false;
    double snapped = min + std::round((n - min) / step) * step;
    if (std::fabs(n - snapped) > std::fmax(1e-5, std::fabs(n) * 1e-7)) return false;
    next = snapped;
    return true;
  }
  if (s.kind == Kind::Select && c.cluster == 0x13 && c.id == 0x55 && c.value && c.value <= s.states) {
    next = f["options"][size_t(c.value - 1)]["value"];
    return true;
  }
  if (s.kind == Kind::Climate) {
    next = current;
    if (c.cluster == 0x201 && (c.id == 0x11 || c.id == 0x12)) {
      double n = int16_t(c.value) / 100.0;
      bool range = false;
      for (size_t i = 0; i < f["modes"].length(); ++i) range |= f["modes"][i].asString() == "heat_cool";
      if (range && current["mode"].asString() == "heat_cool") {
        next[c.id == 0x11 ? "temperature_high" : "temperature_low"] = n;
        double step = s.step;
        return n >= s.min && n <= s.max && std::fabs((n - s.min) / step - std::round((n - s.min) / step)) < 0.000001 && next["temperature_high"].asDouble() - next["temperature_low"].asDouble() >= 2.5;
      }
      bool cooling = current["mode"].asString() == "cool";
      if (s.sequence == 4) {
        if (c.id == 0x11 && !cooling) n -= 2.5;
        else if (c.id == 0x12 && cooling) n += 2.5;
      }
      double min = f["min"].asDouble(), max = f["max"].asDouble(), step = f.hasOwnProperty("step") ? f["step"].asDouble() : 0.5;
      next["temperature"] = min + std::round((n - min) / step) * step;
      return n >= min && n <= max;
    }
    if ((c.cluster == 0x201 && c.id == 0x1c) || (c.cluster == 0x202 && c.id == 0 && s.fan)) {
      bool fan = c.cluster == 0x202;
      const V& options = f[fan ? "fan_modes" : "modes"];
      if (!fan && c.value == 1)
        for (size_t i = 0; i < options.length(); ++i)
          if (options[i].asString() == "heat_cool") {
            next["mode"] = "heat_cool";
            if (next["temperature_low"].isUndefined()) next["temperature_low"] = f["min"];
            if (next["temperature_high"].isUndefined()) next["temperature_high"] = f["max"];
            return true;
          }
      for (size_t i = 0; i < options.length(); ++i)
        if (mode(options[i].asString(), fan) == c.value) {
          next[fan ? "fan_mode" : "mode"] = options[i];
          return true;
        }
    }
    return false;
  }
  if (s.kind == Kind::Light) {
    if (c.cluster == 6 && c.id == 0 && c.value <= 1) {
      property = "state";
      next = c.value ? "ON" : "OFF";
      return true;
    }
    if (c.cluster == 8 && c.id == 0 && c.value <= 254) {
      uint8_t h, sat;
      rgbToHs(current["color"], h, sat);
      next = current;
      next["color"] = hsToRgb(h, sat);
      next["brightness"] = int(std::lround(c.value * 255.0 / 254));
      return true;
    }
    if (c.cluster == 0x300 && (c.id == 3 || c.id == 4)) {
      V color = xyToRgb(c.x, c.y);
      if (!color.isValid() || color.isUndefined()) return false;
      next = current;
      next["color"] = color;
      next["brightness"] = int(std::lround(c.level * 255.0 / 254));
      return true;
    }
    if (c.cluster == 0x300 && (c.id == 0 || c.id == 1) && c.value <= 254) {
      uint8_t h = c.hue, sat = c.saturation;
      next = current;
      next["color"] = hsToRgb(h, sat);
      next["brightness"] = int(std::lround(c.level * 255.0 / 254));
      return true;
    }
  }
  return false;
}
}
