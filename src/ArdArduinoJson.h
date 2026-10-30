// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <ArduinoJson.h>
#if ARDUINOJSON_VERSION_MAJOR != 7
#error ARDPORTAL_USE_ARDUINOJSON requires ArduinoJson 7
#endif

// The public value facade remains shared: references, undefined patch deletions,
// exact numeric lexemes and ownership retain their ArdPortal semantics.
namespace ArdJSON {
struct ArduinoJsonCheck {
  const Limits& limits;
  size_t nodes = 0, stringBytes = 0;
  const char* error = nullptr;
  bool event(ArdJsonStream::Event event, unsigned depth) {
    using E = ArdJsonStream::Event;
    if (event == E::Key || event == E::String) stringBytes = 0;
    if ((event == E::KeyByte || event == E::StringByte) && ++stringBytes > limits.maxStringBytes) error = ARD_LANG_NATIVE_STRING_LIMIT;
    if (event == E::Object || event == E::Array || event == E::String || event == E::Number || event == E::Literal) {
      if (++nodes > limits.maxNodes) error = ARD_LANG_NATIVE_NODE_LIMIT;
      if (depth > limits.maxDepth || depth > 32) error = ARD_LANG_NATIVE_DEPTH_LIMIT;
    }
    return !error;
  }
};

class ArduinoParser {
public:
  ArduinoParser(const String& input, const Limits& limits)
    : _input(input), _limits(limits) {}
  JSONVar parse() {
    _offset = _nodes = 0;
    _error = nullptr;
    if (_input.length() > _limits.maxInputBytes) return fail(ARD_LANG_NATIVE_INPUT_LIMIT);
    // ArduinoJson accepts duplicate keys, trailing bytes and some non-JSON
    // numbers. Keep the same strict syntax contract as the native backend.
    ArdJsonStream syntax(_limits.maxDepth, true);
    ArduinoJsonCheck check{ _limits };
    const ArdJsonStream::Listener listener{ &check, [](void* context, ArdJsonStream::Event event, uint8_t, unsigned depth) {
                                             return static_cast<ArduinoJsonCheck*>(context)->event(event, depth);
                                           } };
    ArdCooperativeBudget budget;
    for (size_t i = 0; i < _input.length(); ++i) {
      if ((i & 127) == 0) budget.checkpoint();
      syntax.inputByte(uint8_t(_input[i]), listener);
      if (syntax.failed) {
        _offset = i;
        return fail(check.error ? check.error : ARD_LANG_NATIVE_EXPECTED_VALUE);
      }
    }
    if (!syntax.finishInput(listener)) return fail(ARD_LANG_NATIVE_EXPECTED_VALUE);
    ArduinoJson::JsonDocument document;
    auto error = ArduinoJson::deserializeJson(document, _input.c_str(), _input.length(),
                                              ArduinoJson::DeserializationOption::NestingLimit(33));
    if (error) return fail(error == ArduinoJson::DeserializationError::NoMemory ? ARD_LANG_NATIVE_OUT_OF_MEMORY : ARD_LANG_NATIVE_EXPECTED_VALUE);
    _offset = 0;
    JSONVar value = import(document.as<ArduinoJson::JsonVariantConst>(), 0);
    if (_error || !value.isValid()) return fail(_error ? _error : ARD_LANG_NATIVE_OUT_OF_MEMORY);
    _offset = _input.length();
    return value;
  }
  const char* error() const {
    return _error;
  }
  size_t offset() const {
    return _offset;
  }
  size_t nodes() const {
    return _nodes;
  }
private:
  const String& _input;
  const Limits& _limits;
  size_t _offset = 0, _nodes = 0;
  const char* _error = nullptr;
  JSONVar fail(const char* error) {
    _error = error;
    return JSONVar();
  }
  void whitespace() {
    while (_offset < _input.length() && (_input[_offset] == ' ' || _input[_offset] == '\n' || _input[_offset] == '\r' || _input[_offset] == '\t')) ++_offset;
  }
  void skipString() {
    ++_offset;
    while (_offset < _input.length()) {
      char c = _input[_offset++];
      if (c == '\\') ++_offset;
      else if (c == '"') break;
    }
  }
  JSONVar import(ArduinoJson::JsonVariantConst source, unsigned depth) {
    if (depth > _limits.maxDepth || depth > 32) return fail(ARD_LANG_NATIVE_DEPTH_LIMIT);
    if (++_nodes > _limits.maxNodes) return fail(ARD_LANG_NATIVE_NODE_LIMIT);
    whitespace();
    if (source.is<ArduinoJson::JsonObjectConst>()) {
      JSONVar result = JSONVar::object();
      ++_offset;
      for (auto member : source.as<ArduinoJson::JsonObjectConst>()) {
        whitespace();
        skipString();
        whitespace();
        ++_offset;
        auto key = member.key();
        if (key.size() > _limits.maxStringBytes) return fail(ARD_LANG_NATIVE_STRING_LIMIT);
        String name;
        name.concat(key.c_str(), key.size());
        if (name.length() != key.size()) return fail(ARD_LANG_NATIVE_OUT_OF_MEMORY);
        result[name] = import(member.value(), depth + 1);
        if (_error) return JSONVar();
        whitespace();
        if (_input[_offset] == ',') ++_offset;
      }
      ++_offset;
      return result;
    }
    if (source.is<ArduinoJson::JsonArrayConst>()) {
      JSONVar result = JSONVar::array();
      ++_offset;
      for (auto member : source.as<ArduinoJson::JsonArrayConst>()) {
        if (!result.push(import(member, depth + 1))) return fail(ARD_LANG_NATIVE_OUT_OF_MEMORY);
        if (_error) return JSONVar();
        whitespace();
        if (_input[_offset] == ',') ++_offset;
      }
      ++_offset;
      return result;
    }
    if (source.is<ArduinoJson::JsonString>()) {
      auto text = source.as<ArduinoJson::JsonString>();
      if (text.size() > _limits.maxStringBytes) return fail(ARD_LANG_NATIVE_STRING_LIMIT);
      String value;
      value.concat(text.c_str(), text.size());
      skipString();
      return value.length() == text.size() ? JSONVar(value) : fail(ARD_LANG_NATIVE_OUT_OF_MEMORY);
    }
    size_t start = _offset;
    while (_offset < _input.length()) {
      char c = _input[_offset];
      if (c == ',' || c == ']' || c == '}' || c == ' ' || c == '\n' || c == '\r' || c == '\t') break;
      ++_offset;
    }
    if (_input[start] == 'n') return JSONVar(nullptr);
    if (_input[start] == 't' || _input[start] == 'f') return JSONVar(_input[start] == 't');
    JSONVar result;
    result._type = JSONVar::Type::Number;
    result._text = _input.substring(start, _offset);
    if (result._text.length() != _offset - start) return fail(ARD_LANG_NATIVE_OUT_OF_MEMORY);
    return result;
  }
};

// The optional backend buffers a bounded document before ArduinoJson parses it.
// Native streaming remains available explicitly as NativeStreamParser.
class ArduinoStreamParser {
public:
  ARDPORTAL_NO_THROW_ALLOCATION
  explicit ArduinoStreamParser(const Limits& limits = Limits())
    : _limits(limits), _syntax(limits.maxDepth, true), _check{ _limits } {}
  void reset(const Limits& limits = Limits()) {
    _limits = limits;
    _input = String();
    _error = nullptr;
    _nodes = _offset = 0;
    _syntax.reset(limits.maxDepth, true);
    _check.nodes = _check.stringBytes = 0;
    _check.error = nullptr;
  }
  bool input(size_t offset, const uint8_t* bytes, size_t length) {
    if (_error) return false;
    if (offset != _input.length() || length > _limits.maxInputBytes || offset > _limits.maxInputBytes - length) {
      _error = ARD_LANG_NATIVE_INPUT_LIMIT;
      return false;
    }
    const ArdJsonStream::Listener listener{ &_check, [](void* context, ArdJsonStream::Event event, uint8_t, unsigned depth) {
                                             return static_cast<ArduinoJsonCheck*>(context)->event(event, depth);
                                           } };
    ArdCooperativeBudget budget;
    for (size_t i = 0; i < length; ++i) {
      if ((i & 127) == 0) budget.checkpoint();
      _syntax.inputByte(bytes[i], listener);
      if (_syntax.failed) {
        _error = _check.error ? _check.error : ARD_LANG_NATIVE_EXPECTED_VALUE;
        _offset = offset + i;
        return false;
      }
    }
    _input.concat(reinterpret_cast<const char*>(bytes), length);
    if (_input.length() != offset + length) {
      _error = ARD_LANG_NATIVE_OUT_OF_MEMORY;
      return false;
    }
    _offset = _input.length();
    return true;
  }
  JSONVar take() {
    if (_error) return JSONVar();
    ArduinoParser parser(_input, _limits);
    JSONVar result = parser.parse();
    _error = parser.error();
    _offset = parser.offset();
    _nodes = parser.nodes();
    _input = String();
    return result;
  }
  const char* error() const {
    return _error;
  }
  size_t offset() const {
    return _offset;
  }
  size_t nodes() const {
    return _nodes;
  }
private:
  Limits _limits;
  ArdJsonStream _syntax;
  ArduinoJsonCheck _check;
  String _input;
  const char* _error = nullptr;
  size_t _nodes = 0, _offset = 0;
};
struct ArduinoAPI : NativeAPI {
  JSONVar parse(const String& text, String* error = nullptr, const Limits& limits = Limits()) const {
    ArduinoParser parser(text, limits);
    JSONVar value = parser.parse();
    if (error) *error = parser.error() ? String(parser.error()) + ARD_LANG_NATIVE_AT_BYTE + String(parser.offset()) : String();
    return value;
  }
  String stringify(const JSONVar& value, bool pretty = false, String* error = nullptr, const Limits& limits = Limits()) const {
    // Shared validation keeps every backend's limits and error contract identical.
    Writer validator(limits, false);
    if (!validator.measure(value)) {
      if (error) *error = validator.error();
      return String();
    }
    ArduinoJson::JsonDocument document;
    if (!exportValue(value, document.to<ArduinoJson::JsonVariant>()) || document.overflowed()) {
      if (error) *error = ARD_LANG_NATIVE_OUT_OF_MEMORY;
      return String();
    }
    Output output(limits);
    if (pretty) ArduinoJson::serializeJsonPretty(document, output);
    else ArduinoJson::serializeJson(document, output);
    if (error) *error = output.error ? output.error : "";
    return output.failed ? String() : std::move(output.text);
  }
private:
  static bool exportValue(const JSONVar& value, ArduinoJson::JsonVariant target) {
    using T = JSONVar::Type;
    switch (value._type) {
      case T::Null: return target.set(nullptr);
      case T::Boolean: return target.set(value._boolean);
      case T::Number: return target.set(ArduinoJson::serialized(value._text.c_str(), value._text.length()));
      case T::String: return target.set(ArduinoJson::JsonString(value._text.c_str(), value._text.length()));
      case T::Object:
        {
          auto object = target.to<ArduinoJson::JsonObject>();
          for (const auto* item = value._children.get(); item; item = item->next.get()) {
            String key = item->key.text();
            if (key.length() != item->key.length() || !exportValue(item->value, object[ArduinoJson::JsonString(key.c_str(), key.length())].to<ArduinoJson::JsonVariant>())) return false;
          }
          return !object.isNull();
        }
      case T::Array:
        {
          auto array = target.to<ArduinoJson::JsonArray>();
          for (const auto* item = value._children.get(); item; item = item->next.get())
            if (!exportValue(item->value, array.add<ArduinoJson::JsonVariant>())) return false;
          return !array.isNull();
        }
      default: return false;
    }
  }
  // Normalize ArduinoJson's short control escapes and CRLF pretty output to
  // the existing wire format. Slice/patch writers can share the native walker.
  struct Output {
    const Limits& limits;
    String text;
    bool quoted = false, escaped = false, failed = false;
    const char* error = nullptr;
    explicit Output(const Limits& value)
      : limits(value) {}
    bool append(char c) {
      const size_t before = text.length();
      if (before >= limits.maxOutputBytes) {
        error = ARD_LANG_NATIVE_OUTPUT_LIMIT;
        failed = true;
        return false;
      }
      text += c;
      if (text.length() != before + 1) {
        error = ARD_LANG_NATIVE_OUT_OF_MEMORY;
        failed = true;
      }
      return !failed;
    }
    bool unicode(uint8_t c) {
      const char* hex = "0123456789abcdef";
      return append('\\') && append('u') && append('0') && append('0') && append(hex[c >> 4]) && append(hex[c & 15]);
    }
    size_t write(uint8_t c) {
      if (failed) return 0;
      if (escaped) {
        escaped = false;
        const char* shortEscapes = "bfnrt";
        const char* found = strchr(shortEscapes, c);
        if (found) {
          const uint8_t codes[] = { 8, 12, 10, 13, 9 };
          unicode(codes[found - shortEscapes]);
        } else {
          append('\\');
          append(char(c));
        }
      } else if (quoted && c == '\\') escaped = true;
      else {
        if (c == '"') quoted = !quoted;
        if (quoted && (c < 32 || (limits.escapeHtml && (c == '<' || c == '>' || c == '&')))) unicode(c);
        else if (quoted || c != '\r') append(char(c));
      }
      return failed ? 0 : 1;
    }
    size_t write(const uint8_t* data, size_t size) {
      size_t i = 0;
      while (i < size && write(data[i])) ++i;
      return i;
    }
  };
};
}
