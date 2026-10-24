// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "LanguageLiterals.h"
#include "ArdJSON.h"
#include "ControlData.h"
#include "ControlClasses.h"

namespace ArdControl {
using V = ArdJSON::JSONVar;
/**
 * @brief Build a normalized application control descriptor.
 * @param type JSON, control or protocol type being examined.
 * @return A normalized application control descriptor.
 */
inline V descriptor(const String& type) {
#if ARDPORTAL_CONTROL_SUPPORT_EXTENDED
  const uint8_t id = uint8_t(ArdControl::typeOf(type));
  for (size_t i = 0; i < ARD_CONTROL_SPEC_COUNT; ++i) {
    if (pgm_read_byte(ARD_CONTROL_SPEC_TYPES + i) != id) continue;
    const char* json;
    memcpy_P(&json, ARD_CONTROL_SPECS + i, sizeof(json));
    return ArdJSON::JSON.parse(String(FPSTR(json)));
  }
#else
  (void)type;
#endif
  return V();
}
/**
 * @brief Check the normalized extended-control flag.
 * @param field Application field definition or identifier.
 * @return True if the field extended flag is set.
 */
inline bool extended(const V& field) {
  return field["extended"].asBool();
}
/**
 * @brief Check whether the field uses a transient event instead of retained state.
 * @param field Application field definition or identifier.
 * @return True if the field uses a transient event instead of retained state; false otherwise.
 */
inline bool transient(const V& field) {
  String type = field["type"].asString();
  return false
#if ARDPORTAL_ENABLE_CONTROL_EVENT
         || type == "event"
#endif
#if ARDPORTAL_ENABLE_CONTROL_DEVICE_TRIGGER
         || type == "device_trigger"
#endif
#if ARDPORTAL_ENABLE_CONTROL_TAG
         || type == "tag"
#endif
#if ARDPORTAL_ENABLE_CONTROL_BUTTON
         || type == "button"
#endif
#if ARDPORTAL_ENABLE_CONTROL_SCENE
         || type == "scene"
#endif
#if ARDPORTAL_ENABLE_CONTROL_NOTIFY
         || type == "notify"
#endif
#if ARDPORTAL_ENABLE_CONTROL_INFRARED
         || type == "infrared"
#endif
    ;
}

/**
 * @brief Check whether the field rejects user changes.
 * @param field Application field definition or identifier.
 * @return True when the field is read-only; false otherwise.
 */
inline bool readonly(const V& field) {
  String t = field["type"].asString();
  return false
         || field["readonly"].asBool()
#if ARDPORTAL_ENABLE_CONTROL_TEXT
         || t == "text"
#endif
#if ARDPORTAL_ENABLE_CONTROL_BINARY_SENSOR
         || t == "binary_sensor"
#endif
#if ARDPORTAL_ENABLE_CONTROL_EVENT
         || t == "event"
#endif
#if ARDPORTAL_ENABLE_CONTROL_DEVICE_TRACKER
         || t == "device_tracker"
#endif
#if ARDPORTAL_ENABLE_CONTROL_DEVICE_TRIGGER
         || t == "device_trigger"
#endif
#if ARDPORTAL_ENABLE_CONTROL_TAG
         || t == "tag"
#endif
    ;
}

/**
 * @brief Validate a bounded JSON string without embedded NUL characters.
 * @param value Input value, or output destination when passed by mutable reference.
 * @param max Maximum allowed value.
 * @return True for a string within the byte limit and without NULs; false otherwise.
 */
inline bool string(const V& value, size_t max = 512) {
  const String* s = value.stringValue();
  return s && s->length() <= max && strlen(s->c_str()) == s->length();
}
#if ARDPORTAL_CONTROL_SUPPORT_EXTENDED
/**
 * @brief Validate a proposed value against its extended control definition.
 * @param c Control definition specifying type and allowed values.
 * @param value Input value, or output destination when passed by mutable reference.
 * @return True if the value satisfies the control type, range and options; false otherwise.
 */
inline bool validControl(const V& c, const V& value) {
  String type = c["type"].asString();
#if ARDPORTAL_CONTROL_SUPPORT_SLIDER
  if (type == "slider") {
    double n;
    if (!value.toDouble(n) || n < c["min"].asDouble() || n > c["max"].asDouble()) return false;
    double step = c["step"].isUndefined() ? 1 : c["step"].asDouble();
    return step > 0 && fabs((n - c["min"].asDouble()) / step - round((n - c["min"].asDouble()) / step)) < 0.000001;
  }
#endif
#if ARDPORTAL_CONTROL_SUPPORT_SWITCH
  if (type == "switch") return c.hasOwnProperty("on") ? value.type() == V::Type::String && (value.asString() == c["on"].asString() || value.asString() == c["off"].asString()) : value.type() == V::Type::Boolean;
#endif
#if ARDPORTAL_CONTROL_SUPPORT_SELECT
  if (type == "select") {
    for (size_t i = 0; i < c["options"].length(); ++i)
      if (value.type() == V::Type::String && c["options"][i].asString() == value.asString()) return true;
    return false;
  }
#endif
#if ARDPORTAL_ENABLE_CONTROL_LIGHT
  if (type == "color") {
    if (value.type() != V::Type::Object || value.length() != 3) return false;
    for (const char* key : { "r", "g", "b" }) {
      int64_t n;
      if (!value[key].toInteger(n) || n < 0 || n > 255) return false;
    }
    return true;
  }
#endif
#if ARDPORTAL_ENABLE_CONTROL_DATE
  if (type == "date") return string(value) && date(value.asString());
#endif
#if ARDPORTAL_ENABLE_CONTROL_TIME
  if (type == "time") return string(value) && time(value.asString());
#endif
#if ARDPORTAL_ENABLE_CONTROL_DATETIME
  if (type == "datetime") {
    return string(value) && datetime(value.asString());
  }
#endif
  return false
#if ARDPORTAL_CONTROL_SUPPORT_EXTENDED
         || (type == "edit" && string(value))
#endif
#if ARDPORTAL_ENABLE_CONTROL_NOTIFY
         || (type == "action_text" && string(value))
#endif
    ;
}
/**
 * @brief Validate a value against the application control type and constraints.
 * @param f Field definition to inspect or normalize.
 * @param value Input value, or output destination when passed by mutable reference.
 * @return True if the supplied value satisfies the field constraints.
 */
inline bool validValue(const V& f, const V& value) {
  String type = f["type"].asString();
#if ARDPORTAL_ENABLE_CONTROL_INFRARED
  if (type == "infrared" && (classOf(f) == Class::Receiver || classOf(f) == Class::Transceiver)) {
    if (value.isNull()) return true;
    if (value.type() != V::Type::Object || value["timings"].type() != V::Type::Array || !value["timings"].length() || value["timings"].length() > 256) return false;
    for (size_t i = 0; i < value["timings"].length(); ++i) {
      int64_t timing;
      if (!value["timings"][i].toInteger(timing) || timing < INT32_MIN || timing > INT32_MAX || !timing) return false;
    }
    if (!value["modulation"].isUndefined()) {
      uint64_t modulation;
      if (!value["modulation"].toUnsignedInteger(modulation) || !modulation || modulation > UINT32_MAX) return false;
    }
    ArdJSON::Limits limits;
    limits.maxOutputBytes = 1500;
    return ArdJSON::JSON.measure(value, nullptr, limits) > 0;
  }
#endif
  if (f["action_only"].asBool()) return value.isNull();
#if ARDPORTAL_ENABLE_CONTROL_BINARY_SENSOR
  if (type == "binary_sensor") return value.type() == V::Type::Boolean;
#endif
#if ARDPORTAL_ENABLE_CONTROL_EVENT
  if (type == "event") {
    if (value.type() != V::Type::Object) return false;
    for (size_t i = 0; i < f["event_types"].length(); ++i)
      if (value["event_type"].asString() == f["event_types"][i].asString()) return true;
    return false;
  }
#endif
  if (type == "tag" || type == "device_trigger" || type == "device_tracker") return string(value);
  const V& initial = f["default"];
  if (initial.type() == V::Type::Object) {
    if (value.type() != V::Type::Object) return false;
    V required = initial.keys();
    for (size_t i = 0; i < required.length(); ++i)
      if (!value.hasOwnProperty(required[i].asString())) return false;
    const V& controls = f["controls"];
    for (size_t i = 0; i < controls.length(); ++i)
      if (controls[i]["type"].asString().indexOf("action") != 0 && !validControl(controls[i], value[controls[i]["key"].asString()])) return false;
    // Bound additional telemetry while allowing standard HA attributes.
    ArdJSON::Limits limits;
    limits.maxOutputBytes = 1024;
    return ArdJSON::JSON.measure(value, nullptr, limits) > 0;
  }
  if (f["controls"].length() && f["controls"][0]["type"].asString().indexOf("action") != 0) return validControl(f["controls"][0], value);
  return string(value);
}
/**
 * @brief Normalize a field definition and merge its protocol-independent control defaults.
 * @param field Application field definition or identifier.
 * @param error Output error text; populated when the operation fails.
 * @return True on success; false if validation, resource allocation or the operation fails.
 */
inline bool normalize(V& field, String* error = nullptr) {
  auto fail = [&](const String& reason) {
    if (error) *error = reason;
    return false;
  };
  if (error) *error = String();
  if (!validClass(field)) return fail(F("invalid class for control type"));
  const bool suppliedEventTypes = field.hasOwnProperty("event_types");
#if ARDPORTAL_ENABLE_CONTROL_EVENT || ARDPORTAL_ENABLE_CONTROL_DEVICE_TRIGGER
  const bool suppliedDefault = field.hasOwnProperty("default");
#endif
  V spec = descriptor(field["type"].asString());
  if (spec.isUndefined()) return fail(ARD_LANG_NATIVE_UNSUPPORTED_DISABLED_DESCRIPTOR_TYPE + field["type"].asString());
  V keys = spec.keys();
  for (size_t i = 0; i < keys.length(); ++i) {
    String key = keys[i].asString();
    if (!field.hasOwnProperty(key)) field[key] = std::move(spec[key]);
  }
#if ARDPORTAL_ENABLE_CONTROL_INFRARED
  if (field["type"].asString() == "infrared" && classOf(field) == Class::Receiver) {
    field["controls"] = V::array();
    field["readonly"] = true;
    field["action_only"] = false;
    field["event_only"] = true;
  }
#endif
#if ARDPORTAL_ENABLE_CONTROL_EVENT
  if (field["type"].asString() == "event" && !static_cast<const V&>(field)["class"].isUndefined()) {
    // Explicit user event definitions win; otherwise choose the class's event vocabulary.
    if ((classOf(field) == Class::Doorbell || classOf(field) == Class::Motion) && !suppliedEventTypes) {
      String event = classOf(field) == Class::Doorbell ? String(F("ring")) : String(F("motion"));
      field["event_types"] = V::array();
      field["event_types"].push(event);
      if (!suppliedDefault) field["default"]["event_type"] = event;
    }
  }
#else
  (void)suppliedEventTypes;
#endif
#if ARDPORTAL_ENABLE_CONTROL_DEVICE_TRIGGER
  if (field["type"].asString() == "device_trigger" && !suppliedDefault) {
    if (classOf(field) == Class::Doorbell) field["default"] = String(F("ring"));
    else if (classOf(field) == Class::Motion) field["default"] = String(F("motion"));
  }
#endif
  field["extended"] = true;
  if (!field.hasOwnProperty("persist")) field["persist"] = !readonly(field) && !transient(field);
#if ARDPORTAL_ENABLE_CONTROL_EVENT
  if (field["type"].asString() == "event") {
    if (field["event_types"].type() != V::Type::Array || !field["event_types"].length() || field["event_types"].length() > 32) return fail(F("event_types must contain 1-32 strings"));
    for (size_t i = 0; i < field["event_types"].length(); ++i)
      if (!string(field["event_types"][i], 64)) return fail(F("invalid event type"));
  }
#endif
  if (field["controls"].type() != V::Type::Array) return fail(ARD_LANG_NATIVE_CONTROLS_MUST_BE_AN_ARRAY);
  for (size_t i = 0; i < field["controls"].length(); ++i) {
    const V& c = static_cast<const V&>(field)["controls"][i];
    String t = c["type"].asString();
    bool supported = false;
#if ARDPORTAL_CONTROL_SUPPORT_SLIDER
    supported = supported || t == "slider";
#endif
#if ARDPORTAL_CONTROL_SUPPORT_SWITCH
    supported = supported || t == "switch";
#endif
#if ARDPORTAL_CONTROL_SUPPORT_SELECT
    supported = supported || t == "select";
#endif
#if ARDPORTAL_ENABLE_CONTROL_LIGHT
    supported = supported || t == "color";
#endif
#if ARDPORTAL_ENABLE_CONTROL_DATE
    supported = supported || t == "date";
#endif
#if ARDPORTAL_ENABLE_CONTROL_TIME
    supported = supported || t == "time";
#endif
#if ARDPORTAL_ENABLE_CONTROL_DATETIME
    supported = supported || t == "datetime";
#endif
#if ARDPORTAL_CONTROL_SUPPORT_ACTIONS
    supported = supported || t == "action";
#endif
#if ARDPORTAL_ENABLE_CONTROL_NOTIFY
    supported = supported || t == "action_text";
#endif
#if ARDPORTAL_ENABLE_CONTROL_INFRARED || ARDPORTAL_ENABLE_CONTROL_VACUUM
    supported = supported || t == "action_json";
#endif
#if ARDPORTAL_CONTROL_SUPPORT_EXTENDED
    supported = supported || t == "edit";
#endif
    if (c.hasOwnProperty("class") && !validClass(c)) return fail(F("invalid class in controls"));
    if (!supported) return fail(ARD_LANG_NATIVE_UNSUPPORTED_DISABLED_CONTROL_TYPE_AT_CONTROLS + String(i) + "]: " + t);
    if (!c["command"].isUndefined() && !string(c["command"], 24)) return fail(ARD_LANG_NATIVE_INVALID_COMMAND_AT_CONTROLS + String(i) + ARD_LANG_NATIVE_MAXIMUM_24_BYTES);
#if ARDPORTAL_CONTROL_SUPPORT_SLIDER
    if (t == "slider" && (!c["min"].isValid() || c["min"].asDouble() >= c["max"].asDouble() || c["step"].asDouble() <= 0)) return fail(ARD_LANG_NATIVE_INVALID_SLIDER_MIN_MAX_STEP_AT_CONTROLS + String(i) + "]");
#endif
  }
  return validValue(field, field["default"]) || fail(ARD_LANG_NATIVE_INVALID_DEFAULT_VALUE_FOR_CONTROL);
}
#else
/**
 * @brief Normalize a field definition and merge its protocol-independent control defaults.
 * Input: V&.
 * @param error Output error text; populated when the operation fails.
 * @return True on success; false if validation, resource allocation or the operation fails.
 */
inline bool normalize(V&, String* error = nullptr) {
  if (error) *error = ARD_LANG_NATIVE_EXTENDED_CONTROLS_ARE_DISABLED;
  return false;
}
/**
 * @brief Validate a proposed value against its extended control definition.
 * Input: const V&.
 * Input: const V&.
 * @return True if the value satisfies the control type, range and options; false otherwise.
 */
inline bool validControl(const V&, const V&) {
  return false;
}
/**
 * @brief Validate a value against the application control type and constraints.
 * Input: const V&.
 * Input: const V&.
 * @return True if the supplied value satisfies the field constraints.
 */
inline bool validValue(const V&, const V&) {
  return false;
}
#endif
}
