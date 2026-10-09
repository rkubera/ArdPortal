// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ArdPortalDeclarations.h"
#include "ArdJSON.h"

// Configuration schema validation, independent of ArdFS. Parsing/encoding is generic.
namespace ArdPortalJson {
/**
 * @brief Build the JSON limits used for persistent configuration documents.
 * @return Configured JSON byte, node, string and depth limits.
 */
inline ArdJSON::Limits limits() {
  ArdJSON::Limits value;value.maxDepth=1;value.maxNodes=15;value.maxStringBytes=4096;return value;
}
struct TextField { const char* key; String ArdPortal::PortalConfig::*member; uint16_t limit; bool required; };
static const TextField TextFields[] PROGMEM = {
  {"ssid",&ArdPortal::PortalConfig::ssid,32,true}, {"password",&ArdPortal::PortalConfig::password,64,true},
  {"host",&ArdPortal::PortalConfig::host,253,true}, {"user",&ArdPortal::PortalConfig::user,128,true},
  {"mqttPassword",&ArdPortal::PortalConfig::mqttPassword,128,true}, {"caCert",&ArdPortal::PortalConfig::caCert,4096,true},
  {"apName",&ArdPortal::PortalConfig::apName,32,true}, {"apPassword",&ArdPortal::PortalConfig::apPassword,63,true},
  {"deviceName",&ArdPortal::PortalConfig::deviceName,32,false},
  {"deviceDescription",&ArdPortal::PortalConfig::deviceDescription,128,false},
  {"deviceManufacturer",&ArdPortal::PortalConfig::deviceManufacturer,128,false}
};
/**
 * @brief Copy a portal text-field descriptor from program memory.
 * @param index Zero-based element or field index.
 * @return Descriptor containing its key, config member pointer, limit and required flag.
 */
inline TextField field(size_t index) {
  TextField entry;memcpy_P(&entry,TextFields+index,sizeof(entry));return entry;
}
/**
 * @brief Compare portal settings by their scalar and text fields.
 * @param a First value or byte range to compare.
 * @param b Second value or byte range to compare.
 * @return True if every portal setting matches; false otherwise.
 */
inline bool equal(const ArdPortal::PortalConfig& a, const ArdPortal::PortalConfig& b) {
  if(a.port!=b.port || a.mqttTls!=b.mqttTls)return false;
  for(size_t i=0;i<sizeof(TextFields)/sizeof(TextFields[0]);++i)if(a.*field(i).member!=b.*field(i).member)return false;
  return true;
}
/**
 * @brief Build the portal configuration JSON object with version and connection settings.
 * @param config Portal settings to validate and apply.
 * @return Portal settings as a JSON object.
 */
inline ArdJSON::JSONVar value(const ArdPortal::PortalConfig& config) {
  auto root=ArdJSON::JSONVar::object();
  root["version"]=1;root["port"]=config.port;root["mqttTls"]=config.mqttTls;
  for(size_t i=0;i<sizeof(TextFields)/sizeof(TextFields[0]);++i) {
    auto entry=field(i);root[entry.key]=config.*entry.member;
  }
  return root;
}
/**
 * @brief Serialize portal settings as a versioned JSON document.
 * @param config Portal settings to validate and apply.
 * @param codec JSON codec used for validation and serialization.
 * @return Serialized JSON text, or an empty string when serialization fails.
 */
inline String encode(const ArdPortal::PortalConfig& config, const ArdJsonCodec& codec = ArdJsonCodec()) {
  return codec.stringify(value(config),true,nullptr,limits());
}
/**
 * @brief Extract a bounded JSON string suitable for C-string configuration APIs.
 * @param value Input value, or output destination when passed by mutable reference.
 * @param result Operation result to inspect or return.
 * @param limit Maximum allowed byte count.
 * @return True with result populated for a valid bounded string without embedded NULs.
 */
inline bool text(const ArdJSON::JSONVar& value,String& result,size_t limit) {
  if(value.type()!=ArdJSON::JSONVar::Type::String)return false;
  String parsed=value.asString();if(parsed.length()>limit)return false;
  // JSON allows U+0000, but the configuration feeds APIs taking C strings.
  for(size_t i=0;i<parsed.length();++i)if(parsed[i]==0)return false;
  const size_t length=parsed.length();result=std::move(parsed);return result.length()==length;
}
/**
 * @brief Validate a portal configuration object and copy its settings to result.
 * @param root Parsed portal configuration object.
 * @param result Operation result to inspect or return.
 * @return True with result populated if all keys, types and settings are valid; false otherwise.
 */
inline bool decodeValue(const ArdJSON::JSONVar& root,ArdPortal::PortalConfig& result) {
  if(root.type()!=ArdJSON::JSONVar::Type::Object || !root.isValid())return false;
  int64_t version,port;
  if(!root["version"].toInteger(version) || version!=1 || !root["port"].toInteger(port) || port<1 || port>65535)return false;
  if(root["mqttTls"].type()!=ArdJSON::JSONVar::Type::Boolean)return false;
  ArdPortal::PortalConfig config;config.port=static_cast<uint16_t>(port);config.mqttTls=root["mqttTls"].asBool();
  size_t count=3;
  for(size_t i=0;i<sizeof(TextFields)/sizeof(TextFields[0]);++i) {
    auto entry=field(i);
    if(root.hasOwnProperty(entry.key)) {
      ++count;if(!text(root[entry.key],config.*entry.member,entry.limit))return false;
    } else if(entry.required)return false;
  }
  if(root.length()!=count || !ArdPortal::validPortalConfig(config))return false;
  result=std::move(config);return true;
}
/**
 * @brief Parse and validate a versioned portal configuration JSON document.
 * @param json Serialized portal configuration document.
 * @param result Operation result to inspect or return.
 * @param codec JSON codec used for validation and serialization.
 * @return True with result populated for a valid document; false otherwise.
 */
inline bool decode(const String& json,ArdPortal::PortalConfig& result, const ArdJsonCodec& codec = ArdJsonCodec()) {
  return decodeValue(codec.parse(json,nullptr,limits()),result);
}
}
