// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ArdJSON.h"
#include "HaEntityData.h"

namespace ArdHa {
using V=ArdJSON::JSONVar;
/**
 * @brief Build a normalized Home Assistant entity descriptor.
 * @param type JSON, control or protocol type being examined.
 * @param stateOnly Whether to use only runtime state rather than persisted configuration.
 * @return A normalized Home Assistant entity descriptor.
 */
inline V descriptor(const String& type,bool stateOnly=false) {
#if ARDPORTAL_CONTROL_SUPPORT_EXTENDED
  for(size_t i=0;i<ARD_HA_SPEC_COUNT;++i) {
    ArdHaSpec spec; memcpy_P(&spec,&ARD_HA_SPECS[i],sizeof(spec));
    if(strcmp_P(type.c_str(),spec.name)==0) return ArdJSON::JSON.parse(String(FPSTR(stateOnly?spec.state:spec.json)));
  }
#else
  (void)type;(void)stateOnly;
#endif
  return V();
}
/**
 * @brief Resolve the Home Assistant component associated with a field.
 * @param field Application field definition or identifier.
 * @return The resulting text; an empty value indicates no available text or failure where applicable.
 */
inline String component(const V& field) {
  String type=field["type"].asString();
#if ARDPORTAL_CONTROL_SUPPORT_SLIDER
  if(type=="slider") return "number";
#endif
#if ARDPORTAL_ENABLE_CONTROL_TEXT
  if(type=="text") return "sensor";
#endif
#if ARDPORTAL_CONTROL_SUPPORT_SWITCH
  if(type=="switch") return "switch";
#endif
#if ARDPORTAL_ENABLE_CONTROL_EDIT
  if(type=="edit") return "text";
#endif
#if ARDPORTAL_ENABLE_CONTROL_DEVICE_TRIGGER
  if(type=="device_trigger") return "device_automation";
#endif
  return type;
}
/**
 * @brief Check the normalized extended-control flag.
 * @param field Application field definition or identifier.
 * @return True if the field extended flag is set.
 */
inline bool extended(const V& field) { return field["extended"].asBool(); }
/**
 * @brief Check whether the field uses a transient event instead of retained state.
 * @param field Application field definition or identifier.
 * @return True if the field uses a transient event instead of retained state; false otherwise.
 */
inline bool transient(const V& field) {String type=field["type"].asString();return false
#if ARDPORTAL_ENABLE_CONTROL_EVENT
  || type=="event"
#endif
#if ARDPORTAL_ENABLE_CONTROL_DEVICE_TRIGGER
  || type=="device_trigger"
#endif
#if ARDPORTAL_ENABLE_CONTROL_TAG
  || type=="tag"
#endif
#if ARDPORTAL_ENABLE_CONTROL_BUTTON
  || type=="button"
#endif
#if ARDPORTAL_ENABLE_CONTROL_SCENE
  || type=="scene"
#endif
#if ARDPORTAL_ENABLE_CONTROL_NOTIFY
  || type=="notify"
#endif
#if ARDPORTAL_ENABLE_CONTROL_INFRARED
  || type=="infrared"
#endif
  ;}

/**
 * @brief Check whether the field rejects user changes.
 * @param field Application field definition or identifier.
 * @return True when the field is read-only; false otherwise.
 */
inline bool readonly(const V& field) {String t=field["type"].asString();return false
  || field["readonly"].asBool()
#if ARDPORTAL_ENABLE_CONTROL_TEXT
  || t=="text"
#endif
#if ARDPORTAL_ENABLE_CONTROL_BINARY_SENSOR
  || t=="binary_sensor"
#endif
#if ARDPORTAL_ENABLE_CONTROL_EVENT
  || t=="event"
#endif
#if ARDPORTAL_ENABLE_CONTROL_DEVICE_TRACKER
  || t=="device_tracker"
#endif
#if ARDPORTAL_ENABLE_CONTROL_DEVICE_TRIGGER
  || t=="device_trigger"
#endif
#if ARDPORTAL_ENABLE_CONTROL_TAG
  || t=="tag"
#endif
  ;}

/**
 * @brief Validate a bounded JSON string without embedded NUL characters.
 * @param value Input value, or output destination when passed by mutable reference.
 * @param max Maximum allowed value.
 * @return True for a string within the byte limit and without NULs; false otherwise.
 */
inline bool string(const V& value,size_t max=512) { String s=value.asString(); return value.type()==V::Type::String && s.length()<=max && strlen(s.c_str())==s.length(); }
#if ARDPORTAL_ENABLE_CONTROL_DATE || ARDPORTAL_ENABLE_CONTROL_TIME || ARDPORTAL_ENABLE_CONTROL_DATETIME
/**
 * @brief Decode a decimal digit range into an unsigned value.
 * @param text Text to read, encode or display.
 * @param count Number of items or bytes to process.
 * @return The decoded value; callers must validate the digits first.
 */
inline unsigned decimalDigits(const char* text, size_t count) {
  unsigned value=0; for(size_t i=0;i<count;++i)value=value*10+unsigned(text[i]-'0'); return value;
}
/**
 * @brief Validate YYYY-MM-DD syntax and calendar ranges, including leap years.
 * @param text Text to read, encode or display.
 * @param length Number of bytes or elements to process.
 * @return True for a valid calendar date; false otherwise.
 */
inline bool dateParts(const char* text, size_t length) {
  if(length!=10||text[4]!='-'||text[7]!='-') return false;
  for(size_t i=0;i<10;++i) if(i!=4&&i!=7&&(text[i]<'0'||text[i]>'9')) return false;
  unsigned y=decimalDigits(text,4),m=decimalDigits(text+5,2),d=decimalDigits(text+8,2);
  static const uint8_t days[] PROGMEM={31,28,31,30,31,30,31,31,30,31,30,31};
  return y>=1&&m>=1&&m<=12&&d>=1&&d<=unsigned(pgm_read_byte(days+m-1)+(m==2&&y%4==0&&(y%100!=0||y%400==0)));
}
/**
 * @brief Validate or format a calendar date value.
 * @param text Text to read, encode or display.
 * @return True on success; false if validation, resource allocation or the operation fails.
 */
inline bool date(const String& text) { return dateParts(text.c_str(),text.length()); }
/**
 * @brief Validate HH:MM or HH:MM:SS syntax and time ranges.
 * @param text Text to read, encode or display.
 * @param length Number of bytes or elements to process.
 * @return True for a valid clock value; false otherwise.
 */
inline bool clockParts(const char* text, size_t length) {
  if((length!=5&&length!=8)||text[2]!=':'||(length==8&&text[5]!=':'))return false;
  for(size_t i=0;i<length;++i) if(i!=2&&i!=5&&(text[i]<'0'||text[i]>'9'))return false;
  return decimalDigits(text,2)<24&&decimalDigits(text+3,2)<60&&(length==5||decimalDigits(text+6,2)<60);
}
/**
 * @brief Validate the time control value and its range.
 * @param text Text to read, encode or display.
 * @return True on success; false if validation, resource allocation or the operation fails.
 */
inline bool time(const String& text) { return text.length()==8&&clockParts(text.c_str(),8); }
#endif
#if ARDPORTAL_CONTROL_SUPPORT_EXTENDED
/**
 * @brief Validate a proposed value against its extended control definition.
 * @param c Control definition specifying type and allowed values.
 * @param value Input value, or output destination when passed by mutable reference.
 * @return True if the value satisfies the control type, range and options; false otherwise.
 */
inline bool validControl(const V& c,const V& value) {
  String type=c["type"].asString();
#if ARDPORTAL_CONTROL_SUPPORT_SLIDER
  if(type=="slider") { double n; if(!value.toDouble(n)||n<c["min"].asDouble()||n>c["max"].asDouble()) return false; double step=c["step"].isUndefined()?1:c["step"].asDouble(); return step>0&&fabs((n-c["min"].asDouble())/step-round((n-c["min"].asDouble())/step))<0.000001; }
#endif
#if ARDPORTAL_CONTROL_SUPPORT_SWITCH
  if(type=="switch") return c.hasOwnProperty("on") ? value.type()==V::Type::String&&(value.asString()==c["on"].asString()||value.asString()==c["off"].asString()) : value.type()==V::Type::Boolean;
#endif
#if ARDPORTAL_CONTROL_SUPPORT_SELECT
  if(type=="select") { for(size_t i=0;i<c["options"].length();++i) if(value.type()==V::Type::String&&c["options"][i].asString()==value.asString()) return true; return false; }
#endif
#if ARDPORTAL_ENABLE_CONTROL_LIGHT
  if(type=="color") { if(value.type()!=V::Type::Object||value.length()!=3) return false; for(const char* key:{"r","g","b"}) { int64_t n; if(!value[key].toInteger(n)||n<0||n>255) return false; } return true; }
#endif
#if ARDPORTAL_ENABLE_CONTROL_DATE
  if(type=="date") return string(value)&&date(value.asString());
#endif
#if ARDPORTAL_ENABLE_CONTROL_TIME
  if(type=="time") return string(value)&&time(value.asString());
#endif
#if ARDPORTAL_ENABLE_CONTROL_DATETIME
  if(type=="datetime") { String s=value.asString(); if(!string(value)||s.length()<20||s[10]!='T'||!dateParts(s.c_str(),10)||!clockParts(s.c_str()+11,8)) return false;
    size_t end=19;if(s[end]=='.') {++end;size_t start=end;while(end<s.length()&&s[end]>='0'&&s[end]<='9') ++end;if(end==start||end-start>6) return false;}
    if(end+1==s.length()&&s[end]=='Z') return true;
    return end+6==s.length()&&(s[end]=='+'||s[end]=='-')&&s[end+3]==':'&&clockParts(s.c_str()+end+1,5); }
#endif
  return (type=="edit"||type=="action_text")&&string(value);
}
/**
 * @brief Validate a value against the application control type and constraints.
 * @param f Field definition to inspect or normalize.
 * @param value Input value, or output destination when passed by mutable reference.
 * @return True if the supplied value satisfies the field constraints.
 */
inline bool validValue(const V& f,const V& value) {
  String type=f["type"].asString();
  if(f["action_only"].asBool()) return value.isNull();
#if ARDPORTAL_ENABLE_CONTROL_BINARY_SENSOR
  if(type=="binary_sensor") return value.type()==V::Type::Boolean;
#endif
#if ARDPORTAL_ENABLE_CONTROL_EVENT
  if(type=="event") { if(value.type()!=V::Type::Object) return false; for(size_t i=0;i<f["ha"]["event_types"].length();++i) if(value["event_type"].asString()==f["ha"]["event_types"][i].asString()) return true; return false; }
#endif
  if(type=="tag"||type=="device_trigger"||type=="device_tracker") return string(value);
  const V& initial=f["default"];
  if(initial.type()==V::Type::Object) {
    if(value.type()!=V::Type::Object) return false;
    V required=initial.keys(); for(size_t i=0;i<required.length();++i) if(!value.hasOwnProperty(required[i].asString())) return false;
    const V& controls=f["controls"]; for(size_t i=0;i<controls.length();++i) if(controls[i]["type"].asString().indexOf("action")!=0&&!validControl(controls[i],value[controls[i]["key"].asString()])) return false;
    // Bound additional telemetry while allowing standard HA attributes.
    ArdJSON::Limits limits;limits.maxOutputBytes=1024;return ArdJSON::JSON.measure(value,nullptr,limits)>0;
  }
  if(f["controls"].length()&&f["controls"][0]["type"].asString().indexOf("action")!=0) return validControl(f["controls"][0],value);
  return string(value);
}
/**
 * @brief Normalize a field definition and merge its control or Home Assistant defaults.
 * @param field Application field definition or identifier.
 * @param stateOnly Whether to use only runtime state rather than persisted configuration.
 * @param error Output error text; populated when the operation fails.
 * @return True on success; false if validation, resource allocation or the operation fails.
 */
inline bool normalize(V& field,bool stateOnly=false,String* error=nullptr) {
  auto fail=[&](const String& reason){if(error)*error=reason;return false;};
  if(error)*error=String();
  V spec=descriptor(field["type"].asString(),stateOnly); if(spec.isUndefined()) return fail("unsupported/disabled descriptor type: "+field["type"].asString());
  V supplied=std::move(field["ha"]);
  V keys=spec.keys();for(size_t i=0;i<keys.length();++i) { String key=keys[i].asString(); if(key!="ha"&&!field.hasOwnProperty(key)) field[key]=std::move(spec[key]); }
  V ha=std::move(spec["ha"]); if(!supplied.isUndefined()) { if(supplied.type()!=V::Type::Object) return fail("ha must be an object"); V options=supplied.keys();for(size_t i=0;i<options.length();++i) ha[options[i].asString()]=std::move(supplied[options[i].asString()]); }
  field["ha"]=std::move(ha); field["extended"]=true; if(!field.hasOwnProperty("persist")) field["persist"]=!readonly(field)&&!transient(field);
  for(const char* key:{"unit_of_measurement","device_class","state_class","entity_category"}) if(field.hasOwnProperty(key)) field["ha"][key]=field[key];
  if(field["controls"].type()!=V::Type::Array) return fail("controls must be an array");
  for(size_t i=0;i<field["controls"].length();++i) {
    const V& c=static_cast<const V&>(field)["controls"][i];String t=c["type"].asString();
    bool supported=false;
#if ARDPORTAL_CONTROL_SUPPORT_SLIDER
    supported=supported||t=="slider";
#endif
#if ARDPORTAL_CONTROL_SUPPORT_SWITCH
    supported=supported||t=="switch";
#endif
#if ARDPORTAL_CONTROL_SUPPORT_SELECT
    supported=supported||t=="select";
#endif
#if ARDPORTAL_ENABLE_CONTROL_LIGHT
    supported=supported||t=="color";
#endif
#if ARDPORTAL_ENABLE_CONTROL_DATE
    supported=supported||t=="date";
#endif
#if ARDPORTAL_ENABLE_CONTROL_TIME
    supported=supported||t=="time";
#endif
#if ARDPORTAL_ENABLE_CONTROL_DATETIME
    supported=supported||t=="datetime";
#endif
#if ARDPORTAL_CONTROL_SUPPORT_ACTIONS
    supported=supported||t=="action";
#endif
#if ARDPORTAL_ENABLE_CONTROL_NOTIFY
    supported=supported||t=="action_text";
#endif
#if ARDPORTAL_ENABLE_CONTROL_INFRARED || ARDPORTAL_ENABLE_CONTROL_VACUUM
    supported=supported||t=="action_json";
#endif
#if ARDPORTAL_CONTROL_SUPPORT_EXTENDED
    supported=supported||t=="edit";
#endif
    if(!supported) return fail("unsupported/disabled control type at controls["+String(i)+"]: "+t);
    if(!c["command"].isUndefined()&&!string(c["command"],24)) return fail("invalid command at controls["+String(i)+"] (maximum 24 bytes)");
    if(t=="slider"&&(!c["min"].isValid()||c["min"].asDouble()>=c["max"].asDouble()||c["step"].asDouble()<=0)) return fail("invalid slider min/max/step at controls["+String(i)+"]");
  }
  return validValue(field,field["default"]) || fail("invalid descriptor default value");
}
#else
/**
 * @brief Normalize a field definition and merge its control or Home Assistant defaults.
 * Input: V&.
 * Input: bool.
 * @param error Output error text; populated when the operation fails.
 * @return True on success; false if validation, resource allocation or the operation fails.
 */
inline bool normalize(V&,bool=false,String* error=nullptr) {if(error)*error="extended controls are disabled";return false;}
/**
 * @brief Validate a proposed value against its extended control definition.
 * Input: const V&.
 * Input: const V&.
 * @return True if the value satisfies the control type, range and options; false otherwise.
 */
inline bool validControl(const V&,const V&) {return false;}
/**
 * @brief Validate a value against the application control type and constraints.
 * Input: const V&.
 * Input: const V&.
 * @return True if the supplied value satisfies the field constraints.
 */
inline bool validValue(const V&,const V&) {return false;}
#endif
}
