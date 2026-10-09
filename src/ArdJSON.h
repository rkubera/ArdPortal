// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>
#include "CooperativeBudget.h"
#include "ArdAllocation.h"
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
  /**
   * @brief Initialize this instance and its owned state.
   * @return No value.
   */
  JSONVar() = default;
  /**
   * @brief Initialize this instance and its owned state.
   * Input: nullptr, producing a JSON null value.
   * @return No value.
   */
  JSONVar(std::nullptr_t) : _type(Type::Null) {}
  /**
   * @brief Initialize this instance and its owned state.
   * @param value Input value, or output destination when passed by mutable reference.
   * @return No value.
   */
  JSONVar(bool value) : _type(Type::Boolean), _boolean(value) {}
  /**
   * @brief Initialize this instance and its owned state.
   * @param value Input value, or output destination when passed by mutable reference.
   * @return No value.
   */
  JSONVar(const char* value) : _type(value ? Type::String : Type::Null), _text(value ? value : "") { _failed=value && _text.length()!=strlen(value); }
  /**
   * @brief Initialize this instance and its owned state.
   * @param value Input value, or output destination when passed by mutable reference.
   * @return No value.
   */
  JSONVar(const String& value) : _type(Type::String), _text(value) { _failed=!equal(_text,value); }
  /**
   * @brief Initialize this instance and its owned state.
   * @param value Input value, or output destination when passed by mutable reference.
   * @return No value.
   */
  JSONVar(double value);
  /**
   * @brief Initialize this instance and its owned state.
   * @param value Input value, or output destination when passed by mutable reference.
   * @return No value.
   */
  template<class T, typename std::enable_if<std::is_integral<T>::value && !std::is_same<T,bool>::value,int>::type = 0>
  JSONVar(T value) : _type(Type::Number) {
    const bool negative = std::is_signed<T>::value && static_cast<int64_t>(value) < 0;
    const uint64_t bits = static_cast<uint64_t>(value);
    assignInteger(negative ? uint64_t(0) - bits : bits, negative);
  }
  /**
   * @brief Initialize this instance and its owned state.
   * @param other Instance whose state is copied, moved or exchanged.
   * @return No value.
   */
  JSONVar(const JSONVar& other);
  /**
   * @brief Initialize this instance and its owned state.
   * @param other Instance whose state is copied, moved or exchanged.
   * @return No value.
   */
  JSONVar(JSONVar&& other) noexcept;
  /**
   * @brief Release the resources owned by this instance.
   * @return No value.
   */
  ~JSONVar();
  /**
   * @brief Replace this instance state with the supplied value.
   * @param other Instance whose state is copied, moved or exchanged.
   * @return Reference to this instance after assignment; deleted overloads cannot be called.
   */
  JSONVar& operator=(JSONVar other) { swap(other); return *this; }
  /**
   * @brief Exchange the stored state with another instance.
   * @param other Instance whose state is copied, moved or exchanged.
   * @return No value.
   */
  void swap(JSONVar& other) noexcept;
  /**
   * @brief Create an empty JSON object.
   * @return An empty JSON object.
   */
  static JSONVar object();
  /**
   * @brief Create an empty JSON array.
   * @return An empty JSON array.
   */
  static JSONVar array();
  /**
   * @brief Read the stored JSON value type.
   * @return The stored JSON type.
   */
  Type type() const { return _type; }
  /**
   * @brief Check whether the JSON value has the Undefined type.
   * @return True only for the Undefined JSON type.
   */
  bool isUndefined() const { return _type == Type::Undefined; }
  /**
   * @brief Check whether the JSON value has the Null type.
   * @return True only for the Null JSON type.
   */
  bool isNull() const { return _type == Type::Null; }
  /**
   * @brief Check whether the JSON value and its descendants were created without allocation failure.
   * @return True if this value and its descendants have no recorded allocation failure.
   */
  bool isValid() const;
  /**
   * @brief Check whether the numeric lexeme represents an exact integer.
   * @return True only when the number can be represented as an exact signed integer.
   */
  bool isInteger() const;
  /**
   * @brief Convert the exact numeric lexeme to a signed 64-bit integer.
   * @param value Input value, or output destination when passed by mutable reference.
   * @return True with value populated for an exact signed integer in range; false otherwise.
   */
  bool toInteger(int64_t& value) const;
  /**
   * @brief Convert the exact numeric lexeme to an unsigned 64-bit integer.
   * @param value Input value, or output destination when passed by mutable reference.
   * @return True with value populated for an unsigned integer in range; false otherwise.
   */
  bool toUnsignedInteger(uint64_t& value) const {
    if (_type != Type::Number || !_text.length()) return false;
    uint64_t n = 0;
    for (size_t i=0; i<_text.length(); ++i) {
      char c=_text[i]; if (c<'0' || c>'9' || n>(UINT64_MAX-uint64_t(c-'0'))/10) return false;
      n=n*10+uint64_t(c-'0');
    }
    value=n; return true;
  }
  /**
   * @brief Convert a numeric lexeme to double with range and syntax checks.
   * @param value Input value, or output destination when passed by mutable reference.
   * @return True with value populated for a finite representable number; false otherwise.
   */
  bool toDouble(double& value) const;
  /**
   * @brief Read a JSON string, returning an empty string for other types.
   * @return The stored JSON string, or an empty string for other types.
   */
  String asString() const { return _type == Type::String ? _text : String(); }
  /**
   * @brief Read a JSON boolean, returning false for other types.
   * @return The stored boolean, or false when the value has a different type.
   */
  bool asBool() const { return _type == Type::Boolean && _boolean; }
  /**
   * @brief Convert a JSON number to double, returning zero when conversion fails.
   * @return The converted numeric value, or zero when conversion fails.
   */
  double asDouble() const { double value = 0; toDouble(value); return value; }
  /**
   * @brief Convert the stored JSON value to String.
   * @return The converted String value; the component conversion defines its fallback for incompatible values.
   */
  explicit operator String() const { return asString(); }
  /**
   * @brief Convert the stored JSON value to bool.
   * @return The converted bool value; the component conversion defines its fallback for incompatible values.
   */
  explicit operator bool() const { return asBool(); }
  /**
   * @brief Convert the stored JSON value to double.
   * @return The converted double value; the component conversion defines its fallback for incompatible values.
   */
  explicit operator double() const { return asDouble(); }
  /**
   * @brief Convert the stored JSON value to int.
   * @return The converted int value; the component conversion defines its fallback for incompatible values.
   */
  explicit operator int() const {
    int64_t value;
    return toInteger(value) && value >= std::numeric_limits<int>::min() && value <= std::numeric_limits<int>::max() ? static_cast<int>(value) : 0;
  }
  /**
   * @brief Read the number of elements in this JSON container.
   * @return Number of members or elements in the JSON container.
   */
  size_t length() const { return _size; }
  /**
   * @brief Check whether this JSON object contains the supplied key.
   * @param key Configuration key or JSON object member name.
   * @return True if the object owns the key; false for missing keys or nonobject values.
   */
  bool hasOwnProperty(const String& key) const;
  /**
   * @brief Check whether this JSON object contains the supplied key.
   * @param key Configuration key or JSON object member name.
   * @return True if the object owns the key; false for missing keys or nonobject values.
   */
  bool hasOwnProperty(const char* key) const;
  /**
   * @brief Build an array of the keys in this JSON object.
   * @return An array of member names; an invalid value indicates allocation failure.
   */
  JSONVar keys() const;
  /**
   * @brief Access the requested indexed value; mutable JSON access can create a missing member.
   * @param key Configuration key or JSON object member name.
   * @return Reference to the requested stored value or component.
   */
  JSONVar& operator[](const char* key);
  /**
   * @brief Access the requested indexed value; mutable JSON access can create a missing member.
   * @param key Configuration key or JSON object member name.
   * @return Reference to the requested stored value or component.
   */
  const JSONVar& operator[](const char* key) const;
  /**
   * @brief Access the requested indexed value; mutable JSON access can create a missing member.
   * @param key Configuration key or JSON object member name.
   * @return Reference to the requested stored value or component.
   */
  JSONVar& operator[](const String& key);
  /**
   * @brief Access the requested indexed value; mutable JSON access can create a missing member.
   * @param key Configuration key or JSON object member name.
   * @return Reference to the requested stored value or component.
   */
  const JSONVar& operator[](const String& key) const;
  /**
   * @brief Access the requested indexed value; mutable JSON access can create a missing member.
   * @param index Zero-based element or field index.
   * @return Reference to the requested stored value or component.
   */
  JSONVar& operator[](size_t index);
  /**
   * @brief Access the requested indexed value; mutable JSON access can create a missing member.
   * @param index Zero-based element or field index.
   * @return Reference to the requested stored value or component.
   */
  const JSONVar& operator[](size_t index) const;
  /**
   * @brief Access the requested indexed value; mutable JSON access can create a missing member.
   * @param index Zero-based element or field index.
   * @return Reference to the requested stored value or component.
   */
  JSONVar& operator[](int index);
  /**
   * @brief Access the requested indexed value; mutable JSON access can create a missing member.
   * @param index Zero-based element or field index.
   * @return Reference to the requested stored value or component.
   */
  const JSONVar& operator[](int index) const;
  /**
   * @brief Append a copy of the supplied value to the JSON array.
   * @param value Input value, or output destination when passed by mutable reference.
   * @return True when appended; false on a type, capacity or allocation failure.
   */
  bool push(const JSONVar& value);
  /**
   * @brief Append a value transactionally, preserving the array on failure.
   * @param value Input value, or output destination when passed by mutable reference.
   * @return True when appended; false if the value could not be copied or linked.
   */
  bool tryPush(const JSONVar& value); // Transactional append: failure leaves the array intact.
  /**
   * @brief Remove the selected JSON member or array element.
   * @param key Configuration key or JSON object member name.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool remove(const String& key);
  /**
   * @brief Remove the selected JSON member or array element.
   * @param index Zero-based element or field index.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool remove(size_t index);
  /** @brief Conservative allocator footprint of one JSON member, including heap bookkeeping. */
  static size_t memberAllocationBytes();
  // Iterate without constructing an array containing every object key.
  template<class Visitor> bool forEachObjectMember(Visitor&& visitor) const;
  // Undefined patch members delete keys; existing/new nodes are moved, never cloned.
  bool applyObjectPatch(JSONVar&& patch, bool eraseUndefined=true);
  bool isObjectPatchValid() const;

