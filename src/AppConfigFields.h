// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <stddef.h>
#include <stdint.h>
constexpr size_t ArdAppConfigMaxFields = 1024;
// Override in the sketch before including ArdPortal.h (bytes, shared by pages/entities).
#ifndef ARDPORTAL_APP_CONFIG_MAX_DEFINITION_BYTES
#define ARDPORTAL_APP_CONFIG_MAX_DEFINITION_BYTES (256UL * 1024UL)
#endif
static_assert(ARDPORTAL_APP_CONFIG_MAX_DEFINITION_BYTES > 0 &&
              static_cast<unsigned long long>(ARDPORTAL_APP_CONFIG_MAX_DEFINITION_BYTES) <= UINT32_MAX,
              "ARDPORTAL_APP_CONFIG_MAX_DEFINITION_BYTES must be 1..UINT32_MAX");
constexpr size_t ArdAppConfigMaxDefinitionBytes = ARDPORTAL_APP_CONFIG_MAX_DEFINITION_BYTES;
// Fixed storage; no allocation or shifts beyond a machine word.
class ArdAppConfigFieldMask {
  uint64_t words[16];
public:
  ArdAppConfigFieldMask(uint64_t value = 0) : words{} { words[0] = value; }
  static ArdAppConfigFieldMask forField(size_t i) {
    ArdAppConfigFieldMask result;
    if (i < ArdAppConfigMaxFields) result.words[i / 64] = uint64_t(1) << (i % 64);
    return result;
  }
  bool test(size_t i) const {return i<ArdAppConfigMaxFields&&(words[i/64]&(uint64_t(1)<<(i%64)));}
  size_t firstSet() const {for(size_t i=0;i<16;++i)if(words[i])return i*64+size_t(__builtin_ctzll(words[i]));return ArdAppConfigMaxFields;}
  bool operator==(const ArdAppConfigFieldMask& v) const {
    for(size_t i=0;i<16;++i) if(words[i]!=v.words[i]) return false;
    return true;
  }
  explicit operator bool() const { for(size_t i=0;i<16;++i) if(words[i]) return true; return false; }
  ArdAppConfigFieldMask operator~() const {
    ArdAppConfigFieldMask result;for(size_t i=0;i<16;++i) result.words[i]=~words[i];return result;
  }
  ArdAppConfigFieldMask operator&(const ArdAppConfigFieldMask& v) const {
    ArdAppConfigFieldMask result;for(size_t i=0;i<16;++i) result.words[i]=words[i]&v.words[i];return result;
  }
  ArdAppConfigFieldMask operator|(const ArdAppConfigFieldMask& v) const {
    ArdAppConfigFieldMask result;for(size_t i=0;i<16;++i) result.words[i]=words[i]|v.words[i];return result;
  }
  ArdAppConfigFieldMask& operator|=(const ArdAppConfigFieldMask& v) {
    for(size_t i=0;i<16;++i) words[i]|=v.words[i];return *this;
  }
  ArdAppConfigFieldMask& operator&=(const ArdAppConfigFieldMask& v) {
    for(size_t i=0;i<16;++i) words[i]&=v.words[i];return *this;
  }
};
