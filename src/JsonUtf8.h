// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
namespace ArdUtf8 {
template<class = void>
#if defined(__GNUC__)
__attribute__((noinline))
#endif
bool
consume(uint8_t b, uint8_t& remaining, uint32_t& code, uint32_t& minimum) {
  if (remaining) {
    if ((b & 0xc0) != 0x80) return false;
    code = (code << 6) | (b & 63);
    if (!--remaining && (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))) return false;
  } else if (b >= 0x80) {
    if (b >= 0xc2 && b <= 0xdf) {
      remaining = 1;
      code = b & 31;
      minimum = 0x80;
    } else if (b >= 0xe0 && b <= 0xef) {
      remaining = 2;
      code = b & 15;
      minimum = 0x800;
    } else if (b >= 0xf0 && b <= 0xf4) {
      remaining = 3;
      code = b & 7;
      minimum = 0x10000;
    } else return false;
  }
  return true;
}
}