private:
  struct Member;
  Type _type = Type::Undefined;
  bool _boolean = false, _failed = false;
  String _text; // Strings or exact number lexemes: large integers never lose precision.
  std::unique_ptr<Member> _children;
  size_t _size = 0;
  /**
   * @brief Compare strings by exact length and byte contents.
   * @param a First value or byte range to compare.
   * @param b Second value or byte range to compare.
   * @return True if the strings are byte-for-byte equal; false otherwise.
   */
  static bool equal(const String& a,const String& b) { return a.length()==b.length() && !memcmp(a.c_str(),b.c_str(),a.length()); }
  /**
   * @brief Store an integer as an exact decimal lexeme without floating-point rounding.
   * @param magnitude Unsigned magnitude of the integer.
   * @param negative Whether to encode a negative integer.
   * @return No value.
   */
  template<class = void>
#if defined(__GNUC__)
  __attribute__((noinline))
#endif
  void assignInteger(uint64_t magnitude, bool negative);
  /**
   * @brief Expose the immutable Undefined value used for unsuccessful lookups.
   * @return Reference to the shared Undefined sentinel.
   */
  static const JSONVar& missing();
  /**
   * @brief Mark this JSON value invalid after an allocation or type failure.
   * @return Reference to this now-invalid value.
   */
  JSONVar& failure();
  /**
   * @brief Append the supplied data or journal record to the current destination.
   * @param key Configuration key or JSON object member name.
   * @return Pointer to the requested data, or nullptr when unavailable.
   */
  Member* append(const String& key);
  /**
   * @brief Locate an object member by its key.
   * @param key Configuration key or JSON object member name.
   * @return Pointer to the requested data, or nullptr when unavailable.
   */
  template<class = void>
