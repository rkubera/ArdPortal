// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ArdJSON.h"

// Registration-time structural scan of already validated JSON. Flash sources
// are read directly; runtime extraction copies only the selected field.
class ArdJsonFieldSlices {
  const String* text; const __FlashStringHelper* flash; size_t length;
  /**
   * @brief Read one character from the RAM or flash-backed source.
   * @param i Zero-based bit, byte or element index.
   * @return Source character, or zero outside its byte range.
   */
  char at(size_t i) const {return i<length?(flash?char(pgm_read_byte(reinterpret_cast<const char*>(flash)+i)):(*text)[i]):0;}
  /**
   * @brief Skip JSON whitespace at the current input position.
   * @param i Zero-based bit, byte or element index.
   * @return No value.
   */
  void whitespace(size_t& i) const {while(i<length&&(at(i)==' '||at(i)=='\r'||at(i)=='\n'||at(i)=='\t'))++i;}
  /**
   * @brief Locate the byte immediately after a JSON string, respecting escapes.
   * @param i Zero-based bit, byte or element index.
   * @return Offset after the closing quote, or source length for an invalid or unterminated string.
   */
  size_t stringEnd(size_t i) const {if(at(i++)!='"')return length;while(i<length){char c=at(i++);if(c=='\\'){if(i<length)++i;}else if(c=='"')return i;}return length;}
  /**
   * @brief Locate the byte after a JSON value without building a DOM.
   * @param i Zero-based bit, byte or element index.
   * @return End offset, or source length when the value extends to the end.
   */
  size_t valueEnd(size_t i) const {
    if(at(i)=='"')return stringEnd(i);
    if(at(i)=='{'||at(i)=='['){size_t depth=0;while(i<length){char c=at(i);if(c=='"'){i=stringEnd(i);continue;}++i;if(c=='{'||c=='[')++depth;else if(c=='}'||c==']'){if(!--depth)return i;}}return length;}
    while(i<length&&at(i)!=','&&at(i)!=']'&&at(i)!='}')++i;return i;
  }
public:
  /**
   * @brief Read one character from the RAM or flash-backed source.
   * @param index Zero-based element or field index.
   * @return Source character, or zero outside its byte range.
   */
  char character(size_t index) const {return at(index);}
  /**
   * @brief Read the source definition byte length.
   * @return Number of source bytes.
   */
  size_t size() const {return length;}
  /**
   * @brief Initialize this instance and its owned state.
   * @param source Input source or origin of a configuration change, as indicated by its type.
   * @return No value.
   */
  explicit ArdJsonFieldSlices(const String& source):text(&source),flash(nullptr),length(source.length()){}
  /**
   * @brief Initialize this instance and its owned state.
   * @param source Input source or origin of a configuration change, as indicated by its type.
   * @return No value.
   */
  explicit ArdJsonFieldSlices(const __FlashStringHelper* source):text(nullptr),flash(source),length(strlen_P(reinterpret_cast<const char*>(source))){}
  /**
   * @brief Initialize this instance and its owned state.
   * @param source Input source or origin of a configuration change, as indicated by its type.
   * @param bytes Byte buffer to read or write.
   * @return No value.
   */
  ArdJsonFieldSlices(const __FlashStringHelper* source,size_t bytes):text(nullptr),flash(source),length(bytes){}
  /**
   * @brief Copy a bounded source slice into a RAM string.
   * @param start Start byte or element offset.
   * @param count Number of items or bytes to process.
   * @return The resulting text; an empty value indicates no available text or failure where applicable.
   */
  String slice(size_t start,size_t count) const {
    if(start>length||count>length-start)return String();
    if(!flash)return text->substring(start,start+count);
    String result;if(!result.reserve(count))return String();for(size_t i=0;i<count;++i)result+=at(start+i);return result.length()==count?result:String();
  }
  /**
   * @brief Locate each field slice in a validated page and pass its index, offset and length to the callback.
   * @param expected Expected number of fields.
   * @param store Callback receiving each field index, 16-bit byte offset and 16-bit length.
   * @return True when the fields array has exactly the expected count and all slices fit the supported offsets.
   */
  /** @brief Visit members of a validated object without allocating its DOM.
   * Offsets refer to this source. Return false on malformed ranges or callback refusal. */
  template<class Callback> bool members(size_t start,size_t count,Callback visit) const {
    if(start>length||count>length-start)return false;
    const size_t limit=start+count;size_t i=start;whitespace(i);
    if(i>=limit||at(i++)!='{')return false;
    while(i<limit){
      whitespace(i);if(at(i)=='}'){++i;whitespace(i);return i==limit;}
      if(at(i)!='"')return false;
      size_t key=i,end=stringEnd(i);if(end<=i||end>limit||at(end-1)!='"')return false;
      auto decoded=ArdJSON::JSON.parse(slice(key,end-key));if(!decoded.isValid()||decoded.type()!=ArdJSON::JSONVar::Type::String)return false;
      String name=decoded.asString();i=end;whitespace(i);if(i>=limit||at(i++)!=':')return false;whitespace(i);
      size_t value=i;i=valueEnd(i);if(i<=value||i>limit)return false;
      if(!visit(name,value,i-value))return false;
      whitespace(i);if(i>=limit)return false;
      if(at(i)==','){++i;whitespace(i);if(at(i)=='}')return false;continue;}
      if(at(i)!='}')return false;
    }
    return false;
  }
  template<class Callback> bool fields(size_t expected,Callback store) const {
    size_t i=0;whitespace(i);if(at(i++)!='{')return false;
    while(i<length){whitespace(i);if(at(i)=='}')return false;size_t key=i,end=stringEnd(i);if(end==i)return false;
      String name=ArdJSON::JSON.parse(slice(key,end-key)).asString();i=end;whitespace(i);if(at(i++)!=':')return false;whitespace(i);
      if(name=="fields"){if(at(i++)!='[')return false;size_t count=0;while(i<length){whitespace(i);if(at(i)==']')return count==expected;size_t start=i;i=valueEnd(i);if(count>=expected||i<=start||start>65535||i-start>65535)return false;store(count++,uint16_t(start),uint16_t(i-start));whitespace(i);if(at(i)==','){++i;continue;}if(at(i)==']')return count==expected;return false;}return false;}
      i=valueEnd(i);whitespace(i);if(at(i)==',')++i;else return false;
    }return false;
  }
};
