#include <cassert>
#include "ZigbeeTime.h"
int main() {
  uint8_t frame[] = { 0x18, 77, 1, 0, 0, 0, 0xe2, 0, 0, 0, 0, 1, 0, 0, 0x18, 3 };
  uint32_t utc = 0;
  assert(ArdZigbeeTime::decode(frame, sizeof(frame), 77, utc) && utc == 946684800);
  for (size_t size = 0; size < sizeof(frame); ++size) assert(!ArdZigbeeTime::decode(frame, size, 77, utc));
  assert(!ArdZigbeeTime::decode(frame, sizeof(frame), 78, utc));
  frame[15] = 0;
  assert(!ArdZigbeeTime::decode(frame, sizeof(frame), 77, utc));
  frame[15] = 2;
  frame[0] = 8;
  assert(ArdZigbeeTime::decode(frame, sizeof(frame), 77, utc));
  frame[3] = 2;
  assert(!ArdZigbeeTime::decode(frame, sizeof(frame), 77, utc));
  frame[3] = 0;
  frame[5] = 0x86;
  assert(!ArdZigbeeTime::decode(frame, sizeof(frame), 77, utc));
  frame[5] = 0;
  for (size_t i = 7; i < 11; ++i) frame[i] = 255;
  assert(!ArdZigbeeTime::decode(frame, sizeof(frame), 77, utc));
}