#if defined(__GNUC__)
  __attribute__((noinline))
#endif
  Member* findMember(const char* key) const;
  /**
   * @brief Allocate and link a member using an exact or flash-backed key.
   * @param key Key stored in the new member.
   * @return New member pointer, or nullptr on allocation failure.
   */
  template<class Key> Member* appendKey(const Key& key);
  /**
   * @brief Release JSON children and text, restoring an Undefined value.
   * @return No value.
   */
  void clear();
  friend class Parser;
  friend class Writer;
};
// Common immutable object keys share flash storage. Other keys own exact bytes,
// including embedded NULs; array members need no key allocation.
class ObjectKey {
public:
  /**
   * @brief Initialize this instance and its owned state.
   * @return No value.
   */
  ObjectKey() = default;
  /**
   * @brief Release the resources owned by this instance.
   * @return No value.
   */
  ~ObjectKey() { if(!flash()) std::free(const_cast<char*>(_bytes)); }
  /**
   * @brief Disallow copying or moving this resource-owning instance.
   * Input: const ObjectKey&.
   * @return No result; this operation is deleted and cannot be called.
   */
  ObjectKey(const ObjectKey&) = delete;
  /**
   * @brief Replace this instance state with the supplied value.
   * Input: const ObjectKey&.
   * @return No result; this operation is deleted and cannot be called.
   */
  ObjectKey& operator=(const ObjectKey&) = delete;
  /**
   * @brief Assign a JSON object member through this key proxy.
   * @param key Configuration key or JSON object member name.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
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
    if(!flash()) std::free(const_cast<char*>(_bytes));
    _bytes=nullptr;_size=0;
    if(!key.length()) return true;
    if(selected) {_bytes=selected;_size=key.length()|FlashFlag;return true;}
    char* bytes=static_cast<char*>(ArdAllocation::allocate(key.length()+1));if(!bytes) return false;
    memcpy(bytes,key.c_str(),key.length()+1);_bytes=bytes;_size=key.length();return true;
  }
  /**
   * @brief Assign a JSON object member through this key proxy.
   * @param other Instance whose state is copied, moved or exchanged.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool assign(const ObjectKey& other) {
    if(this==&other) return true;
    if(!flash()) std::free(const_cast<char*>(_bytes));
    _bytes=nullptr;_size=0;
    if(other.flash()||!other.length()) {_bytes=other._bytes;_size=other._size;return true;}
    char* bytes=static_cast<char*>(ArdAllocation::allocate(other.length()+1));if(!bytes) return false;
    memcpy(bytes,other._bytes,other.length()+1);_bytes=bytes;_size=other._size;return true;
  }
  /**
   * @brief Read the number of elements in this JSON container.
   * @return Number of members or elements in the JSON container.
   */
  size_t length() const {return _size&~FlashFlag;}
  /**
   * @brief Compare this object key with a supplied key, including its exact length.
   * @param key Configuration key or JSON object member name.
   * @return True if the key bytes and lengths match; false otherwise.
   */
  bool matches(const ObjectKey& other) const {
    if(length()!=other.length())return false;
    if(_bytes==other._bytes||!length())return true;
    if(!flash()&&!other.flash())return !memcmp(_bytes,other._bytes,length());
    for(size_t i=0;i<length();++i)if((flash()?pgm_read_byte(_bytes+i):uint8_t(_bytes[i]))!=(other.flash()?pgm_read_byte(other._bytes+i):uint8_t(other._bytes[i])))return false;
    return true;
  }
  bool matches(const String& key) const {
    if(key.length()!=length()) return false;
    if(!length()) return true;
    return flash()?strcmp_P(key.c_str(),_bytes)==0:memcmp(key.c_str(),_bytes,length())==0;
  }
  /**
   * @brief Compare this object key with a supplied key, including its exact length.
   * @param key Configuration key or JSON object member name.
   * @param size Number of bytes or elements.
   * @return True if the key bytes and lengths match; false otherwise.
   */
  bool matches(const char* key,size_t size) const {
    if(size!=length()) return false;
    if(!length()) return true;
    return flash()?strcmp_P(key,_bytes)==0:memcmp(key,_bytes,length())==0;
  }
  /**
   * @brief Read, decode or append text according to the component representation.
   * @return The resulting text; an empty value indicates no available text or failure where applicable.
   */
  String text() const {
    if(flash()) return String(FPSTR(_bytes));
    String result;if(!result.reserve(length())) return result;
    for(size_t i=0;i<length();++i) result+=_bytes[i];
    return result;
  }
private:
  static constexpr size_t FlashFlag=size_t(1)<<(sizeof(size_t)*8-1);
  const char* _bytes=nullptr;size_t _size=0;
  /**
   * @brief Check whether the object key points to immutable program memory.
   * @return True for a flash-backed key; false for an owned RAM key.
   */
  bool flash() const {return (_size&FlashFlag)!=0;}
};
struct JSONVar::Member {
  ARDPORTAL_NO_THROW_ALLOCATION
  ObjectKey key;
  JSONVar value;
  std::unique_ptr<Member> next;
};
inline size_t JSONVar::memberAllocationBytes() { return sizeof(Member)+2*sizeof(void*); }

