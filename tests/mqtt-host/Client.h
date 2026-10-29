#pragma once
#include <Arduino.h>
struct IPAddress {};
class Client {
public:
  virtual ~Client() = default;
  virtual int connect(IPAddress, uint16_t) {
    return 0;
  }
  virtual int connect(const char*, uint16_t) {
    return 0;
  }
  virtual uint8_t connected() = 0;
  virtual operator bool() {
    return connected();
  }
  virtual int available() = 0;
  virtual int peek() {
    return -1;
  }
  virtual int read() = 0;
  virtual int read(uint8_t*, size_t) {
    return 0;
  }
  virtual size_t write(uint8_t byte) {
    return write(&byte, 1);
  }
  virtual size_t write(const uint8_t*, size_t) = 0;
  virtual void flush() {}
  virtual void stop() = 0;
};
