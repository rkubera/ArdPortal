// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#include "JsonStream.h"
#include "JsonUtf8.h"

ArdJsonStream::ArdJsonStream(unsigned maxDepth, bool checkKeys)
  : _maxDepth(maxDepth > 32 ? 32 : maxDepth), _checkKeys(checkKeys) {}

ArdJsonStream::~ArdJsonStream() {
  clearFrames();
}

void ArdJsonStream::clearFrames() {
  for (auto& f : _frames) f.clear();
  if (_overflow)
    for (unsigned i = 0; i < 16; ++i) _overflow[i].clear();
  _overflow.reset();
}

void ArdJsonStream::inputByte(uint8_t b, const Listener& emit) {
  if (failed) return;
  if (_token == 1) {
    stringByte(b, emit);
    return;
  }
  if (_token == 2) {
    if (numberByte(b)) {
      if (!emit(Event::NumberByte, b, _depth)) failed = true;
      return;
    }
    if (!numberEnd()) {
      fail(_number == 3 ? Error::Fraction : _number >= 5 ? Error::Exponent
                                                         : Error::Number);
      return;
    }
    _token = 0;
    if (!emit(Event::EndNumber, 0, _depth)) {
      failed = true;
      return;
    }
  } else if (_token == 3) {
    if (b != uint8_t(_literal[_literalAt++])) {
      fail(Error::Literal);
      return;
    }
    if (!_literal[_literalAt]) _token = 0;
    return;
  }
  if (b == ' ' || b == '\t' || b == '\r' || b == '\n') return;
  if (_depth) {
    Frame& f = frame(_depth - 1);
    if (f.state == 0 || f.state == 1) {  // Object key, optionally empty object.
      if (b == '}' && f.state == 0) {
        end(true, emit);
        return;
      }
      if (b != '"') {
        fail(Error::String);
        return;
      }
      _key = true;
      _hash = UINT64_C(14695981039346656037);
      beginString(emit);
      return;
    }
    if (f.state == 2) {
      if (b != ':') {
        fail(Error::Member);
        return;
      }
      f.state = 3;
      return;
    }
    if (f.state == 4) {
      if (b == '}' && f.object) {
        end(true, emit);
        return;
      }
      if (b == ']' && !f.object) {
        end(false, emit);
        return;
      }
      if (b != ',') {
        fail(Error::Separator);
        return;
      }
      f.state = f.object ? 1 : 3;
      return;
    }
    if (f.state == 5 && b == ']') {
      end(false, emit);
      return;
    }
    if (++f.items > 1024) {
      fail(Error::Container);
      return;
    }
    f.state = 4;
  } else {
    if (_done) {
      fail(Error::Trailing);
      return;
    }
    _done = true;
  }
  if (_depth > _maxDepth) {
    fail(Error::Depth);
    return;
  }
  if (b == '{' || b == '[') {
    if (_depth >= 17 && !_overflow) {
      _overflow.reset(new (std::nothrow) Frame[16]);
      if (!_overflow) {
        fail(Error::Memory);
        return;
      }
    }
    if (!emit(b == '{' ? Event::Object : Event::Array, 0, _depth)) {
      failed = true;
      return;
    }
    Frame& f = frame(_depth++);
    f.clear();
    f.object = b == '{';
    f.state = f.object ? 0 : 5;
  } else if (b == '"') {
    _key = false;
    beginString(emit);
  } else if (b == '-' || (b >= '0' && b <= '9')) {
    _token = 2;
    _number = b == '-' ? 0 : b == '0' ? 1
                                      : 2;
    if (!emit(Event::Number, 0, _depth) || !emit(Event::NumberByte, b, _depth)) failed = true;
  } else if (b == 't' || b == 'f' || b == 'n') {
    _literal = b == 't' ? "true" : b == 'f' ? "false"
                                            : "null";
    _literalAt = 1;
    _token = 3;
    if (!emit(Event::Literal, b, _depth)) failed = true;
  } else fail(Error::Value);
}

bool ArdJsonStream::finishInput(const Listener& emit) {
  if (_token == 2 && numberEnd()) {
    _token = 0;
    if (!emit(Event::EndNumber, 0, _depth)) failed = true;
  }
  if (!failed && (_token || _depth || !_done)) {
    Error reason = Error::Value;
    if (_token == 2) reason = _number == 3 ? Error::Fraction : _number >= 5 ? Error::Exponent
                                                                            : Error::Number;
    else if (_token == 3) reason = Error::Literal;
    else if (_token == 1) reason = _high ? Error::MissingSurrogate : _hex    ? Error::Unicode
                                                                   : _escape ? Error::UnfinishedEscape
                                                                             : Error::UnterminatedString;
    else if (_depth) reason = frame(_depth - 1).state == 2 ? Error::Member : frame(_depth - 1).state == 4 ? Error::Separator
                                                                           : frame(_depth - 1).object     ? Error::String
                                                                                                          : Error::Value;
    fail(reason);
  }
  return !failed;
}

void ArdJsonStream::reset(unsigned maxDepth, bool checkKeys) {
  clearFrames();
  _maxDepth = maxDepth > 32 ? 32 : maxDepth;
  _checkKeys = checkKeys;
  error = Error::None;
  _depth = _token = _number = _literalAt = _escape = _hex = _utfLeft = 0;
  _unicode = _high = 0;
  _utf = _utfMin = 0;
  _hash = 0;
  _done = _key = failed = outOfMemory = false;
  _literal = nullptr;
}