template<class Visitor> inline bool JSONVar::forEachObjectMember(Visitor&& visitor) const {
  if(_type!=Type::Object||_failed)return false;
  for(const Member* item=_children.get();item;item=item->next.get()) {
    String key=item->key.text();if(key.length()!=item->key.length()||!visitor(key,item->value))return false;
  }
  return true;
}
inline bool JSONVar::isObjectPatchValid() const {
  if(_type!=Type::Object||_failed)return false;
  for(const Member* item=_children.get();item;item=item->next.get())if(item->value._failed||(!item->value.isUndefined()&&!item->value.isValid()))return false;
  return true;
}
inline bool JSONVar::applyObjectPatch(JSONVar&& patch, bool eraseUndefined) {
  if(this==&patch||_type!=Type::Object||patch._type!=Type::Object||_failed||!patch.isObjectPatchValid())return false;
  // Validate the resulting size before changing either object.
  size_t size=_size;
  for(const Member* item=patch._children.get();item;item=item->next.get()) {
    bool present=false;for(const Member* old=_children.get();old;old=old->next.get())if(old->key.matches(item->key)){present=true;break;}
    if(eraseUndefined&&item->value.isUndefined()){if(present)--size;}else if(!present)++size;
  }
  if(size>MaxContainerElements)return false;
  while(patch._children){
    auto item=std::move(patch._children);patch._children=std::move(item->next);--patch._size;
    std::unique_ptr<Member>* found=&_children;
    while(*found&&!(*found)->key.matches(item->key))found=&(*found)->next;
    if(eraseUndefined&&item->value.isUndefined()) {
      if(*found){auto removed=std::move(*found);*found=std::move(removed->next);--_size;}
    }else if(*found)(*found)->value=std::move(item->value);
    else{*found=std::move(item);++_size;}
  }
  return true;
}
// One bounded integer formatter shared by all integral constructor types.
/**
 * @brief Store an integer as an exact decimal lexeme without floating-point rounding.
 * @param magnitude Unsigned magnitude of the integer.
 * @param negative Whether to encode a negative integer.
 * @return No value.
 */
template<class Tag>
void JSONVar::assignInteger(uint64_t magnitude, bool negative) {
  char text[22]; char* cursor = text + sizeof(text) - 1; *cursor = 0;
  do { *--cursor = char('0' + magnitude % 10); magnitude /= 10; } while (magnitude);
  if (negative) *--cursor = '-';
  _text = cursor; _failed = _text.length() != size_t(text + sizeof(text) - 1 - cursor);
}
/**
 * @brief Initialize this instance and its owned state.
 * @param value Input value, or output destination when passed by mutable reference.
 * @return No value.
 */
inline JSONVar::JSONVar(double value) : _type(Type::Number) {
  if (!std::isfinite(value)) { _type=Type::Undefined; _failed=true; return; }
  char text[32]; snprintf(text,sizeof(text),"%.17g",value); _text=text; _failed=!_text.length();
}
/**
 * @brief Create an empty JSON object.
 * @return An empty JSON object.
 */
inline JSONVar JSONVar::object() { JSONVar value; value._type=Type::Object; return value; }
/**
 * @brief Create an empty JSON array.
 * @return An empty JSON array.
 */
inline JSONVar JSONVar::array() { JSONVar value; value._type=Type::Array; return value; }
/**
 * @brief Exchange the stored state with another instance.
 * @param other Instance whose state is copied, moved or exchanged.
 * @return No value.
 */
inline void JSONVar::swap(JSONVar& other) noexcept {
  std::swap(_type,other._type);std::swap(_boolean,other._boolean);std::swap(_failed,other._failed);
  std::swap(_text,other._text);_children.swap(other._children);std::swap(_size,other._size);
}
/**
 * @brief Initialize this instance and its owned state.
 * @param other Instance whose state is copied, moved or exchanged.
 * @return No value.
 */
inline JSONVar::JSONVar(JSONVar&& other) noexcept { swap(other); }
/**
 * @brief Release JSON children and text, restoring an Undefined value.
 * @return No value.
 */
inline void JSONVar::clear() {
  while(_children) { Member* item=_children.release();_children=std::move(item->next);delete item; }
  _size=0;
}
/**
 * @brief Release the resources owned by this instance.
 * @return No value.
 */
inline JSONVar::~JSONVar() { clear(); }
/**
 * @brief Append the supplied data or journal record to the current destination.
 * @param key Configuration key or JSON object member name.
 * @return Pointer to the requested data, or nullptr when unavailable.
 */
inline JSONVar::Member* JSONVar::append(const String& key) { return appendKey(key); }
/**
 * @brief Allocate and link an object member with an exact or flash-backed key.
 * @param key Configuration key or JSON object member name.
 * @return New member pointer, or nullptr on allocation failure.
 */
template<class Key> inline JSONVar::Member* JSONVar::appendKey(const Key& key) {
  if(_size>=MaxContainerElements || _failed) { _failed=true;return nullptr; }
  std::unique_ptr<Member> item(new(std::nothrow) Member());
  if(!item) { _failed=true;return nullptr; }
  if(!item->key.assign(key)) { _failed=true;return nullptr; }
  Member* result=item.get();auto* slot=&_children;while(*slot)slot=&(*slot)->next;
  *slot=std::move(item);++_size;return result;
}
/**
 * @brief Initialize this instance and its owned state.
 * @param other Instance whose state is copied, moved or exchanged.
 * @return No value.
 */
