// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include <stddef.h>
namespace ArdZigbeeTime {
/** @brief Decode a correlated standard Time read response. @param data ZCL frame. @param bytes Frame size. @param sequence Expected transaction. @param unixTime Output UTC seconds. @return True only for a valid synchronized/master clock. */
inline bool decode(const uint8_t* data, size_t bytes, uint8_t sequence, uint32_t& unixTime) {
  if (!data || bytes < 3 || (data[0] & 0xef) != 0x08 || data[1] != sequence || data[2] != 1) return false;
  size_t at = 3;
  uint32_t seconds = 0;
  uint8_t status = 0;
  bool haveTime = false, haveStatus = false;
  while (at + 3 <= bytes) {
    uint16_t id = data[at] | uint16_t(data[at + 1]) << 8;
    at += 2;
    if (data[at++] != 0) continue;
    if (at >= bytes) return false;
    uint8_t type = data[at++];
    size_t size = type == 0xe2 ? 4 : (type == 0x18 ? 1 : 0);
    if (!size || at + size > bytes) return false;
    if (id == 0 && type == 0xe2) {
      seconds = uint32_t(data[at]) | uint32_t(data[at + 1]) << 8 | uint32_t(data[at + 2]) << 16 | uint32_t(data[at + 3]) << 24;
      haveTime = true;
    }
    if (id == 1 && type == 0x18) {
      status = data[at];
      haveStatus = true;
    }
    at += size;
  }
  if (at != bytes || !haveTime || !haveStatus || !(status & 3) || seconds == 0xffffffff || seconds > 0xffffffffUL - 946684800UL) return false;
  unixTime = seconds + 946684800UL;
  return true;
}
}
