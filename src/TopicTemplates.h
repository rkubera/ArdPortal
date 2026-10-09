// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ArdJSON.h"
#include "JsonFieldSlices.h"
namespace ArdTopicTemplates {
// Resolve only HA topic strings. Labels, payloads and Jinja templates stay literal.
inline bool resolve(ArdJSON::JSONVar& field,const String& device) {
  using V=ArdJSON::JSONVar;
  const V& definition=field;
  if(!definition.hasOwnProperty("ha"))return true;
  const V& ha=definition["ha"];
  if(ha.type()!=V::Type::Object)return true;
  V keys=ha.keys();if(!keys.isValid())return false;
  for(size_t i=0;i<keys.length();++i){
    String key=keys[i].asString();
    if(key!="topic"&&!(key.length()>=6&&!memcmp(key.c_str()+key.length()-6,"_topic",6)))continue;
    if(ha[key].type()!=V::Type::String)continue;
    String topic=ha[key].asString();int at=topic.indexOf("$device");if(at<0)continue;
    if(!device.length())return false;
    size_t count=0;for(int p=at;p>=0;p=topic.indexOf("$device",p+7))++count;
    const size_t expected=topic.length()-count*7+count*device.length();
    topic.replace("$device",device);if(topic.length()!=expected)return false;
    field["ha"][key]=topic;if(!field.isValid())return false;
  }
  return true;
}
/** @brief Resolve topic strings in a validated basic definition without building its DOM.
 * Only direct members of the top-level HA object are expanded, matching resolve().
 * Preserve all other source bytes, including labels, payloads and escaped strings. */
inline String resolveJson(const String& source,const String& device) {
  ArdJsonFieldSlices slices(source);String output;size_t cursor=0;bool changed=false;
  const bool ok=slices.members(0,source.length(),[&](const String& name,size_t start,size_t length){
    if(name!="ha"||slices.character(start)!='{')return true;
    return slices.members(start,length,[&](const String& key,size_t value,size_t bytes){
      if(key!="topic"&&!(key.length()>=6&&!memcmp(key.c_str()+key.length()-6,"_topic",6)))return true;
      if(slices.character(value)!='"')return true;
      auto parsed=ArdJSON::JSON.parse(slices.slice(value,bytes));if(!parsed.isValid()||parsed.type()!=ArdJSON::JSONVar::Type::String)return false;
      String topic=parsed.asString();int at=topic.indexOf("$device");if(at<0)return true;
      if(!device.length())return false;
      size_t count=0;for(int p=at;p>=0;p=topic.indexOf("$device",p+7))++count;
      const size_t expected=topic.length()-count*7+count*device.length();
      topic.replace("$device",device);if(topic.length()!=expected)return false;
      String encoded=ArdJSON::JSON.stringify(ArdJSON::JSONVar(topic));if(!encoded.length())return false;
      if(!changed&&!output.reserve(source.length()))return false;
      String prefix=slices.slice(cursor,value-cursor);if(prefix.length()!=value-cursor)return false;
      if(!output.concat(prefix.c_str(),prefix.length())||!output.concat(encoded.c_str(),encoded.length()))return false;
      cursor=value+bytes;changed=true;return true;
    });
  });
  if(!ok)return String();if(!changed)return source;
  String tail=slices.slice(cursor,source.length()-cursor);
  if(tail.length()!=source.length()-cursor||!output.concat(tail.c_str(),tail.length()))return String();return output;
}

/** @brief Reproduce registration's extended/persist flags without a definition DOM. */
inline String fieldHttpJson(const String& source,const String& device,bool extended,bool persist) {
  if(!extended)return resolveJson(source,device);
  ArdJsonFieldSlices slices(source);String normalized="{";bool first=true;
  if(!normalized.reserve(source.length()+40))return String();
  if(!slices.members(0,source.length(),[&](const String& key,size_t start,size_t bytes){
    if(key=="extended"||key=="persist"||key=="_pageVisibleWhen")return true;
    String encoded=ArdJSON::JSON.stringify(ArdJSON::JSONVar(key)),value=slices.slice(start,bytes);
    if(!encoded.length()||value.length()!=bytes)return false;
    if(!first&&!normalized.concat(",",1))return false;first=false;
    return normalized.concat(encoded.c_str(),encoded.length())&&normalized.concat(":",1)&&normalized.concat(value.c_str(),value.length());
  }))return String();
  if(!first&&!normalized.concat(",",1))return String();
  const char* flags=persist?"\"extended\":true,\"persist\":true}":"\"extended\":true,\"persist\":false}";
  if(!normalized.concat(flags,strlen(flags)))return String();return resolveJson(normalized,device);
}

}
