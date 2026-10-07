// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>
#include "CooperativeBudget.h"
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <cstdlib>
#include <cerrno>
#include <cmath>
#include <limits>

// Standalone JSON DOM and strict parser. No portal, filesystem or third-party dependencies.
namespace ArdJSON {
constexpr size_t MaxContainerElements = 1024;
struct Limits {
  size_t maxInputBytes = 32768;
  size_t maxOutputBytes = 32768;
  size_t maxStringBytes = 8192;
  size_t maxNodes = 256;
  bool escapeHtml = false; // Escape HTML delimiters when embedding JSON in a script element.
  unsigned maxDepth = 16; // Parser and writer also enforce a hard ceiling of 32.
};
class Parser;
class Writer;
class ObjectKey;
class JSONVar {
public:
  enum class Type { Undefined, Null, Boolean, Number, String, Object, Array };
  JSONVar() = default;
  JSONVar(std::nullptr_t) : _type(Type::Null) {}
  JSONVar(bool value) : _type(Type::Boolean), _boolean(value) {}
  JSONVar(const char* value) : _type(value ? Type::String : Type::Null), _text(value ? value : "") { _failed=value && _text.length()!=strlen(value); }
  JSONVar(const String& value) : _type(Type::String), _text(value) { _failed=!equal(_text,value); }
  JSONVar(double value);
  template<class T, typename std::enable_if<std::is_integral<T>::value && !std::is_same<T,bool>::value,int>::type = 0>
  JSONVar(T value) : _type(Type::Number) {
    const bool negative = std::is_signed<T>::value && static_cast<int64_t>(value) < 0;
    const uint64_t bits = static_cast<uint64_t>(value);
    assignInteger(negative ? uint64_t(0) - bits : bits, negative);
  }
  JSONVar(const JSONVar& other);
  JSONVar(JSONVar&& other) noexcept;
  ~JSONVar();
  JSONVar& operator=(JSONVar other) { swap(other); return *this; }
  void swap(JSONVar& other) noexcept;
  static JSONVar object();
  static JSONVar array();
  Type type() const { return _type; }
  bool isUndefined() const { return _type == Type::Undefined; }
  bool isNull() const { return _type == Type::Null; }
  bool isValid() const;
  bool isInteger() const;
  bool toInteger(int64_t& value) const;
  bool toUnsignedInteger(uint64_t& value) const {
    if (_type != Type::Number || !_text.length()) return false;
    uint64_t n = 0;
    for (size_t i=0; i<_text.length(); ++i) {
      char c=_text[i]; if (c<'0' || c>'9' || n>(UINT64_MAX-uint64_t(c-'0'))/10) return false;
      n=n*10+uint64_t(c-'0');
    }
    value=n; return true;
  }
  bool toDouble(double& value) const;
  String asString() const { return _type == Type::String ? _text : String(); }
  bool asBool() const { return _type == Type::Boolean && _boolean; }
  double asDouble() const { double value = 0; toDouble(value); return value; }
  explicit operator String() const { return asString(); }
  explicit operator bool() const { return asBool(); }
  explicit operator double() const { return asDouble(); }
  explicit operator int() const {
    int64_t value;
    return toInteger(value) && value >= std::numeric_limits<int>::min() && value <= std::numeric_limits<int>::max() ? static_cast<int>(value) : 0;
  }
  size_t length() const { return _size; }
  bool hasOwnProperty(const String& key) const;
  bool hasOwnProperty(const char* key) const;
  JSONVar keys() const;
  JSONVar& operator[](const char* key);
  const JSONVar& operator[](const char* key) const;
  JSONVar& operator[](const String& key);
  const JSONVar& operator[](const String& key) const;
  JSONVar& operator[](size_t index);
  const JSONVar& operator[](size_t index) const;
  JSONVar& operator[](int index);
  const JSONVar& operator[](int index) const;
  bool push(const JSONVar& value);
  bool tryPush(const JSONVar& value); // Transactional append: failure leaves the array intact.
  bool remove(const String& key);
  bool remove(size_t index);
private:
  struct Member;
  Type _type = Type::Undefined;
  bool _boolean = false, _failed = false;
  String _text; // Strings or exact number lexemes: large integers never lose precision.
  std::unique_ptr<Member> _children;
  size_t _size = 0;
  static bool equal(const String& a,const String& b) { return a.length()==b.length() && !memcmp(a.c_str(),b.c_str(),a.length()); }
  template<class = void>
#if defined(__GNUC__)
  __attribute__((noinline))
#endif
  void assignInteger(uint64_t magnitude, bool negative);
  static const JSONVar& missing();
  JSONVar& failure();
  Member* append(const String& key);
  template<class = void>
#if defined(__GNUC__)
  __attribute__((noinline))
#endif
  Member* findMember(const char* key) const;
  template<class Key> Member* appendKey(const Key& key);
  void clear();
  friend class Parser;
  friend class Writer;
};
// Common immutable object keys share flash storage. Other keys own exact bytes,
// including embedded NULs; array members need no key allocation.
class ObjectKey {
public:
  ObjectKey() = default;
  ~ObjectKey() { if(!flash()) delete[] _bytes; }
  ObjectKey(const ObjectKey&) = delete;
  ObjectKey& operator=(const ObjectKey&) = delete;
  bool assign(const String& key) {
    if(key.length()>=FlashFlag) return false;
    static const char dictionary[] PROGMEM =
      "id\0type\0name\0names\0en\0pl\0order\0fields\0default\0persist\0extended\0readonly\0"
      "min\0max\0step\0options\0value\0icon\0modes\0fan_modes\0controls\0ha\0key\0command\0"
      "payload\0label\0on\0off\0state\0temperature\0current_temperature\0mode\0fan_mode\0"
      "humidity\0current_humidity\0brightness\0color\0r\0g\0b\0percentage\0preset_mode\0"
      "oscillating\0direction\0position\0tilt\0tone\0duration\0volume_level\0fan_speed\0"
      "version\0ssid\0password\0host\0port\0user\0mqttPassword\0mqttTls\0caCert\0apName\0"
      "apPassword\0deviceName\0config\0app\0journal\0generation\0crc32\0data\0";
    const char* selected=nullptr;
    for(size_t offset=0;offset<sizeof(dictionary)-1;) {
      const char* candidate=dictionary+offset;size_t length=strlen_P(candidate);
      if(length==key.length() && strcmp_P(key.c_str(),candidate)==0) {selected=candidate;break;}
      offset+=length+1;
    }
    if(!flash()) delete[] _bytes;
    _bytes=nullptr;_size=0;
    if(!key.length()) return true;
    if(selected) {_bytes=selected;_size=key.length()|FlashFlag;return true;}
    char* bytes=new(std::nothrow) char[key.length()+1];if(!bytes) return false;
    memcpy(bytes,key.c_str(),key.length()+1);_bytes=bytes;_size=key.length();return true;
  }
  bool assign(const ObjectKey& other) {
    if(this==&other) return true;
    if(!flash()) delete[] _bytes;
    _bytes=nullptr;_size=0;
    if(other.flash()||!other.length()) {_bytes=other._bytes;_size=other._size;return true;}
    char* bytes=new(std::nothrow) char[other.length()+1];if(!bytes) return false;
    memcpy(bytes,other._bytes,other.length()+1);_bytes=bytes;_size=other._size;return true;
  }
  size_t length() const {return _size&~FlashFlag;}
  bool matches(const String& key) const {
    if(key.length()!=length()) return false;
    if(!length()) return true;
    return flash()?strcmp_P(key.c_str(),_bytes)==0:memcmp(key.c_str(),_bytes,length())==0;
  }
  bool matches(const char* key,size_t size) const {
    if(size!=length()) return false;
    if(!length()) return true;
    return flash()?strcmp_P(key,_bytes)==0:memcmp(key,_bytes,length())==0;
  }
  String text() const {
    if(flash()) return String(FPSTR(_bytes));
    String result;if(!result.reserve(length())) return result;
    for(size_t i=0;i<length();++i) result+=_bytes[i];
    return result;
  }
private:
  static constexpr size_t FlashFlag=size_t(1)<<(sizeof(size_t)*8-1);
  const char* _bytes=nullptr;size_t _size=0;
  bool flash() const {return (_size&FlashFlag)!=0;}
};
struct JSONVar::Member {
  ObjectKey key;
  JSONVar value;
  std::unique_ptr<Member> next;
};
// One bounded integer formatter shared by all integral constructor types.
template<class Tag>
void JSONVar::assignInteger(uint64_t magnitude, bool negative) {
  char text[22]; char* cursor = text + sizeof(text) - 1; *cursor = 0;
  do { *--cursor = char('0' + magnitude % 10); magnitude /= 10; } while (magnitude);
  if (negative) *--cursor = '-';
  _text = cursor; _failed = _text.length() != size_t(text + sizeof(text) - 1 - cursor);
}
inline JSONVar::JSONVar(double value) : _type(Type::Number) {
  if (!std::isfinite(value)) { _type=Type::Undefined; _failed=true; return; }
  char text[32]; snprintf(text,sizeof(text),"%.17g",value); _text=text; _failed=!_text.length();
}
inline JSONVar JSONVar::object() { JSONVar value; value._type=Type::Object; return value; }
inline JSONVar JSONVar::array() { JSONVar value; value._type=Type::Array; return value; }
inline void JSONVar::swap(JSONVar& other) noexcept {
  std::swap(_type,other._type);std::swap(_boolean,other._boolean);std::swap(_failed,other._failed);
  std::swap(_text,other._text);_children.swap(other._children);std::swap(_size,other._size);
}
inline JSONVar::JSONVar(JSONVar&& other) noexcept { swap(other); }
inline void JSONVar::clear() {
  while(_children) { Member* item=_children.release();_children=std::move(item->next);delete item; }
  _size=0;
}
inline JSONVar::~JSONVar() { clear(); }
inline JSONVar::Member* JSONVar::append(const String& key) { return appendKey(key); }
template<class Key> inline JSONVar::Member* JSONVar::appendKey(const Key& key) {
  if(_size>=MaxContainerElements || _failed) { _failed=true;return nullptr; }
  std::unique_ptr<Member> item(new(std::nothrow) Member());
  if(!item) { _failed=true;return nullptr; }
  if(!item->key.assign(key)) { _failed=true;return nullptr; }
  Member* result=item.get();auto* slot=&_children;while(*slot)slot=&(*slot)->next;
  *slot=std::move(item);++_size;return result;
}
inline JSONVar::JSONVar(const JSONVar& other) : _type(other._type),_boolean(other._boolean),_failed(other._failed),_text(other._text) {
  if(!equal(_text,other._text)) _failed=true;
  for(Member* item=other._children.get();item && !_failed;item=item->next.get()) {
    Member* copy=appendKey(item->key);if(!copy)break;copy->value=item->value;if(copy->value._failed)_failed=true;
  }
}
inline const JSONVar& JSONVar::missing() { static const JSONVar value;return value; }
inline JSONVar& JSONVar::failure() { _failed=true;static JSONVar sink;sink=JSONVar();return sink; }
inline bool JSONVar::isValid() const {
  if(_failed || isUndefined())return false;
  for(Member* item=_children.get();item;item=item->next.get())if(!item->value.isValid())return false;
  return true;
}
inline bool JSONVar::isInteger() const {
  return _type==Type::Number && _text.indexOf('.')<0 && _text.indexOf('e')<0 && _text.indexOf('E')<0;
}
inline bool JSONVar::toDouble(double& value) const {
  if(_type!=Type::Number)return false;
  errno=0;char* end=nullptr;double parsed=strtod(_text.c_str(),&end);
  if(errno==ERANGE || !end || *end || !std::isfinite(parsed))return false;
  value=parsed;return true;
}
inline bool JSONVar::toInteger(int64_t& value) const {
  if (!isInteger() || !_text.length()) return false;
  const bool negative = _text[0] == '-';
  const uint64_t limit = uint64_t(INT64_MAX) + uint64_t(negative);
  const uint64_t quotient = limit / 10, remainder = limit % 10;
  uint64_t magnitude = 0;
  for (size_t i = negative ? 1 : 0; i < _text.length(); ++i) {
    const unsigned digit = unsigned(_text[i] - '0');
    if (digit > 9 || magnitude > quotient || (magnitude == quotient && digit > remainder)) return false;
    magnitude = magnitude * 10 + digit;
  }
  value = negative ? (magnitude == limit ? INT64_MIN : -static_cast<int64_t>(magnitude)) : static_cast<int64_t>(magnitude);
  return true;
}

inline bool JSONVar::hasOwnProperty(const String& key) const {
  if(_type!=Type::Object)return false;
  for(Member* item=_children.get();item;item=item->next.get())if(item->key.matches(key))return true;
  return false;
}
// Template linkage keeps the shared lookup header-only without C++ inline.
// The compiler attribute limits firmware inlining without conflicting declarations.
// Existing C-string keys need no temporary String or heap allocation.
// String overloads still preserve embedded NULs in arbitrary JSON object keys.
template<class Tag>
JSONVar::Member* JSONVar::findMember(const char* key) const {
  if(_type!=Type::Object)return nullptr;
  if(!key)key="";
  const size_t length=strlen(key);
  for(Member* item=_children.get();item;item=item->next.get())if(item->key.matches(key,length))return item;
  return nullptr;
}
inline bool JSONVar::hasOwnProperty(const char* key) const { return findMember(key)!=nullptr; }
inline JSONVar& JSONVar::operator[](const char* key) {
  if(_type==Type::Undefined && !_failed)_type=Type::Object;
  if(_type!=Type::Object || _failed)return failure();
  if(Member* item=findMember(key))return item->value;
  Member* item=append(String(key));return item?item->value:failure();
}
inline const JSONVar& JSONVar::operator[](const char* key) const {
  if(Member* item=findMember(key))return item->value;
  return missing();
}
inline JSONVar& JSONVar::operator[](const String& key) {
  if(_type==Type::Undefined && !_failed)_type=Type::Object;
  if(_type!=Type::Object || _failed)return failure();
  for(Member* item=_children.get();item;item=item->next.get())if(item->key.matches(key))return item->value;
  Member* item=append(key);return item?item->value:failure();
}
inline const JSONVar& JSONVar::operator[](const String& key) const {
  if(_type==Type::Object)for(Member* item=_children.get();item;item=item->next.get())if(item->key.matches(key))return item->value;
  return missing();
}
inline JSONVar& JSONVar::operator[](size_t index) {
  if(_type==Type::Undefined && !_failed)_type=Type::Array;
  if(_type!=Type::Array || index>=MaxContainerElements || _failed)return failure();
  while(_size<=index) { Member* item=append("");if(!item)return failure();item->value=nullptr; }
  Member* item=_children.get();while(index--)item=item->next.get();return item->value;
}
inline const JSONVar& JSONVar::operator[](size_t index) const {
  if(_type!=Type::Array || index>=_size)return missing();
  Member* item=_children.get();while(index--)item=item->next.get();return item->value;
}
inline JSONVar& JSONVar::operator[](int index) { return index<0?failure():(*this)[static_cast<size_t>(index)]; }
inline const JSONVar& JSONVar::operator[](int index) const { return index<0?missing():(*this)[static_cast<size_t>(index)]; }
inline bool JSONVar::push(const JSONVar& value) {
  // Copy before adding: also safe for array.push(array) and array.push(array[0]).
  JSONVar copy(value);if(!copy.isValid()){_failed=true;return false;}
  JSONVar& slot=(*this)[_size];if(_failed)return false;slot=std::move(copy);return true;
}
inline bool JSONVar::tryPush(const JSONVar& value) {
  if(_type!=Type::Array||_failed||_size>=MaxContainerElements) return false;
  std::unique_ptr<Member> item(new(std::nothrow) Member());if(!item) return false;
  item->value=value;if(!item->value.isValid()) return false;
  auto* tail=&_children;while(*tail) tail=&(*tail)->next;*tail=std::move(item);++_size;return true;
}
inline JSONVar JSONVar::keys() const {
  JSONVar result=array();if(_type!=Type::Object)return result;
  for(Member* item=_children.get();item;item=item->next.get()) {String key=item->key.text();if(key.length()!=item->key.length()) {result._failed=true;break;}if(!result.push(key))break;}
  return result;
}
inline bool JSONVar::remove(const String& key) {
  if(_type!=Type::Object)return false;
  auto* slot=&_children;while(*slot && !(*slot)->key.matches(key))slot=&(*slot)->next;
  if(!*slot)return false;
  auto removed=std::move(*slot);*slot=std::move(removed->next);--_size;return true;
}
inline bool JSONVar::remove(size_t index) {
  if(_type!=Type::Array || index>=_size)return false;
  auto* slot=&_children;while(index--)slot=&(*slot)->next;
  auto removed=std::move(*slot);*slot=std::move(removed->next);--_size;return true;
}

inline bool validUtf8(const String& text) {
  for(size_t i=0;i<text.length();) {
    uint8_t first=text[i++];if(first<128)continue;
    unsigned count;uint32_t code,minimum;
    if(first>=0xc2 && first<=0xdf){count=1;code=first&31;minimum=0x80;}
    else if(first>=0xe0 && first<=0xef){count=2;code=first&15;minimum=0x800;}
    else if(first>=0xf0 && first<=0xf4){count=3;code=first&7;minimum=0x10000;}
    else return false;
    while(count--) {if(i==text.length())return false;uint8_t c=text[i++];if((c&0xc0)!=0x80)return false;code=(code<<6)|(c&63);}
    if(code<minimum || code>0x10ffff || (code>=0xd800 && code<=0xdfff))return false;
  }
  return true;
}
class Parser {
public:
  Parser(const String& input,const Limits& limits):_input(input),_limits(limits){}
  JSONVar parse() {
    JSONVar value;
    if(_input.length()>_limits.maxInputBytes){fail("input limit");return value;}
    if(!readValue(value,0))return JSONVar();
    whitespace();if(_offset!=_input.length()){fail("trailing data");return JSONVar();}
    return value;
  }
  const char* error()const{return _error;}
  size_t offset()const{return _offset;}
  size_t nodes()const{return _nodes;}
private:
  ArdCooperativeBudget _budget;
  const String& _input;const Limits& _limits;size_t _offset=0,_nodes=0;const char* _error=nullptr;
  bool fail(const char* error){if(!_error)_error=error;return false;}
  char peek()const{return _offset<_input.length()?_input[_offset]:0;}
  void whitespace(){while(peek()==' ' || peek()=='\t' || peek()=='\r' || peek()=='\n'){++_offset;if((_offset&127)==0)_budget.checkpoint();}}
  bool take(char c){whitespace();if(_offset>=_input.length() || peek()!=c)return false;++_offset;return true;}
  bool put(String& out,char c){size_t size=out.length();out+=c;return out.length()==size+1 || fail("out of memory");}
  bool hex4(uint32_t& code) {
    code=0;for(unsigned i=0;i<4;++i){char c=peek();unsigned digit;
      if(c>='0'&&c<='9')digit=c-'0';else if(c>='a'&&c<='f')digit=c-'a'+10;else if(c>='A'&&c<='F')digit=c-'A'+10;else return fail("invalid unicode escape");
      ++_offset;code=(code<<4)|digit;
    }return true;
  }
  bool unicode(String& out) {
    uint32_t code;if(!hex4(code))return false;
    if(code>=0xd800 && code<=0xdbff){
      if(peek()!='\\')return fail("missing low surrogate");
      ++_offset;
      if(peek()!='u')return fail("missing low surrogate");
      ++_offset;
      uint32_t low;if(!hex4(low) || low<0xdc00 || low>0xdfff)return fail("invalid low surrogate");
      code=0x10000+((code-0xd800)<<10)+low-0xdc00;
    }else if(code>=0xdc00 && code<=0xdfff)return fail("unpaired surrogate");
    if(code<0x80)return put(out,char(code));
    if(code<0x800)return put(out,char(0xc0|(code>>6))) && put(out,char(0x80|(code&63)));
    if(code<0x10000)return put(out,char(0xe0|(code>>12))) && put(out,char(0x80|((code>>6)&63))) && put(out,char(0x80|(code&63)));
    return put(out,char(0xf0|(code>>18))) && put(out,char(0x80|((code>>12)&63))) && put(out,char(0x80|((code>>6)&63))) && put(out,char(0x80|(code&63)));
  }
  bool string(String& out) {
    if(!take('"'))return fail("expected string");
    out="";
    while(_offset<_input.length()){
      if((_offset&127)==0)_budget.checkpoint();
      uint8_t c=_input[_offset++];if(c=='"')return validUtf8(out) || fail("invalid UTF-8");
      if(c<32)return fail("unescaped control character");
      if(c=='\\'){
        if(_offset==_input.length())return fail("unfinished escape");
        char escape=_input[_offset++];
        switch(escape){
          case '"':case '\\':case '/':if(!put(out,escape))return false;break;
          case 'b':if(!put(out,'\b'))return false;break;case 'f':if(!put(out,'\f'))return false;break;
          case 'n':if(!put(out,'\n'))return false;break;case 'r':if(!put(out,'\r'))return false;break;
          case 't':if(!put(out,'\t'))return false;break;case 'u':if(!unicode(out))return false;break;
          default:return fail("invalid escape");
        }
      }else if(!put(out,char(c)))return false;
      if(out.length()>_limits.maxStringBytes)return fail("string limit");
    }return fail("unterminated string");
  }
  bool digit()const{return peek()>='0'&&peek()<='9';}
  bool number(JSONVar& value) {
    size_t start=_offset;if(peek()=='-')++_offset;
    if(peek()=='0')++_offset;else {if(peek()<'1'||peek()>'9')return fail("invalid number");while(digit()){++_offset;if((_offset&127)==0)_budget.checkpoint();}}
    if(peek()=='.'){++_offset;if(!digit())return fail("missing fraction");while(digit()){++_offset;if((_offset&127)==0)_budget.checkpoint();}}
    if(peek()=='e'||peek()=='E'){++_offset;if(peek()=='+'||peek()=='-')++_offset;if(!digit())return fail("missing exponent");while(digit()){++_offset;if((_offset&127)==0)_budget.checkpoint();}}
    value._type=JSONVar::Type::Number;value._text=_input.substring(start,_offset);
    if(value._text.length()!=_offset-start)return fail("out of memory");
    return true;
  }
  bool readValue(JSONVar& value,unsigned depth) {
    if(depth>_limits.maxDepth || depth>32)return fail("depth limit");
    if(++_nodes>_limits.maxNodes)return fail("node limit");
    if((_nodes&31)==0)_budget.checkpoint();
    whitespace();char c=peek();
    if(c=='"'){value._type=JSONVar::Type::String;return string(value._text);}
    if(c=='-' || digit())return number(value);
    if(c=='{' || c=='['){
      bool object=c=='{';++_offset;value=object?JSONVar::object():JSONVar::array();char end=object?'}':']';
      if(take(end))return true;
      do{
        String key;if(object){if(!string(key) || !take(':'))return fail("expected object member");if(value.hasOwnProperty(key))return fail("duplicate key");}
        if(value.length()>=MaxContainerElements)return fail("container limit");
        JSONVar::Member* item=value.append(key);if(!item)return fail("out of memory");
        if(!readValue(item->value,depth+1))return false;
        if(take(end))return true;
      }while(take(','));return fail("expected comma or closing bracket");
    }
    const char* literal=c=='t'?"true":c=='f'?"false":c=='n'?"null":nullptr;
    if(!literal)return fail("expected value");
    size_t size=strlen(literal);if(_input.substring(_offset,_offset+size)!=literal)return fail("invalid literal");
    _offset+=size;value=c=='n'?JSONVar(nullptr):JSONVar(c=='t');return true;
  }
};
class Writer {
public:
  Writer(const Limits& limits,bool pretty):_limits(limits),_pretty(pretty){}
  String stringify(const JSONVar& value){if(!write(value,0))return "";return std::move(_output);}
  size_t measure(const JSONVar& value){_measure=true;return write(value,0)?_bytes:0;}
  String slice(const JSONVar& value,size_t offset,size_t maximum){_slice=true;_sliceOffset=offset;_sliceMaximum=maximum;return stringify(value);}
  const char* error()const{return _error;}
private:
  ArdCooperativeBudget _budget;
  const Limits& _limits;bool _pretty;String _output;const char* _error=nullptr;size_t _nodes=0,_bytes=0;
  bool _measure=false,_slice=false;size_t _sliceOffset=0,_sliceMaximum=0;
  bool fail(const char* error){if(!_error)_error=error;return false;}
  bool append(const String& text){
    size_t before=_bytes;if(text.length()>_limits.maxOutputBytes || before>_limits.maxOutputBytes-text.length())return fail("output limit");
    _bytes+=text.length();if(_measure)return true;
    if(_slice) {if(_bytes<=_sliceOffset||_output.length()>=_sliceMaximum)return true;size_t start=before<_sliceOffset?_sliceOffset-before:0;size_t count=text.length()-start;if(count>_sliceMaximum-_output.length())count=_sliceMaximum-_output.length();size_t old=_output.length();_output+=text.substring(start,start+count);return _output.length()==old+count||fail("out of memory");}
    _output+=text;return _output.length()==_bytes || fail("out of memory");
  }
  bool append(const char* text){String value(text);if(value.length()!=strlen(text))return fail("out of memory");return append(value);}
  bool indent(unsigned depth){if(!_pretty)return true;if(!append("\n"))return false;while(depth--)if(!append("  "))return false;return true;}
  bool string(const String& text){
    if(text.length()>_limits.maxStringBytes)return fail("string limit");
    if(!validUtf8(text))return fail("invalid UTF-8");
    if(!append("\""))return false;
    for(size_t i=0;i<text.length();++i){if((i&127)==0)_budget.checkpoint();uint8_t c=text[i];
      if(_limits.escapeHtml&&(c=='<'||c=='>'||c=='&')) {char escaped[7];snprintf(escaped,sizeof(escaped),"\\u%04x",c);if(!append(escaped))return false;}
      else if(c=='"'||c=='\\'){String byte;byte+=char(c);if(byte.length()!=1)return fail("out of memory");if(!append("\\") || !append(byte))return false;}
      else if(c<32){char escaped[7];snprintf(escaped,sizeof(escaped),"\\u%04x",c);if(!append(escaped))return false;}
      else {String byte;byte+=char(c);if(byte.length()!=1)return fail("out of memory");if(!append(byte))return false;}
    }return append("\"");
  }
  bool write(const JSONVar& value,unsigned depth){
    if(depth>_limits.maxDepth || depth>32)return fail("depth limit");
    if(++_nodes>_limits.maxNodes)return fail("node limit");
    if((_nodes&31)==0)_budget.checkpoint();
    if(value._failed)return fail("invalid value or out of memory");
    switch(value._type){
      case JSONVar::Type::Undefined:return fail("undefined value");
      case JSONVar::Type::Null:return append("null");
      case JSONVar::Type::Boolean:return append(value._boolean?"true":"false");
      case JSONVar::Type::Number:return append(value._text);
      case JSONVar::Type::String:return string(value._text);
      default:break;
    }
    bool object=value._type==JSONVar::Type::Object;if(!append(object?"{":"["))return false;bool first=true;
    for(JSONVar::Member* item=value._children.get();item;item=item->next.get()){
      if(!first && !append(","))return false;
      if(!indent(depth+1))return false;
      first=false;
      if(object) {String key=item->key.text();if(key.length()!=item->key.length())return fail("out of memory");if(!string(key) || !append(_pretty?": ":":"))return false;}
      if(!write(item->value,depth+1))return false;
    }
    if(!first && !indent(depth))return false;
    return append(object?"}":"]");
  }
};
struct API {
  JSONVar parse(const String& text,String* error=nullptr,const Limits& limits=Limits())const {
    Parser parser(text,limits);JSONVar value=parser.parse();
    if(error)*error=parser.error()?String(parser.error())+" at byte "+String(parser.offset()):String();
    return value;
  }
  String stringify(const JSONVar& value,bool pretty=false,String* error=nullptr,const Limits& limits=Limits())const {
    Writer writer(limits,pretty);String text=writer.stringify(value);if(error)*error=writer.error()?writer.error():"";return text;
  }
  size_t measure(const JSONVar& value,String* error=nullptr,const Limits& limits=Limits())const {
    Writer writer(limits,false);size_t bytes=writer.measure(value);if(error)*error=writer.error()?writer.error():"";return bytes;
  }
  String stringifySlice(const JSONVar& value,size_t offset,size_t maximum,String* error=nullptr,const Limits& limits=Limits())const {
    Writer writer(limits,false);String text=writer.slice(value,offset,maximum);if(error)*error=writer.error()?writer.error():"";return text;
  }
  const char* typeof_(const JSONVar& value)const {
    switch(value.type()){
      case JSONVar::Type::Null:return "null";case JSONVar::Type::Boolean:return "boolean";
      case JSONVar::Type::Number:return "number";case JSONVar::Type::String:return "string";
      case JSONVar::Type::Object:return "object";case JSONVar::Type::Array:return "array";default:return "undefined";
    }
  }
};
static const API JSON;
}
