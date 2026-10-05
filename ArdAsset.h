// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>

// Compiler-selected independent DEFLATE blocks share one gzip header/footer.
static const char ARD_ASSET_GZIP_HEAD[] PROGMEM = "\x1f\x8b\x08\x00\x00\x00\x00\x00\x02\xff";
static const char ARD_ASSET_DEFLATE_END[] PROGMEM = "\x03\x00";
struct ArdAssetChunk { const char* data; size_t size; uint32_t crc=0, plainSize=0; };
inline ArdAssetChunk ardAssetChunk(const ArdAssetChunk* chunks,size_t index) {
  ArdAssetChunk value;memcpy_P(&value,chunks+index,sizeof(value));return value;
}
inline size_t ardAssetSize(const ArdAssetChunk* chunks,size_t count) {
  size_t size=0;for(size_t i=0;i<count;++i)size+=ardAssetChunk(chunks,i).size;return size;
}
// Polynomial multiplication in the reflected CRC-32 representation.
inline uint32_t ardAssetCrcProduct(uint32_t a,uint32_t b) {
  uint32_t result=0;
  for(uint32_t bit=0x80000000U;bit;bit>>=1) {
    if(a&bit) result^=b;
    b=(b>>1)^((b&1)?0xedb88320U:0);
  }
  return result;
}
inline uint32_t ardAssetCrcAppend(uint32_t crc,uint32_t next,uint32_t bytes) {
  uint32_t factor=0x40000000U;
  for(unsigned i=0;i<3;++i)factor=ardAssetCrcProduct(factor,factor);
  while(bytes) {
    if(bytes&1)crc=ardAssetCrcProduct(crc,factor);
    bytes>>=1;if(bytes)factor=ardAssetCrcProduct(factor,factor);
  }
  return crc^next;
}
