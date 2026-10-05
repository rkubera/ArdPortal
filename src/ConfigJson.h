// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ArdPortalDeclarations.h"
#include "ArdJSON.h"

// Configuration schema validation, independent of ArdFS. Parsing/encoding is generic.
namespace ArdPortalJson {
inline ArdJSON::Limits limits() {
  ArdJSON::Limits value;value.maxDepth=1;value.maxNodes=15;value.maxStringBytes=4096;return value;
}
struct TextField { const char* key; String ArdPortal::Config::*member; uint16_t limit; bool required; };
static const TextField TextFields[] PROGMEM = {
  {"ssid",&ArdPortal::Config::ssid,32,true}, {"password",&ArdPortal::Config::password,64,true},
  {"host",&ArdPortal::Config::host,253,true}, {"user",&ArdPortal::Config::user,128,true},
  {"mqttPassword",&ArdPortal::Config::mqttPassword,128,true}, {"caCert",&ArdPortal::Config::caCert,4096,true},
  {"apName",&ArdPortal::Config::apName,32,true}, {"apPassword",&ArdPortal::Config::apPassword,63,true},
  {"deviceName",&ArdPortal::Config::deviceName,32,false},
  {"deviceDescription",&ArdPortal::Config::deviceDescription,128,false},
  {"deviceManufacturer",&ArdPortal::Config::deviceManufacturer,128,false}
};
inline TextField field(size_t index) {
  TextField entry;memcpy_P(&entry,TextFields+index,sizeof(entry));return entry;
}
inline bool equal(const ArdPortal::Config& a, const ArdPortal::Config& b) {
  if(a.port!=b.port || a.mqttTls!=b.mqttTls)return false;
  for(size_t i=0;i<sizeof(TextFields)/sizeof(TextFields[0]);++i)if(a.*field(i).member!=b.*field(i).member)return false;
  return true;
}
inline ArdJSON::JSONVar value(const ArdPortal::Config& config) {
  auto root=ArdJSON::JSONVar::object();
  root["version"]=1;root["port"]=config.port;root["mqttTls"]=config.mqttTls;
  for(size_t i=0;i<sizeof(TextFields)/sizeof(TextFields[0]);++i) {
    auto entry=field(i);root[entry.key]=config.*entry.member;
  }
  return root;
}
inline String encode(const ArdPortal::Config& config, const ArdJsonCodec& codec = ArdJsonCodec()) {
  return codec.stringify(value(config),true,nullptr,limits());
}
inline bool text(const ArdJSON::JSONVar& value,String& result,size_t limit) {
  if(value.type()!=ArdJSON::JSONVar::Type::String)return false;
  String parsed=value.asString();if(parsed.length()>limit)return false;
  // JSON allows U+0000, but the configuration feeds APIs taking C strings.
  for(size_t i=0;i<parsed.length();++i)if(parsed[i]==0)return false;
  const size_t length=parsed.length();result=std::move(parsed);return result.length()==length;
}
inline bool decodeValue(const ArdJSON::JSONVar& root,ArdPortal::Config& result) {
  if(root.type()!=ArdJSON::JSONVar::Type::Object || !root.isValid())return false;
  int64_t version,port;
  if(!root["version"].toInteger(version) || version!=1 || !root["port"].toInteger(port) || port<1 || port>65535)return false;
  if(root["mqttTls"].type()!=ArdJSON::JSONVar::Type::Boolean)return false;
  ArdPortal::Config config;config.port=static_cast<uint16_t>(port);config.mqttTls=root["mqttTls"].asBool();
  size_t count=3;
  for(size_t i=0;i<sizeof(TextFields)/sizeof(TextFields[0]);++i) {
    auto entry=field(i);
    if(root.hasOwnProperty(entry.key)) {
      ++count;if(!text(root[entry.key],config.*entry.member,entry.limit))return false;
    } else if(entry.required)return false;
  }
  if(root.length()!=count || !ArdPortal::validConfig(config))return false;
  result=std::move(config);return true;
}
inline bool decode(const String& json,ArdPortal::Config& result, const ArdJsonCodec& codec = ArdJsonCodec()) {
  return decodeValue(codec.parse(json,nullptr,limits()),result);
}
}
