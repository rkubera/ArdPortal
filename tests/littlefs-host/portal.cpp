// Reuse the Portal persistence scenario against the real LittleFS adapter.
// The define is deliberately local to this translation unit, like an .ino.
#define ARDPORTAL_USE_LITTLEFS 1
#include "../memory-host/no-dynamic.cpp"
