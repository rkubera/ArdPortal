#pragma once
#include <string>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <functional>
#include <vector>
#include <memory>
#include <deque>
#include <map>
#include <cctype>
#include <type_traits>
#define pgm_read_byte(address) (*reinterpret_cast<const uint8_t*>(address))
#define PROGMEM
#define memcpy_P memcpy
#define strlen_P strlen
#define strcmp_P strcmp
class __FlashStringHelper;
#define FPSTR(value) reinterpret_cast<const __FlashStringHelper*>(value)
#define F(value) FPSTR(value)
extern uint32_t fakeMillis;
inline uint32_t fakeMillisStepForTest=0;
inline uint32_t millis() { fakeMillis+=fakeMillisStepForTest;return fakeMillis; }
class String {
  std::string value;
public:
  String() = default;
  static size_t& cStringConstructionsForTest() { static size_t count=0;return count; }
  String(const char* text) : value(text ? text : "") { ++cStringConstructionsForTest(); }
  String(const __FlashStringHelper* text) : String(reinterpret_cast<const char*>(text)) {}
  String(const std::string& text) : value(text) {}
  template<class T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
  String(T number) : value(std::to_string(number)) {}
  String& operator=(const char* text) { value.assign(text?text:"");return *this; }
  bool concat(const char* text,size_t size) {value.append(text,size);return true;}
  size_t capacityForTest() const { return value.capacity(); }
  size_t length() const { return value.size(); }
  const char* c_str() const { return value.c_str(); }
  char operator[](size_t i) const { return value[i]; }
  String& operator+=(char c) { value += c; return *this; }
  String& operator+=(const String& s) { value += s.value; return *this; }
  friend String operator+(String a, const String& b) { return a += b; }
  friend String operator+(String a, char b) { return a += b; }
  bool operator==(const String& b) const { return value == b.value; }
  bool operator!=(const String& b) const { return !(*this == b); }
  int indexOf(const String& needle, size_t offset = 0) const { size_t p = value.find(needle.value, offset); return p == std::string::npos ? -1 : int(p); }
  int indexOf(char needle, size_t offset = 0) const { size_t p = value.find(needle, offset); return p == std::string::npos ? -1 : int(p); }
  String substring(size_t first, size_t last) const { return first < value.size() ? value.substr(first, last - first) : String(); }
  String substring(size_t first) const { return first < value.size() ? value.substr(first) : String(); }
  void remove(size_t first) { if (first < value.size()) value.erase(first); }
  void toLowerCase() { for (char& c : value) c = std::tolower(static_cast<unsigned char>(c)); }
  void trim() { size_t first = value.find_first_not_of(" \t\r\n"); if(first == std::string::npos) value.clear(); else value = value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1); }
  void replace(const String& a, const String& b) { size_t p = 0; while ((p = value.find(a.value, p)) != std::string::npos) { value.replace(p, a.length(), b.value); p += b.length(); } }
  static size_t& reserveLimitForTest() { static size_t limit = SIZE_MAX; return limit; }
  bool reserve(size_t size) { if(size > reserveLimitForTest()) return false; value.reserve(size); return true; }
};
struct SerialStub {
  void begin(int) {}
  template<class T> void print(const T&) {}
  template<class T> void println(const T&) {}
  void println() {}
  template<class... T> void printf(const char*, T...) {}
  void write(const uint8_t*, size_t) {}
};
extern SerialStub Serial;

#include <time.h>
extern time_t fakeUnixTime;
inline time_t fakeTime(time_t*) { return fakeUnixTime; }
extern int ntpStarts;
extern String lastNtpServer1, lastNtpServer2;
inline void configTime(int timezone, int daylight, const char* first, const char* second) {
  if (timezone != 0 || daylight != 0) std::abort();
  ++ntpStarts; lastNtpServer1 = first; lastNtpServer2 = second;
}
#define time fakeTime

extern std::vector<uint8_t> ardfsTestFlash;
extern int ardfsFlashCalls, ardfsFailAt, ardfsPrograms, ardfsErases;
extern bool ardfsPowerOff, ardfsTornOperation;
inline bool ardfsFlashRange(uint32_t address, size_t size) {
  return address >= 0x100000U && size <= ardfsTestFlash.size() && address - 0x100000U <= ardfsTestFlash.size() - size;
}
inline bool ardfsFlashFailure() {
  if (ardfsPowerOff) return true;
  if (++ardfsFlashCalls == ardfsFailAt) { ardfsPowerOff = true; return true; }
  return false;
}
struct rst_info {uint32_t reason=4;};
struct EspStub {
  rst_info resetInfo;
  uint32_t freeHeap=32000;
  rst_info* getResetInfoPtr(){return &resetInfo;}
  uint32_t getMaxFreeBlockSize() const{return 24000;}
  int restarts = 0;
  bool flashRead(uint32_t address, uint8_t* bytes, size_t size) {
    if ((address & 3U) || (size & 3U) || !ardfsFlashRange(address, size) || ardfsFlashFailure()) return false;
    memcpy(bytes, ardfsTestFlash.data() + address - 0x100000U, size); return true;
  }
  bool flashWrite(uint32_t address, const uint8_t* bytes, size_t size) {
    if (ardfsPowerOff || !ardfsFlashRange(address, size)) return false;
    ++ardfsPrograms; bool failed = ardfsFlashFailure();
    size_t count = failed ? (ardfsTornOperation ? size / 2 : 0) : size;
    for(size_t i=0;i<count;++i) ardfsTestFlash[address - 0x100000U + i] &= bytes[i];
    return !failed;
  }
  bool flashEraseSector(uint32_t sector) {
    uint32_t address = sector * 4096U;
    if (ardfsPowerOff || !ardfsFlashRange(address, 4096)) return false;
    ++ardfsErases; bool failed = ardfsFlashFailure();
    size_t count = failed ? (ardfsTornOperation ? 2048 : 0) : 4096;
    std::fill_n(ardfsTestFlash.begin() + address - 0x100000U, count, 0xff); return !failed;
  }
  uint32_t getChipId() const { return 0x123ABC; }
  uint32_t getFlashChipRealSize() const { return 4*1024*1024; }
  uint32_t getFlashChipSize() const { return 4*1024*1024; }
  uint32_t getFreeHeap() const { return freeHeap; }
  uint32_t getSketchSize() const { return 400000; }
  uint32_t getFreeSketchSpace() const { return 1000000; }
  void restart() { ++restarts; }
};
extern EspStub ESP;

inline unsigned fakeYieldCalls=0;
inline void yield() { ++fakeYieldCalls; } // Core scheduling point in host tests.
