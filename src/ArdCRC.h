// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include <stddef.h>
namespace ArdCRC {
// Shared reflected CRC-32 byte update for native files and journal streams.
__attribute__((noinline)) inline uint32_t updateByte(uint32_t crc, uint8_t byte) {
  crc ^= byte;
  for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1)));
  return crc;
}
// Reflected CRC-32 polynomials: bit 31 is the multiplicative identity.
inline uint32_t multiply(uint32_t left, uint32_t right) {
  uint32_t result = 0;
  for (uint32_t bit = uint32_t(1) << 31; bit; bit >>= 1) {
    if (left & bit) result ^= right;
    right = (right >> 1) ^ (0xedb88320U & (0U - (right & 1)));
  }
  return result;
}
__attribute__((noinline)) inline uint32_t combine(uint32_t left, uint32_t right, size_t bytes) {
  if (!bytes) return left;
  uint32_t power = uint32_t(1) << 23;  // x^8: append one zero byte.
  do {
    if (bytes & 1) left = multiply(power, left);
    bytes >>= 1;
    if (bytes) power = multiply(power, power);
  } while (bytes);
  return left ^ right;
}

}
