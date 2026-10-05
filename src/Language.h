// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "LanguageData.h"

// Native messages follow the language of the most recent portal HTTP request.
// This preference stays in RAM; browser preferences do not wear out flash.
namespace ArdUILanguage {
inline size_t& active() { static size_t index = 0; return index; }
inline ArdUILanguageData::Language language(size_t index) {
  ArdUILanguageData::Language value;
  if (index >= ArdUILanguageData::Count) index = 0;
  memcpy_P(&value, &ArdUILanguageData::Languages[index], sizeof(value));
  return value;
}
inline int find(const String& code) {
  for (size_t i = 0; i < ArdUILanguageData::Count; ++i) {
    if (code == String(FPSTR(language(i).code))) return int(i);
  }
  return -1;
}
inline bool select(const String& code) {
  int index = find(code); if (index < 0) return false;
  active() = size_t(index); return true;
}
using Key = ArdUILanguageData::Key;
// Numeric IDs avoid retaining message-key strings and repeated flash literals.
template<size_t Size>
inline String lookup(uint16_t id, const ArdUILanguageData::Message (&messages)[Size]) {
  for (size_t i = 0; i < Size; ++i) {
    ArdUILanguageData::Message message;
    // Copy records from flash before reading their fields.
    memcpy_P(&message, &messages[i], sizeof(message));
    if (id == message.id) return String(FPSTR(message.values[active()]));
  }
  return String();
}
inline uint16_t messageId(const char* key) {
  if(!key || key[0]!='s' || key[1]!='_' || key[2]<'1' || key[2]>'9') return 0;
  uint32_t id=0;
  for(const char* p=key+2;*p;++p) {
    if(*p<'0'||*p>'9')return 0;
    id=id*10+uint32_t(*p-'0');if(id>65535)return 0;
  }
  return uint16_t(id);
}
inline String storageText(Key key) {
  String value=lookup(uint16_t(key),ArdUILanguageData::StorageMessages);
  return value.length()?value:String(F("s_"))+String(uint16_t(key));
}
inline String text(Key key) {
  String value=lookup(uint16_t(key),ArdUILanguageData::Messages);
  return value.length()?value:storageText(key);
}
inline String storageText(const char* key) {
  String value=lookup(messageId(key),ArdUILanguageData::StorageMessages);
  return value.length()?value:String(key);
}
inline String text(const char* key) {
  uint16_t id=messageId(key);
  String value=lookup(id,ArdUILanguageData::Messages);
  if(!value.length())value=lookup(id,ArdUILanguageData::StorageMessages);
  return value.length()?value:String(key);
}
inline String storageText(const __FlashStringHelper* key) { return storageText(String(key).c_str()); }
inline String text(const __FlashStringHelper* key) { return text(String(key).c_str()); }
}