inline JSONVar::JSONVar(const JSONVar& other) : _type(other._type),_boolean(other._boolean),_failed(other._failed),_text(other._text) {
  if(!equal(_text,other._text)) _failed=true;
  for(Member* item=other._children.get();item && !_failed;item=item->next.get()) {
    Member* copy=appendKey(item->key);if(!copy)break;copy->value=item->value;if(copy->value._failed)_failed=true;
  }
}
/**
 * @brief Expose the immutable Undefined value used for unsuccessful lookups.
 * @return Reference to the shared Undefined sentinel.
 */
inline const JSONVar& JSONVar::missing() { static const JSONVar value;return value; }
/**
 * @brief Mark this JSON value invalid after an allocation or type failure.
 * @return Reference to this now-invalid value.
 */
inline JSONVar& JSONVar::failure() { _failed=true;static JSONVar sink;sink=JSONVar();return sink; }
/**
 * @brief Check whether the JSON value and its descendants were created without allocation failure.
 * @return True if this value and its descendants have no recorded allocation failure.
 */
inline bool JSONVar::isValid() const {
  if(_failed || isUndefined())return false;
  for(Member* item=_children.get();item;item=item->next.get())if(!item->value.isValid())return false;
  return true;
}
/**
 * @brief Check whether the numeric lexeme represents an exact integer.
 * @return True only when the number can be represented as an exact signed integer.
 */
inline bool JSONVar::isInteger() const {
  return _type==Type::Number && _text.indexOf('.')<0 && _text.indexOf('e')<0 && _text.indexOf('E')<0;
}
/**
 * @brief Convert a numeric lexeme to double with range and syntax checks.
 * @param value Input value, or output destination when passed by mutable reference.
 * @return True with value populated for a finite representable number; false otherwise.
 */
inline bool JSONVar::toDouble(double& value) const {
  if(_type!=Type::Number)return false;
  errno=0;char* end=nullptr;double parsed=strtod(_text.c_str(),&end);
  if(errno==ERANGE || !end || *end || !std::isfinite(parsed))return false;
  value=parsed;return true;
}
/**
 * @brief Convert the exact numeric lexeme to a signed 64-bit integer.
 * @param value Input value, or output destination when passed by mutable reference.
 * @return True with value populated for an exact signed integer in range; false otherwise.
 */
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

/**
 * @brief Check whether this JSON object contains the supplied key.
 * @param key Configuration key or JSON object member name.
 * @return True if the object owns the key; false for missing keys or nonobject values.
 */
inline bool JSONVar::hasOwnProperty(const String& key) const {
  if(_type!=Type::Object)return false;
  for(Member* item=_children.get();item;item=item->next.get())if(item->key.matches(key))return true;
  return false;
}
// Template linkage keeps the shared lookup header-only without C++ inline.
// The compiler attribute limits firmware inlining without conflicting declarations.
// Existing C-string keys need no temporary String or heap allocation.
// String overloads still preserve embedded NULs in arbitrary JSON object keys.
/**
 * @brief Locate an object member by its key.
 * @param key Configuration key or JSON object member name.
 * @return Pointer to the matching member, or nullptr when absent.
 */
template<class Tag>
JSONVar::Member* JSONVar::findMember(const char* key) const {
  if(_type!=Type::Object)return nullptr;
  if(!key)key="";
  const size_t length=strlen(key);
  for(Member* item=_children.get();item;item=item->next.get())if(item->key.matches(key,length))return item;
  return nullptr;
}
/**
 * @brief Check whether this JSON object contains the supplied key.
 * @param key Configuration key or JSON object member name.
 * @return True if the object owns the key; false for missing keys or nonobject values.
 */
inline bool JSONVar::hasOwnProperty(const char* key) const { return findMember(key)!=nullptr; }
/**
 * @brief Access a JSON member or element, creating missing members or extending arrays when allowed.
 * @param key Object member key.
 * @return Reference to the selected value, or this invalid value on type, index or allocation failure.
 */
inline JSONVar& JSONVar::operator[](const char* key) {
  if(_type==Type::Undefined && !_failed)_type=Type::Object;
  if(_type!=Type::Object || _failed)return failure();
  if(Member* item=findMember(key))return item->value;
  Member* item=append(String(key));return item?item->value:failure();
}
/**
 * @brief Read an existing JSON member or array element.
 * @param key Object member key.
 * @return Reference to the matching value, or the shared Undefined sentinel when absent.
 */
inline const JSONVar& JSONVar::operator[](const char* key) const {
  if(Member* item=findMember(key))return item->value;
  return missing();
}
/**
 * @brief Access a JSON member or element, creating missing members or extending arrays when allowed.
 * @param key Object member key.
 * @return Reference to the selected value, or this invalid value on type, index or allocation failure.
 */
inline JSONVar& JSONVar::operator[](const String& key) {
  if(_type==Type::Undefined && !_failed)_type=Type::Object;
  if(_type!=Type::Object || _failed)return failure();
  for(Member* item=_children.get();item;item=item->next.get())if(item->key.matches(key))return item->value;
  Member* item=append(key);return item?item->value:failure();
}
/**
 * @brief Read an existing JSON member or array element.
 * @param key Object member key.
 * @return Reference to the matching value, or the shared Undefined sentinel when absent.
 */
inline const JSONVar& JSONVar::operator[](const String& key) const {
  if(_type==Type::Object)for(Member* item=_children.get();item;item=item->next.get())if(item->key.matches(key))return item->value;
  return missing();
}
/**
 * @brief Access a JSON member or element, creating missing members or extending arrays when allowed.
 * @param index Zero-based array index; negative integer indexes are rejected.
 * @return Reference to the selected value, or this invalid value on type, index or allocation failure.
 */
