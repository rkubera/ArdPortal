// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include <stddef.h>
namespace ArdZigbeeConverterLayout {
/** @brief Accumulate deterministic exported metadata; no device identity or live state is used.
 * @param first First 32-bit accumulator. @param second Independent 32-bit accumulator.
 * @param data Serialized endpoint metadata. @param bytes Number of bytes.
 * @return No value. This noncryptographic signature identifies a control layout.
 */
inline void append(uint32_t& first,uint32_t& second,const char* data,size_t bytes){
  for(size_t n=0;n<bytes;++n){uint8_t b=uint8_t(data[n]);first=(first^b)*16777619UL;second=(second^b)*2246822519UL;}
}
}
