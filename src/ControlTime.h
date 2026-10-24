// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ArdPortalFeatures.h"
#include <Arduino.h>
namespace ArdControl {
#if ARDPORTAL_ENABLE_CLASSES || ARDPORTAL_ENABLE_CONTROL_DATE || ARDPORTAL_ENABLE_CONTROL_TIME || ARDPORTAL_ENABLE_CONTROL_DATETIME
/**
 * @brief Decode a decimal digit range into an unsigned value.
 * @param text Text to read, encode or display.
 * @param count Number of items or bytes to process.
 * @return The decoded value; callers must validate the digits first.
 */
inline unsigned decimalDigits(const char* text, size_t count) {
  unsigned value = 0;
  for (size_t i = 0; i < count; ++i) value = value * 10 + unsigned(text[i] - '0');
  return value;
}
/**
 * @brief Validate YYYY-MM-DD syntax and calendar ranges, including leap years.
 * @param text Text to read, encode or display.
 * @param length Number of bytes or elements to process.
 * @return True for a valid calendar date; false otherwise.
 */
inline bool dateParts(const char* text, size_t length) {
  if (length != 10 || text[4] != '-' || text[7] != '-') return false;
  for (size_t i = 0; i < 10; ++i)
    if (i != 4 && i != 7 && (text[i] < '0' || text[i] > '9')) return false;
  unsigned y = decimalDigits(text, 4), m = decimalDigits(text + 5, 2), d = decimalDigits(text + 8, 2);
  static const uint8_t days[] PROGMEM = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  return y >= 1 && m >= 1 && m <= 12 && d >= 1 && d <= unsigned(pgm_read_byte(days + m - 1) + (m == 2 && y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)));
}
/**
 * @brief Validate or format a calendar date value.
 * @param text Text to read, encode or display.
 * @return True on success; false if validation, resource allocation or the operation fails.
 */
inline bool date(const String& text) {
  return dateParts(text.c_str(), text.length());
}
/**
 * @brief Validate HH:MM or HH:MM:SS syntax and time ranges.
 * @param text Text to read, encode or display.
 * @param length Number of bytes or elements to process.
 * @return True for a valid clock value; false otherwise.
 */
inline bool clockParts(const char* text, size_t length) {
  if ((length != 5 && length != 8) || text[2] != ':' || (length == 8 && text[5] != ':')) return false;
  for (size_t i = 0; i < length; ++i)
    if (i != 2 && i != 5 && (text[i] < '0' || text[i] > '9')) return false;
  return decimalDigits(text, 2) < 24 && decimalDigits(text + 3, 2) < 60 && (length == 5 || decimalDigits(text + 6, 2) < 60);
}
/**
 * @brief Validate the time control value and its range.
 * @param text Text to read, encode or display.
 * @return True on success; false if validation, resource allocation or the operation fails.
 */
inline bool time(const String& text) {
  return text.length() == 8 && clockParts(text.c_str(), 8);
}
inline bool datetime(const String& s) {
  if (s.length() < 20 || s[10] != 'T' || !dateParts(s.c_str(), 10) || !clockParts(s.c_str() + 11, 8)) return false;
  size_t end = 19;
  if (s[end] == '.') {
    ++end;
    size_t start = end;
    while (end < s.length() && s[end] >= '0' && s[end] <= '9') ++end;
    if (end == start || end - start > 6) return false;
  }
  if (end + 1 == s.length() && s[end] == 'Z') return true;
  return end + 6 == s.length() && (s[end] == '+' || s[end] == '-') && s[end + 3] == ':' && clockParts(s.c_str() + end + 1, 5);
}
#endif
}
