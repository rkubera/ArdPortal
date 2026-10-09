#pragma once
#include "Arduino.h"
#define U_FLASH 0
struct UpdateStub {
  bool active = false, committed = false, failBegin = false, failWrite = false, failEnd = false;
  size_t expected = 0; int begins = 0;
  std::vector<uint8_t> image;
  bool begin(size_t size, int) { ++begins; if(failBegin) return false; expected = size; image.clear(); active = true; committed = false; return true; }
  size_t write(uint8_t* data, size_t length) { if(failWrite) return 0; image.insert(image.end(), data, data + length); return length; }
  bool end(bool) { active = false; committed = !failEnd && image.size() == expected; return committed; }
  int getError() const { return 7; }
};
extern UpdateStub Update;