inline JSONVar& JSONVar::operator[](size_t index) {
  if(_type==Type::Undefined && !_failed)_type=Type::Array;
  if(_type!=Type::Array || index>=MaxContainerElements || _failed)return failure();
  while(_size<=index) { Member* item=append("");if(!item)return failure();item->value=nullptr; }
  Member* item=_children.get();while(index--)item=item->next.get();return item->value;
}
/**
 * @brief Read an existing JSON member or array element.
 * @param index Zero-based array index; negative integer indexes are rejected.
 * @return Reference to the matching value, or the shared Undefined sentinel when absent.
 */
inline const JSONVar& JSONVar::operator[](size_t index) const {
  if(_type!=Type::Array || index>=_size)return missing();
  Member* item=_children.get();while(index--)item=item->next.get();return item->value;
}
/**
 * @brief Access a JSON member or element, creating missing members or extending arrays when allowed.
 * @param index Zero-based array index; negative integer indexes are rejected.
 * @return Reference to the selected value, or this invalid value on type, index or allocation failure.
 */
inline JSONVar& JSONVar::operator[](int index) { return index<0?failure():(*this)[static_cast<size_t>(index)]; }
/**
 * @brief Read an existing JSON member or array element.
 * @param index Zero-based array index; negative integer indexes are rejected.
 * @return Reference to the matching value, or the shared Undefined sentinel when absent.
 */
inline const JSONVar& JSONVar::operator[](int index) const { return index<0?missing():(*this)[static_cast<size_t>(index)]; }
/**
 * @brief Append a copy of the supplied value to the JSON array.
 * @param value Input value, or output destination when passed by mutable reference.
 * @return True when appended; false on a type, capacity or allocation failure.
 */
inline bool JSONVar::push(const JSONVar& value) {
  // Copy before adding: also safe for array.push(array) and array.push(array[0]).
  JSONVar copy(value);if(!copy.isValid()){_failed=true;return false;}
  JSONVar& slot=(*this)[_size];if(_failed)return false;slot=std::move(copy);return true;
}
/**
 * @brief Append a value transactionally, preserving the array on failure.
 * @param value Input value, or output destination when passed by mutable reference.
 * @return True when appended; false if the value could not be copied or linked.
 */
inline bool JSONVar::tryPush(const JSONVar& value) {
  if(_type!=Type::Array||_failed||_size>=MaxContainerElements) return false;
  std::unique_ptr<Member> item(new(std::nothrow) Member());if(!item) return false;
  item->value=value;if(!item->value.isValid()) return false;
  auto* tail=&_children;while(*tail) tail=&(*tail)->next;*tail=std::move(item);++_size;return true;
}
/**
 * @brief Build an array of the keys in this JSON object.
 * @return An array of member names; an invalid value indicates allocation failure.
 */
inline JSONVar JSONVar::keys() const {
  JSONVar result=array();if(_type!=Type::Object)return result;
  for(Member* item=_children.get();item;item=item->next.get()) {String key=item->key.text();if(key.length()!=item->key.length()) {result._failed=true;break;}if(!result.push(key))break;}
  return result;
}
/**
 * @brief Remove the selected JSON member or array element.
 * @param key Configuration key or JSON object member name.
 * @return True on success; false if validation, resource allocation or the operation fails.
 */
inline bool JSONVar::remove(const String& key) {
  if(_type!=Type::Object)return false;
  auto* slot=&_children;while(*slot && !(*slot)->key.matches(key))slot=&(*slot)->next;
  if(!*slot)return false;
  auto removed=std::move(*slot);*slot=std::move(removed->next);--_size;return true;
}
/**
 * @brief Remove the selected JSON member or array element.
 * @param index Zero-based element or field index.
 * @return True on success; false if validation, resource allocation or the operation fails.
 */
inline bool JSONVar::remove(size_t index) {
  if(_type!=Type::Array || index>=_size)return false;
  auto* slot=&_children;while(index--)slot=&(*slot)->next;
  auto removed=std::move(*slot);*slot=std::move(removed->next);--_size;return true;
}

/**
 * @brief Check that the input contains complete, well-formed UTF-8 sequences.
 * @param text Text to read, encode or display.
 * @return True for well-formed UTF-8; false for malformed or incomplete sequences.
 */
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
  /**
   * @brief Initialize this instance and its owned state.
   * @param input Source text or bytes to process.
   * @param limits JSON parsing or serialization resource limits.
   * @return No value.
   */
  Parser(const String& input,const Limits& limits):_input(input),_limits(limits){}
  /**
   * @brief Parse JSON text with the supplied resource limits.
   * @return Parsed JSON value, or Undefined with error text when input is invalid or a resource limit is exceeded.
   */
  JSONVar parse() {
    JSONVar value;
    if(_input.length()>_limits.maxInputBytes){fail("input limit");return value;}
    if(!readValue(value,0))return JSONVar();
    whitespace();if(_offset!=_input.length()){fail("trailing data");return JSONVar();}
    return value;
  }
  /**
   * @brief Expose the most recent error reported by this component.
   * @return Reference to the last error text; empty when no error is recorded.
   */
  const char* error()const{return _error;}
  /**
   * @brief Read the current position in the source or output.
   * @return The current position in the source or output.
   */
  size_t offset()const{return _offset;}
  /**
   * @brief Read the number of JSON nodes consumed or required.
   * @return The number of JSON nodes consumed or required.
   */
  size_t nodes()const{return _nodes;}
