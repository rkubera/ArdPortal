// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>

/** @brief Compare RAM text with a NUL-terminated flash literal without a temporary String. */
inline bool ardFlashEqual(const String& text,const char* flash) {
  return flash&&text.length()==strlen_P(flash)&&strcmp_P(text.c_str(),flash)==0;
}
inline bool ardFlashEqual(const String& text,const __FlashStringHelper* flash) {
  return ardFlashEqual(text,reinterpret_cast<const char*>(flash));
}
