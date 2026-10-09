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
  /**
   * @brief Initialize this instance and its owned state.
   * @param value Input value, or output destination when passed by mutable reference.
   * @return No value.
   */
  ArdAppConfigFieldMask(uint64_t value = 0) : words{} { words[0] = value; }
  /**
   * @brief Build a mask containing only the requested field bit.
   * @param i Zero-based bit, byte or element index.
   * @return A mask with one bit set, or an empty mask for an out-of-range index.
   */
  static ArdAppConfigFieldMask forField(size_t i) {
    ArdAppConfigFieldMask result;
    if (i < ArdAppConfigMaxFields) result.words[i / 64] = uint64_t(1) << (i % 64);
    return result;
  }
  /**
   * @brief Check whether the requested bit is set in the field mask.
   * @param i Zero-based bit, byte or element index.
   * @return True if the indexed bit is set; false if clear or outside the field limit.
   */
  bool test(size_t i) const {return i<ArdAppConfigMaxFields&&(words[i/64]&(uint64_t(1)<<(i%64)));}
  /**
   * @brief Find the lowest marked field index; return the field limit when the mask is empty.
   * @return The lowest set field index, or ArdAppConfigMaxFields when no bit is set.
   */
  size_t firstSet() const {for(size_t i=0;i<16;++i)if(words[i])return i*64+size_t(__builtin_ctzll(words[i]));return ArdAppConfigMaxFields;}
  /**
   * @brief Compare all stored values for equality.
   * @param v Value or mask to compare, combine or store.
   * @return True if all mask words match; false otherwise.
   */
  bool operator==(const ArdAppConfigFieldMask& v) const {
    for(size_t i=0;i<16;++i) if(words[i]!=v.words[i]) return false;
    return true;
  }
  /**
   * @brief Check whether any field-mask bit is set.
   * @return True if at least one mask bit is set; false if the mask is empty.
   */
  explicit operator bool() const { for(size_t i=0;i<16;++i) if(words[i]) return true; return false; }
  /**
   * @brief Build the bitwise complement of the field mask.
   * @return A new mask with every bit inverted.
   */
  ArdAppConfigFieldMask operator~() const {
    ArdAppConfigFieldMask result;for(size_t i=0;i<16;++i) result.words[i]=~words[i];return result;
  }
  /**
   * @brief Build the intersection of two field masks.
   * @param v Value or mask to compare, combine or store.
   * @return A new mask containing bits set in both operands.
   */
  ArdAppConfigFieldMask operator&(const ArdAppConfigFieldMask& v) const {
    ArdAppConfigFieldMask result;for(size_t i=0;i<16;++i) result.words[i]=words[i]&v.words[i];return result;
  }
  /**
   * @brief Build the union of two field masks.
   * @param v Value or mask to compare, combine or store.
   * @return A new mask containing bits set in either operand.
   */
  ArdAppConfigFieldMask operator|(const ArdAppConfigFieldMask& v) const {
    ArdAppConfigFieldMask result;for(size_t i=0;i<16;++i) result.words[i]=words[i]|v.words[i];return result;
  }
  /**
   * @brief Merge another field mask into this mask in place.
   * @param v Value or mask to compare, combine or store.
   * @return Reference to this mask after merging.
   */
  ArdAppConfigFieldMask& operator|=(const ArdAppConfigFieldMask& v) {
    for(size_t i=0;i<16;++i) words[i]|=v.words[i];return *this;
  }
  /**
   * @brief Intersect this field mask with another mask in place.
   * @param v Value or mask to compare, combine or store.
   * @return Reference to this mask after intersection.
   */
  ArdAppConfigFieldMask& operator&=(const ArdAppConfigFieldMask& v) {
    for(size_t i=0;i<16;++i) words[i]&=v.words[i];return *this;
  }
};
