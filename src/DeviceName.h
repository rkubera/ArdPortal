// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "DeviceNameCharacters.h"
namespace ArdDeviceName {
/**
 * @brief Read a packed Unicode range boundary from program memory.
 * @param index Zero-based element or field index.
 * @param edge Lower or upper packed Unicode range boundary.
 * @return A packed Unicode range boundary from program memory.
 */
inline uint32_t characterBound(size_t index,size_t edge) {
  const uint8_t* bytes=&Characters[index][edge*3];
  return uint32_t(pgm_read_byte(bytes))|(uint32_t(pgm_read_byte(bytes+1))<<8)|(uint32_t(pgm_read_byte(bytes+2))<<16);
}
/**
 * @brief Check the Unicode letter and number ranges allowed in device names.
 * @param cp Unicode code point to validate.
 * @return True if the code point belongs to the allowed Unicode letter or number ranges.
 */
inline bool letterOrNumber(uint32_t cp) {
  size_t lo=0,hi=sizeof(Characters)/sizeof(Characters[0]);
  while(lo<hi){size_t mid=lo+(hi-lo)/2;uint32_t first=characterBound(mid,0),last=characterBound(mid,1);if(cp<first)hi=mid;else if(cp>last)lo=mid+1;else return true;}
  return false;
}
/**
 * @brief Validate UTF-8 device names up to 32 bytes; allow letters, numbers, spaces, hyphens and underscores.
 * @param name Device name to validate.
 * @return True for an empty default name or a valid nonblank name; false for malformed UTF-8 or forbidden characters.
 */
inline bool valid(const String& name) {
  if(name.length()>32)return false;
  bool nonSpace=false;
  for(size_t i=0;i<name.length();) {
    uint32_t cp=uint8_t(name[i++]);unsigned remaining=0;uint32_t minimum=0;
    if(cp>=128){if(cp>=0xc2&&cp<=0xdf){cp&=31;remaining=1;minimum=0x80;}else if(cp>=0xe0&&cp<=0xef){cp&=15;remaining=2;minimum=0x800;}else if(cp>=0xf0&&cp<=0xf4){cp&=7;remaining=3;minimum=0x10000;}else return false;
      while(remaining--){if(i>=name.length())return false;uint8_t next=name[i++];if((next&0xc0)!=0x80)return false;cp=(cp<<6)|(next&63);}
      if(cp<minimum||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))return false;
    }
    if(cp<128){if(!((cp>='a'&&cp<='z')||(cp>='A'&&cp<='Z')||(cp>='0'&&cp<='9')||cp==' '||cp=='-'||cp=='_'))return false;}else if(!letterOrNumber(cp))return false;
    if(cp!=' ')nonSpace=true;
  }
  return !name.length()||nonSpace;
}
/**
 * @brief Replace device-name spaces with hyphens for the AP SSID.
 * @param name Configured device name.
 * @return The converted device name.
 */
inline String ap(const String& name){String result=name;result.replace(' ','-');return result;}
/**
 * @brief Replace device-name spaces with hyphens for the MQTT topic identity.
 * @param name Configured device name.
 * @return The converted device name.
 */
inline String mqtt(const String& name){return ap(name);}
/**
 * @brief Build an ASCII hostname from the device name, collapsing separators into hyphens.
 * @param name Configured device name.
 * @param fallback Hostname to use if normalization produces an empty result.
 * @return The resulting ASCII hostname, or fallback when no ASCII letters or numbers remain.
 */
inline String hostname(const String& name,const String& fallback){String result;bool separator=false;for(size_t i=0;i<name.length();++i){char c=name[i];if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')){if(separator&&result.length())result+='-';result+=c;separator=false;}else separator=true;}return result.length()?result:fallback;}
}
