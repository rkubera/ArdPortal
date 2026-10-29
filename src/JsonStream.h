// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ArdAllocation.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Incremental strict JSON syntax validator. Strings/numbers are never retained.
// Only hashes of keys in currently open objects are retained for duplicate-key
// rejection. A hash collision rejects input rather than accepting a duplicate.
class ArdJsonStream {
public:
  enum class Event { Object,
                     Array,
                     EndObject,
                     EndArray,
                     Key,
                     KeyByte,
                     EndKey,
                     String,
                     StringByte,
                     EndString,
                     Number,
                     NumberByte,
                     EndNumber,
                     Literal };
  enum class Error : uint8_t { None,
                               Value,
                               String,
                               Member,
                               Separator,
                               Trailing,
                               Number,
                               Fraction,
                               Exponent,
                               Literal,
                               Escape,
                               Unicode,
                               LowSurrogate,
                               UnpairedSurrogate,
                               MissingSurrogate,
                               Utf8,
                               Control,
                               UnfinishedEscape,
                               UnterminatedString,
                               Depth,
                               Container,
                               Duplicate,
                               Memory };
  bool failed = false, outOfMemory = false;
  Error error = Error::None;
  struct Listener {
    void* context;
    bool (*invoke)(void*, Event, uint8_t, unsigned);
    bool operator()(Event e, uint8_t b, unsigned depth) const {
      return invoke(context, e, b, depth);
    }
  };
  template<class Emit> void input(uint8_t b, Emit& emit) {
    const Listener listener{ const_cast<void*>(static_cast<const void*>(&emit)),
                             [](void* context, Event e, uint8_t c, unsigned depth) {
                               return (*static_cast<Emit*>(context))(e, c, depth);
                             } };
    inputByte(b, listener);
  }
  template<class Emit> bool finish(Emit& emit) {
    const Listener listener{ const_cast<void*>(static_cast<const void*>(&emit)),
                             [](void* context, Event e, uint8_t b, unsigned depth) {
                               return (*static_cast<Emit*>(context))(e, b, depth);
                             } };
    return finishInput(listener);
  }

  void inputByte(uint8_t b, const Listener& emit);
  bool finishInput(const Listener& emit);
private:
  struct Key {
    ARDPORTAL_NO_THROW_ALLOCATION
    uint64_t hash;
    Key* next;
  };
  struct Frame {
    ARDPORTAL_NO_THROW_ALLOCATION
    Key* keys = nullptr;
    uint16_t items = 0;
    uint8_t state = 0;
    bool object = false;
    void clear() {
      while (keys) {
        Key* next = keys->next;
        delete keys;
        keys = next;
      }
      items = 0;
    }
  };
  Frame _frames[17];
  std::unique_ptr<Frame[]> _overflow;
  uint8_t _maxDepth = 16;
  bool _checkKeys = true;
  Frame& frame(unsigned index) {
    return index < 17 ? _frames[index] : _overflow[index - 17];
  }
  bool fail(Error reason) {
    if (!failed) error = reason;
    failed = true;
    outOfMemory = error == Error::Memory;
    return false;
  }
  uint8_t _depth = 0, _token = 0, _number = 0, _literalAt = 0;
  uint8_t _escape = 0, _hex = 0, _utfLeft = 0;
  uint16_t _unicode = 0, _high = 0;
  uint32_t _utf = 0, _utfMin = 0;
  uint64_t _hash = 0;
  bool _done = false, _key = false;
  const char* _literal = nullptr;
public:
  explicit ArdJsonStream(unsigned maxDepth = 16, bool checkKeys = true);
  ArdJsonStream(const ArdJsonStream&) = delete;
  ArdJsonStream& operator=(const ArdJsonStream&) = delete;
  ~ArdJsonStream();
  void reset(unsigned maxDepth = 16, bool checkKeys = true);
private:
  void clearFrames();
  bool numberEnd() const;
  bool numberByte(uint8_t b);
  void end(bool object, const Listener& emit);
  void beginString(const Listener& emit);
  bool output(uint8_t b, const Listener& emit);
  bool codepoint(uint32_t cp, const Listener& emit);
  void stringByte(uint8_t b, const Listener& emit);
};