private:
  ArdCooperativeBudget _budget;
  const String& _input;const Limits& _limits;size_t _offset=0,_nodes=0;const char* _error=nullptr;
  /**
   * @brief Record the parser or writer failure and stop further processing.
   * @param error Output error text; populated when the operation fails.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool fail(const char* error){if(!_error)_error=error;return false;}
  /**
   * @brief Read the current parser character without advancing its cursor.
   * @return Current character, or the parser end-of-input sentinel.
   */
  char peek()const{return _offset<_input.length()?_input[_offset]:0;}
  /**
   * @brief Skip JSON whitespace at the current input position.
   * @return No value.
   */
  void whitespace(){while(peek()==' ' || peek()=='\t' || peek()=='\r' || peek()=='\n'){++_offset;if((_offset&127)==0)_budget.checkpoint();}}
  /**
   * @brief Check take.
   * @param c Current character or running CRC state, according to its type.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool take(char c){whitespace();if(_offset>=_input.length() || peek()!=c)return false;++_offset;return true;}
  /**
   * @brief Append output bytes while enforcing the writer output budget.
   * @param out Output destination populated by this operation.
   * @param c Current character or running CRC state, according to its type.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool put(String& out,char c){size_t size=out.length();out+=c;return out.length()==size+1 || fail("out of memory");}
  /**
   * @brief Decode four hexadecimal digits from a JSON Unicode escape.
   * @param code Protocol, language or error code.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool hex4(uint32_t& code) {
    code=0;for(unsigned i=0;i<4;++i){char c=peek();unsigned digit;
      if(c>='0'&&c<='9')digit=c-'0';else if(c>='a'&&c<='f')digit=c-'a'+10;else if(c>='A'&&c<='F')digit=c-'A'+10;else return fail("invalid unicode escape");
      ++_offset;code=(code<<4)|digit;
    }return true;
  }
  /**
   * @brief Encode a decoded Unicode code point into UTF-8.
   * @param out Output destination populated by this operation.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
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
  /**
   * @brief Parse or serialize a JSON string and its escaping.
   * @param out Output destination populated by this operation.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
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
  /**
   * @brief Convert a decimal or hexadecimal digit to its numeric value.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool digit()const{return peek()>='0'&&peek()<='9';}
  /**
   * @brief Parse or serialize a JSON number while preserving its numeric lexeme.
   * @param value Input value, or output destination when passed by mutable reference.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool number(JSONVar& value) {
    size_t start=_offset;if(peek()=='-')++_offset;
    if(peek()=='0')++_offset;else {if(peek()<'1'||peek()>'9')return fail("invalid number");while(digit()){++_offset;if((_offset&127)==0)_budget.checkpoint();}}
    if(peek()=='.'){++_offset;if(!digit())return fail("missing fraction");while(digit()){++_offset;if((_offset&127)==0)_budget.checkpoint();}}
    if(peek()=='e'||peek()=='E'){++_offset;if(peek()=='+'||peek()=='-')++_offset;if(!digit())return fail("missing exponent");while(digit()){++_offset;if((_offset&127)==0)_budget.checkpoint();}}
    value._type=JSONVar::Type::Number;value._text=_input.substring(start,_offset);
    if(value._text.length()!=_offset-start)return fail("out of memory");
    return true;
  }
  /**
   * @brief Read value.
   * @param value Input value, or output destination when passed by mutable reference.
   * @param depth Current JSON nesting depth.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
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
  /**
   * @brief Initialize this instance and its owned state.
   * @param limits JSON parsing or serialization resource limits.
   * @param pretty Whether to indent the serialized JSON.
   * @return No value.
   */
  Writer(const Limits& limits,bool pretty):_limits(limits),_pretty(pretty){}
  /**
   * @brief Serialize a JSON value with the supplied resource limits.
   * @param value Input value, or output destination when passed by mutable reference.
   * @return Serialized JSON text, or an empty string with error text on failure.
   */
  String stringify(const JSONVar& value){if(!write(value,0))return "";return std::move(_output);}
  /**
   * @brief Measure the serialized value or response without retaining its output.
   * @param value Input value, or output destination when passed by mutable reference.
   * @return Required serialized byte count, or zero on failure with error text where supplied.
   */
  size_t measure(const JSONVar& value){_measure=true;return write(value,0)?_bytes:0;}
  /**
   * @brief Copy a bounded source slice into a RAM string.
   * @param value Input value, or output destination when passed by mutable reference.
   * @param offset Byte offset into the source or destination.
   * @param maximum Maximum permitted byte or element count.
   * @return The resulting text; an empty value indicates no available text or failure where applicable.
   */
  String slice(const JSONVar& value,size_t offset,size_t maximum){_slice=true;_sliceOffset=offset;_sliceMaximum=maximum;return stringify(value);}
  /**
   * @brief Expose the most recent error reported by this component.
   * @return Reference to the last error text; empty when no error is recorded.
   */
  const char* error()const{return _error;}
  size_t measureObjectPatch(const JSONVar& base,const JSONVar& patch){_measure=true;return writeObjectPatch(base,patch)?_bytes:0;}
  String sliceObjectPatch(const JSONVar& base,const JSONVar& patch,size_t offset,size_t maximum){
    _slice=true;_sliceOffset=offset;_sliceMaximum=maximum;
    return writeObjectPatch(base,patch)?std::move(_output):String();
  }

