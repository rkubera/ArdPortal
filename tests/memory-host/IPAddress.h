#pragma once
#include "Arduino.h"
struct IPAddress {
  String text = "192.168.4.1";
  bool fromString(const String& s) { text = s; return s.indexOf('.') >= 0; }
  String toString() const { return text; }
};
