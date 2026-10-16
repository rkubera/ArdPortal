// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ArdJSON.h"
#ifndef ARDPORTAL_ENABLE_TEXT_TYPES
#define ARDPORTAL_ENABLE_TEXT_TYPES 1
#endif
#include <cmath>
namespace ArdText {
using V=ArdJSON::JSONVar;
enum class Type:uint8_t {Invalid,Text,Number,Temperature,Humidity,Pressure,Illuminance,Flow,CarbonDioxide,PM25,WindSpeed,Voltage,Current,Power};
struct Pattern {Type type;const char* unit;const char* device;};
/** @brief Resolve an optional text subtype and its HA pattern. @param f Field JSON. @return Invalid for an unknown/non-string subtype; Text when omitted. */
inline Pattern pattern(const V& f){
  const V& value=f["textType"];
  if(value.isUndefined())return {Type::Text,"",""};
  if(value.type()!=V::Type::String)return {Type::Invalid,"",""};
  const String& name=*value.stringValue();
  if(name=="text")return {Type::Text,"",""};
#if ARDPORTAL_ENABLE_TEXT_TYPES
  if(name=="number")return {Type::Number,"",""};
  if(name=="temperature")return {Type::Temperature,"°C","temperature"};
  if(name=="humidity")return {Type::Humidity,"%","humidity"};
  if(name=="pressure")return {Type::Pressure,"hPa","pressure"};
  if(name=="illuminance")return {Type::Illuminance,"lx","illuminance"};
  if(name=="flow")return {Type::Flow,"m³/h","volume_flow_rate"};
  if(name=="carbon_dioxide")return {Type::CarbonDioxide,"ppm","carbon_dioxide"};
  if(name=="pm25")return {Type::PM25,"µg/m³","pm25"};
  if(name=="wind_speed")return {Type::WindSpeed,"m/s","wind_speed"};
  if(name=="voltage")return {Type::Voltage,"V","voltage"};
  if(name=="current")return {Type::Current,"A","current"};
  if(name=="power")return {Type::Power,"W","power"};
#endif
  return {Type::Invalid,"",""};
}
/** @brief Validate plain text or a numeric reading. @param f Field JSON. @param value State; null denotes an unavailable measurement. @return True for a supported subtype and correctly typed value. */
inline bool validValue(const V& f,const V& value){
  auto p=pattern(f);double n;
  if(p.type==Type::Invalid)return false;
#if ARDPORTAL_ENABLE_TEXT_TYPES
  if(p.type!=Type::Text)return value.isNull()||(value.type()==V::Type::Number&&value.toDouble(n)&&std::isfinite(n)&&std::isfinite(float(n)));
#endif
  if(value.type()==V::Type::Number)return value.toDouble(n)&&std::isfinite(n);
  const String* s=value.stringValue();return s&&s->length()<=512&&strlen(s->c_str())==s->length();
}
/** @brief Populate automatic HA sensor defaults before explicit overrides. @param f Field definition. @param out Discovery JSON destination. @return No value; caller checks JSON allocation status. */
inline void haPattern(const V& f,V& out){
#if ARDPORTAL_ENABLE_TEXT_TYPES
  auto p=pattern(f);if(p.type==Type::Invalid||p.type==Type::Text)return;
  if(*p.unit)out["unit_of_measurement"]=p.unit;
  if(*p.device)out["device_class"]=p.device;
  out["state_class"]="measurement";
  out["value_template"]="{{ value if is_number(value) else 'None' }}";
#else
  (void)f;(void)out;
#endif
}
}
