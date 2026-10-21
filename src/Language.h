// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "LanguageData.h"
#include "FlashText.h"

// Native messages follow the language of the most recent portal HTTP request.
// This preference stays in RAM; browser preferences do not wear out flash.
namespace ArdUILanguage {
/**
 * @brief Expose the active language index stored in RAM.
 * @return Mutable reference to the current language index.
 */
inline size_t& active() { static size_t index = 0; return index; }
/**
 * @brief Copy a supported language descriptor from program memory.
 * @param index Zero-based element or field index.
 * @return Language descriptor; out-of-range indexes use language zero.
 */
inline ArdUILanguageData::Language language(size_t index) {
  ArdUILanguageData::Language value;
  if (index >= ArdUILanguageData::Count) index = 0;
  memcpy_P(&value, &ArdUILanguageData::Languages[index], sizeof(value));
  return value;
}
/**
 * @brief Locate a supported language by code.
 * @param code Protocol, language or error code.
 * @return Zero-based language index, or -1 if no language matches.
 */
inline int find(const String& code) {
  for (size_t i = 0; i < ArdUILanguageData::Count; ++i) {
    if (ardFlashEqual(code,language(i).code)) return int(i);
  }
  return -1;
}
/**
 * @brief Select a supported UI language by code.
 * @param code Protocol, language or error code.
 * @return True if the language was selected; false if the code is unsupported.
 */
inline bool select(const String& code) {
  int index = find(code); if (index < 0) return false;
  active() = size_t(index); return true;
}
using Key = ArdUILanguageData::Key;
// Numeric IDs avoid retaining message-key strings and repeated flash literals.
/**
 * @brief Find a localized message in a flash-backed message table.
 * @param id Numeric message identifier.
 * Input: const ArdUILanguageData::Message (&messages)[Size].
 * @return Localized text for the active language, or an empty string if the ID is absent.
 */
template<size_t Size>
inline const char* lookupFlash(uint16_t id, const ArdUILanguageData::Message (&messages)[Size]) {
  for (size_t i = 0; i < Size; ++i) {
    uint16_t messageId;
    memcpy_P(&messageId,&messages[i].id,sizeof(messageId));
    if(id==messageId) {
      const char* value;
      const size_t index=active()<ArdUILanguageData::Count?active():0;
      memcpy_P(&value,&messages[i].values[index],sizeof(value));
      return value&&pgm_read_byte(value)?value:nullptr;
    }
  }
  return nullptr;
}
/** @brief Copy a selected flash message only when the caller needs owned text. */
template<size_t Size>
inline String lookup(uint16_t id,const ArdUILanguageData::Message (&messages)[Size]) {
  const char* value=lookupFlash(id,messages);
  return value?String(FPSTR(value)):String();
}
/** @brief Borrow a translated message in flash; nullptr requests the legacy key fallback. */
inline const char* textFlash(Key key) {
  const char* value=lookupFlash(uint16_t(key),ArdUILanguageData::Messages);
  return value?value:lookupFlash(uint16_t(key),ArdUILanguageData::StorageMessages);
}
/**
 * @brief Decode a storage message key in the form s_<decimal ID>.
 * @param key Configuration key or JSON object member name.
 * @return The decoded 16-bit ID, or zero for an invalid key.
 */
inline uint16_t messageId(const char* key) {
  if(!key || key[0]!='s' || key[1]!='_' || key[2]<'1' || key[2]>'9') return 0;
  uint32_t id=0;
  for(const char* p=key+2;*p;++p) {
    if(*p<'0'||*p>'9')return 0;
    id=id*10+uint32_t(*p-'0');if(id>65535)return 0;
  }
  return uint16_t(id);
}
/** @brief Decode a flash-backed s_<ID> key without copying it into RAM. */
inline uint16_t messageId(const __FlashStringHelper* key) {
  const char* bytes=reinterpret_cast<const char*>(key);
  if(!bytes||pgm_read_byte(bytes)!='s'||pgm_read_byte(bytes+1)!='_')return 0;
  uint32_t id=0;uint8_t c=pgm_read_byte(bytes+2);
  if(c<'1'||c>'9')return 0;
  for(size_t i=2;(c=pgm_read_byte(bytes+i))!=0;++i) {
    if(c<'0'||c>'9')return 0;
    id=id*10+uint32_t(c-'0');if(id>65535)return 0;
  }
  return uint16_t(id);
}
/**
 * @brief Translate a storage error key using the selected language.
 * @param key Configuration key or JSON object member name.
 * @return Localized storage message, or its s_<ID> key if no translation is present.
 */
inline String storageText(Key key) {
  String value=lookup(uint16_t(key),ArdUILanguageData::StorageMessages);
  return value.length()?value:String(F("s_"))+String(uint16_t(key));
}
/**
 * @brief Resolve a UI message in the active language with storage-message fallback.
 * @param key Configuration key or JSON object member name.
 * @return Localized message, or the original message key when unavailable.
 */
inline String text(Key key) {
  String value=lookup(uint16_t(key),ArdUILanguageData::Messages);
  return value.length()?value:storageText(key);
}
/**
 * @brief Translate a storage error key using the selected language.
 * @param key Configuration key or JSON object member name.
 * @return Localized storage message, or its s_<ID> key if no translation is present.
 */
inline String storageText(const char* key) {
  String value=lookup(messageId(key),ArdUILanguageData::StorageMessages);
  return value.length()?value:String(key);
}
/**
 * @brief Resolve a UI message in the active language with storage-message fallback.
 * @param key Configuration key or JSON object member name.
 * @return Localized message, or the original message key when unavailable.
 */
inline String text(const char* key) {
  uint16_t id=messageId(key);
  String value=lookup(id,ArdUILanguageData::Messages);
  if(!value.length())value=lookup(id,ArdUILanguageData::StorageMessages);
  return value.length()?value:String(key);
}
/**
 * @brief Translate a storage error key using the selected language.
 * @param key Configuration key or JSON object member name.
 * @return Localized storage message, or its s_<ID> key if no translation is present.
 */
inline String storageText(const __FlashStringHelper* key) {
  const char* value=lookupFlash(messageId(key),ArdUILanguageData::StorageMessages);
  return value?String(FPSTR(value)):String(key);
}
/**
 * @brief Resolve a UI message in the active language with storage-message fallback.
 * @param key Configuration key or JSON object member name.
 * @return Localized message, or the original message key when unavailable.
 */
inline String text(const __FlashStringHelper* key) {
  const char* value=textFlash(static_cast<Key>(messageId(key)));
  return value?String(FPSTR(value)):String(key);
}
}