bool ArdJsonStream::numberEnd() const {
  return (0x96U >> _number) & 1U;
}

bool ArdJsonStream::numberByte(uint8_t b) {
  if (b >= '0' && b <= '9') {
    if (!_number) _number = b == '0' ? 1 : 2;
    else if (_number == 1) fail(_depth ? Error::Separator : Error::Trailing);
    else if (_number == 3) _number = 4;
    else if (_number >= 5) _number = 7;
    return true;
  }
  if (b == '.' && (_number == 1 || _number == 2)) _number = 3;
  else if ((b == 'e' || b == 'E') && (_number == 1 || _number == 2 || _number == 4)) _number = 5;
  else if ((b == '-' || b == '+') && _number == 5) _number = 6;
  else return false;
  return true;
}

void ArdJsonStream::end(bool object, const Listener& emit) {
  if (!_depth || frame(_depth - 1).object != object) {
    failed = true;
    return;
  }
  frame(--_depth).clear();
  if (!emit(object ? Event::EndObject : Event::EndArray, 0, _depth)) failed = true;
}

void ArdJsonStream::beginString(const Listener& emit) {
  _token = 1;
  _escape = _hex = _utfLeft = 0;
  _unicode = _high = 0;
  if (!emit(_key ? Event::Key : Event::String, 0, _depth)) failed = true;
}

bool ArdJsonStream::output(uint8_t b, const Listener& emit) {
  if (!ArdUtf8::consume(b, _utfLeft, _utf, _utfMin)) return false;
  if (_key && _checkKeys) {
    _hash ^= b;
    _hash *= UINT64_C(1099511628211);
  }
  return emit(_key ? Event::KeyByte : Event::StringByte, b, _depth);
}

bool ArdJsonStream::codepoint(uint32_t cp, const Listener& emit) {
  uint8_t bytes[4];
  unsigned count = cp < 0x80 ? 1 : cp < 0x800   ? 2
                                 : cp < 0x10000 ? 3
                                                : 4;
  for (unsigned i = count; i > 1; --i) {
    bytes[i - 1] = 0x80 | (cp & 63);
    cp >>= 6;
  }
  bytes[0] = uint8_t(cp) | (count == 1 ? 0 : uint8_t(0xffU << (8 - count)));
  for (unsigned i = 0; i < count; ++i)
    if (!output(bytes[i], emit)) return false;
  return true;
}

void ArdJsonStream::stringByte(uint8_t b, const Listener& emit) {
  if (_hex) {
    int digit = b >= '0' && b <= '9' ? b - '0' : b >= 'a' && b <= 'f' ? b - 'a' + 10
                                               : b >= 'A' && b <= 'F' ? b - 'A' + 10
                                                                      : -1;
    if (digit < 0) {
      fail(Error::Unicode);
      return;
    }
    _unicode = (_unicode << 4) | digit;
    if (--_hex) return;
    _escape = 0;
    if (_high) {
      if (_unicode < 0xdc00 || _unicode > 0xdfff) {
        fail(Error::LowSurrogate);
        return;
      }
      uint32_t cp = 0x10000 + ((uint32_t(_high) - 0xd800) << 10) + _unicode - 0xdc00;
      _high = 0;
      if (!codepoint(cp, emit)) fail(Error::Utf8);
    } else if (_unicode >= 0xd800 && _unicode <= 0xdbff) _high = _unicode;
    else if (_unicode >= 0xdc00 && _unicode <= 0xdfff) fail(Error::UnpairedSurrogate);
    else if (!codepoint(_unicode, emit)) fail(Error::Utf8);
    return;
  }
  if (_escape) {
    _escape = 0;
    if (_high && b != 'u') {
      fail(Error::MissingSurrogate);
      return;
    }
    if (b == 'u') {
      _hex = 4;
      _unicode = 0;
      return;
    }
    uint8_t decoded = b == 'b' ? 8 : b == 'f' ? 12
                                   : b == 'n' ? 10
                                   : b == 'r' ? 13
                                   : b == 't' ? 9
                                              : b;
    if (!(b == 'b' || b == 'f' || b == 'n' || b == 'r' || b == 't' || b == '"' || b == '\\' || b == '/')) fail(Error::Escape);
    else if (!output(decoded, emit)) fail(Error::Utf8);
    return;
  }
  if (_high && b != '\\') {
    fail(Error::MissingSurrogate);
    return;
  }
  if (b == '\\') {
    _escape = 1;
    return;
  }
  if (b == '"') {
    if (_utfLeft || _high) {
      fail(_high ? Error::MissingSurrogate : Error::Utf8);
      return;
    }
    _token = 0;
    if (_key) {
      Frame& f = frame(_depth - 1);
      if (_checkKeys) {
        for (Key* k = f.keys; k; k = k->next)
          if (k->hash == _hash) {
            fail(Error::Duplicate);
            return;
          }
        Key* key = new (std::nothrow) Key;
        if (!key) {
          fail(Error::Memory);
          return;
        }
        key->hash = _hash;
        key->next = f.keys;
        f.keys = key;
      }
      f.state = 2;
    }
    if (!emit(_key ? Event::EndKey : Event::EndString, 0, _depth)) failed = true;
    return;
  }
  if (b < 32) fail(Error::Control);
  else if (!output(b, emit)) fail(Error::Utf8);
}
