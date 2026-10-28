// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#include "ArdJSON.h"

namespace ArdJSON {
StreamParser::StreamParser(const Limits& limits) : _syntax(limits.maxDepth, false), _limits(limits) {}

void StreamParser::reset(const Limits& limits) {
  _limits = limits;
  _syntax.reset(limits.maxDepth, false); // The DOM checks exact keys; no second key table.
  _root = JSONVar(); _key = String();
  for (auto& value : _containers) value = nullptr;
  _current = nullptr; _bytes = _nodes = _offset = 0; _error = nullptr;
  _budget = ArdCooperativeBudget();
}
bool StreamParser::fail(const char* error) {
  if (!_error) _error = error;
  return false;
}
const char* StreamParser::syntaxError(ArdJsonStream::Error error) {
  using E = ArdJsonStream::Error;
  switch (error) {
    case E::String: return ARD_LANG_NATIVE_EXPECTED_STRING;
    case E::Member: return ARD_LANG_NATIVE_EXPECTED_OBJECT_MEMBER;
    case E::Separator: return ARD_LANG_NATIVE_EXPECTED_COMMA_OR_CLOSING_BRACKET;
    case E::Trailing: return ARD_LANG_NATIVE_TRAILING_DATA;
    case E::Number: return ARD_LANG_NATIVE_INVALID_NUMBER;
    case E::Fraction: return ARD_LANG_NATIVE_MISSING_FRACTION;
    case E::Exponent: return ARD_LANG_NATIVE_MISSING_EXPONENT;
    case E::Literal: return ARD_LANG_NATIVE_INVALID_LITERAL;
    case E::Escape: return ARD_LANG_NATIVE_INVALID_ESCAPE;
    case E::Unicode: return ARD_LANG_NATIVE_INVALID_UNICODE_ESCAPE;
    case E::LowSurrogate: return ARD_LANG_NATIVE_INVALID_LOW_SURROGATE;
    case E::UnpairedSurrogate: return ARD_LANG_NATIVE_UNPAIRED_SURROGATE;
    case E::MissingSurrogate: return ARD_LANG_NATIVE_MISSING_LOW_SURROGATE;
    case E::Utf8: return ARD_LANG_NATIVE_INVALID_UTF_8;
    case E::Control: return ARD_LANG_NATIVE_UNESCAPED_CONTROL_CHARACTER;
    case E::UnfinishedEscape: return ARD_LANG_NATIVE_UNFINISHED_ESCAPE;
    case E::UnterminatedString: return ARD_LANG_NATIVE_UNTERMINATED_STRING;
    case E::Depth: return ARD_LANG_NATIVE_DEPTH_LIMIT;
    case E::Container: return ARD_LANG_NATIVE_CONTAINER_LIMIT;
    case E::Duplicate: return ARD_LANG_NATIVE_DUPLICATE_KEY;
    case E::Memory: return ARD_LANG_NATIVE_OUT_OF_MEMORY;
    default: return ARD_LANG_NATIVE_EXPECTED_VALUE;
  }
}
bool StreamParser::dispatch(void* context, ArdJsonStream::Event event, uint8_t b, unsigned depth) {
  return static_cast<StreamParser*>(context)->consume(event, b, depth);
}
bool StreamParser::input(size_t offset, const uint8_t* bytes, size_t length) {
  if (_error) return false;
  if (offset != _bytes || length > _limits.maxInputBytes || _bytes > _limits.maxInputBytes - length) return fail(ARD_LANG_NATIVE_INPUT_LIMIT);
  const ArdJsonStream::Listener emit{this, dispatch};
  for (size_t i = 0; i < length; ++i) {
    _offset = _bytes + i;
    if ((_offset & 127) == 0) _budget.checkpoint();
    _syntax.inputByte(bytes[i], emit);
    if (_syntax.failed) return fail(syntaxError(_syntax.error));
  }
  _offset = _bytes += length;
  return true;
}
JSONVar StreamParser::take() {
  const ArdJsonStream::Listener emit{this, dispatch};
  if (!_error && !_syntax.finishInput(emit)) fail(syntaxError(_syntax.error));
  if (_error || !_root.isValid()) { _root = JSONVar(); return JSONVar(); }
  return std::move(_root);
}
bool StreamParser::append(String& text, uint8_t b, bool stringLimit) {
  if (stringLimit && text.length() >= _limits.maxStringBytes) return fail(ARD_LANG_NATIVE_STRING_LIMIT);
  const size_t before = text.length();
  text += char(b);
  return text.length() == before + 1 || fail(ARD_LANG_NATIVE_OUT_OF_MEMORY);
}
JSONVar* StreamParser::value(unsigned depth) {
  if (depth > _limits.maxDepth || depth > 32) { fail(ARD_LANG_NATIVE_DEPTH_LIMIT); return nullptr; }
  if (++_nodes > _limits.maxNodes) { fail(ARD_LANG_NATIVE_NODE_LIMIT); return nullptr; }
  if (!depth) return &_root;
  JSONVar* parent = _containers[depth - 1];
  if (!parent || parent->_size >= MaxContainerElements) { fail(ARD_LANG_NATIVE_CONTAINER_LIMIT); return nullptr; }
  JSONVar::Member* member = parent->_type == JSONVar::Type::Object ? parent->append(_key) : parent->append(String());
  _key = String();
  if (!member) { fail(ARD_LANG_NATIVE_OUT_OF_MEMORY); return nullptr; }
  return &member->value;
}
bool StreamParser::consume(ArdJsonStream::Event event, uint8_t b, unsigned depth) {
  using E = ArdJsonStream::Event;
  if (event == E::Key) { _key = String(); return true; }
  if (event == E::KeyByte) return append(_key, b);
  if (event == E::EndKey) {
    if (_containers[depth - 1]->hasOwnProperty(_key)) return fail(ARD_LANG_NATIVE_DUPLICATE_KEY);
  } else if (event == E::Object || event == E::Array) {
    JSONVar* target = value(depth);
    if (!target) return false;
    target->_type = event == E::Object ? JSONVar::Type::Object : JSONVar::Type::Array;
    _containers[depth] = target;
  } else if (event == E::EndObject || event == E::EndArray) _containers[depth] = nullptr;
  else if (event == E::String || event == E::Number) {
    _current = value(depth);
    if (!_current) return false;
    _current->_type = event == E::String ? JSONVar::Type::String : JSONVar::Type::Number;
  } else if (event == E::StringByte || event == E::NumberByte) {
    if (!_current) return fail(ARD_LANG_NATIVE_EXPECTED_VALUE);
    return append(_current->_text, b, event == E::StringByte);
  } else if (event == E::EndString || event == E::EndNumber) _current = nullptr;
  else if (event == E::Literal) {
    JSONVar* target = value(depth);
    if (!target) return false;
    if (b == 'n') *target = nullptr;
    else *target = b == 't';
  }
  return !_error;
}
JSONVar Parser::parse() {
  _offset = _nodes = 0; _error = nullptr;
  if (_input.length() > _limits.maxInputBytes) { _error = ARD_LANG_NATIVE_INPUT_LIMIT; return JSONVar(); }
  StreamParser parser(_limits);
  parser.input(0, reinterpret_cast<const uint8_t*>(_input.c_str()), _input.length());
  JSONVar value = parser.take();
  _error = parser.error(); _offset = parser.offset(); _nodes = parser.nodes();
  return value;
}
}