private:
  ArdCooperativeBudget _budget;
  const Limits& _limits;bool _pretty;String _output;const char* _error=nullptr;size_t _nodes=0,_bytes=0;
  bool _measure=false,_slice=false;size_t _sliceOffset=0,_sliceMaximum=0;
  /**
   * @brief Record the parser or writer failure and stop further processing.
   * @param error Output error text; populated when the operation fails.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool fail(const char* error){if(!_error)_error=error;return false;}
  /**
   * @brief Append the supplied data or journal record to the current destination.
   * @param text Text to read, encode or display.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool append(const String& text){
    size_t before=_bytes;if(text.length()>_limits.maxOutputBytes || before>_limits.maxOutputBytes-text.length())return fail("output limit");
    _bytes+=text.length();if(_measure)return true;
    if(_slice) {if(_bytes<=_sliceOffset||_output.length()>=_sliceMaximum)return true;size_t start=before<_sliceOffset?_sliceOffset-before:0;size_t count=text.length()-start;if(count>_sliceMaximum-_output.length())count=_sliceMaximum-_output.length();size_t old=_output.length();_output+=text.substring(start,start+count);return _output.length()==old+count||fail("out of memory");}
    _output+=text;return _output.length()==_bytes || fail("out of memory");
  }
  /**
   * @brief Append the supplied data or journal record to the current destination.
   * @param text Text to read, encode or display.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool append(const char* text){String value(text);if(value.length()!=strlen(text))return fail("out of memory");return append(value);}
  /**
   * @brief Append the indentation required for pretty JSON output.
   * @param depth Current JSON nesting depth.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool indent(unsigned depth){if(!_pretty)return true;if(!append("\n"))return false;while(depth--)if(!append("  "))return false;return true;}
  /**
   * @brief Parse or serialize a JSON string and its escaping.
   * @param text Text to read, encode or display.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
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
  /**
   * @brief Request or perform a write through the component API.
   * @param value Input value, or output destination when passed by mutable reference.
   * @param depth Current JSON nesting depth.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */

  bool writeObjectPatch(const JSONVar& base,const JSONVar& patch){
    if(base._type!=JSONVar::Type::Object||patch._type!=JSONVar::Type::Object||!base.isValid()||!patch.isObjectPatchValid())return fail("invalid object patch");
    if(++_nodes>_limits.maxNodes)return fail("node limit");
    if(!append("{"))return false;bool first=true;size_t members=0;
    auto emit=[&](const JSONVar::Member* item,const JSONVar& value){
      if(value.isUndefined())return true;
      if(++members>MaxContainerElements)return fail("element limit");
      if(!first&&!append(","))return false;first=false;
      String key=item->key.text();if(key.length()!=item->key.length())return fail("out of memory");
      return string(key)&&append(":")&&write(value,1);
    };
    for(const JSONVar::Member* item=base._children.get();item;item=item->next.get()){
      String key=item->key.text();if(key.length()!=item->key.length())return fail("out of memory");
      const auto& value=patch.hasOwnProperty(key)?patch[key]:item->value;if(!emit(item,value))return false;
    }
    for(const JSONVar::Member* item=patch._children.get();item;item=item->next.get()){
      String key=item->key.text();if(key.length()!=item->key.length())return fail("out of memory");
      if(!base.hasOwnProperty(key)&&!emit(item,item->value))return false;
    }
    return append("}");
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
  /**
   * @brief Parse JSON text with the supplied resource limits.
   * @param text Text to read, encode or display.
   * @param error Output error text; populated when the operation fails.
   * @param limits JSON parsing or serialization resource limits.
   * @return Parsed JSON value, or Undefined with error text when input is invalid or a resource limit is exceeded.
   */
  JSONVar parse(const String& text,String* error=nullptr,const Limits& limits=Limits())const {
    Parser parser(text,limits);JSONVar value=parser.parse();
    if(error)*error=parser.error()?String(parser.error())+" at byte "+String(parser.offset()):String();
    return value;
  }
  /**
   * @brief Serialize a JSON value with the supplied resource limits.
   * @param value Input value, or output destination when passed by mutable reference.
   * @param pretty Whether to indent the serialized JSON.
   * @param error Output error text; populated when the operation fails.
   * @param limits JSON parsing or serialization resource limits.
   * @return Serialized JSON text, or an empty string with error text on failure.
   */
  String stringify(const JSONVar& value,bool pretty=false,String* error=nullptr,const Limits& limits=Limits())const {
    Writer writer(limits,pretty);String text=writer.stringify(value);if(error)*error=writer.error()?writer.error():"";return text;
  }
  /**
   * @brief Measure the serialized value or response without retaining its output.
   * @param value Input value, or output destination when passed by mutable reference.
   * @param error Output error text; populated when the operation fails.
   * @param limits JSON parsing or serialization resource limits.
   * @return Required serialized byte count, or zero on failure with error text where supplied.
   */
  size_t measure(const JSONVar& value,String* error=nullptr,const Limits& limits=Limits())const {
    Writer writer(limits,false);size_t bytes=writer.measure(value);if(error)*error=writer.error()?writer.error():"";return bytes;
  }
  /**
   * @brief Serialize only the requested byte slice of a JSON value.
   * @param value Input value, or output destination when passed by mutable reference.
   * @param offset Byte offset into the source or destination.
   * @param maximum Maximum permitted byte or element count.
   * @param error Output error text; populated when the operation fails.
   * @param limits JSON parsing or serialization resource limits.
   * @return The requested serialized byte range, or an empty string on failure.
   */

  size_t measureObjectPatch(const JSONVar& base,const JSONVar& patch,String* error=nullptr,const Limits& limits=Limits())const {
    Writer writer(limits,false);size_t bytes=writer.measureObjectPatch(base,patch);if(error)*error=writer.error()?writer.error():"";return bytes;
  }
  String stringifyObjectPatchSlice(const JSONVar& base,const JSONVar& patch,size_t offset,size_t maximum,String* error=nullptr,const Limits& limits=Limits())const {
    Writer writer(limits,false);String text=writer.sliceObjectPatch(base,patch,offset,maximum);if(error)*error=writer.error()?writer.error():"";return text;
  }
  String stringifySlice(const JSONVar& value,size_t offset,size_t maximum,String* error=nullptr,const Limits& limits=Limits())const {
    Writer writer(limits,false);String text=writer.slice(value,offset,maximum);if(error)*error=writer.error()?writer.error():"";return text;
  }
  /**
   * @brief Return the JSON type name used by the compatibility helpers.
   * @param value Input value, or output destination when passed by mutable reference.
   * @return Pointer to the requested data, or nullptr when unavailable.
   */
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
