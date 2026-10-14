// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <ctime>
#include <cstdio>
namespace ArdConsoleTime {
/** @brief Format UTC for all console channels. @param value UTC seconds. @param output Destination. @param capacity Buffer bytes. @return True when a complete timestamp fits. */
inline bool utcTimestamp(time_t value, char* output, size_t capacity) {
  if (!capacity) return false;
  output[0] = 0;
  struct tm date;
  if (!gmtime_r(&value, &date) || date.tm_year < -1900 || date.tm_year > 8099) return false;
  int length = snprintf(output, capacity, "%04d-%02d-%02dT%02d:%02d:%02dZ",
                        date.tm_year + 1900, date.tm_mon + 1, date.tm_mday,
                        date.tm_hour, date.tm_min, date.tm_sec);
  if (length < 0 || size_t(length) >= capacity) { output[0] = 0; return false; }
  return true;
}
}
