// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ArdJSON.h"

// Registration-time structural scan of already validated JSON. Flash sources
// are read directly; runtime extraction copies only the selected field.
class ArdJsonFieldSlices {
  const String* text; const __FlashStringHelper* flash; size_t length;
  char at(size_t i) const {return i<length?(flash?char(pgm_read_byte(reinterpret_cast<const char*>(flash)+i)):(*text)[i]):0;}
  void whitespace(size_t& i) const {while(i<length&&(at(i)==' '||at(i)=='\r'||at(i)=='\n'||at(i)=='\t'))++i;}
  size_t stringEnd(size_t i) const {if(at(i++)!='"')return length;while(i<length){char c=at(i++);if(c=='\\'){if(i<length)++i;}else if(c=='"')return i;}return length;}
  size_t valueEnd(size_t i) const {
    if(at(i)=='"')return stringEnd(i);
    if(at(i)=='{'||at(i)=='['){size_t depth=0;while(i<length){char c=at(i);if(c=='"'){i=stringEnd(i);continue;}++i;if(c=='{'||c=='[')++depth;else if(c=='}'||c==']'){if(!--depth)return i;}}return length;}
    while(i<length&&at(i)!=','&&at(i)!=']'&&at(i)!='}')++i;return i;
  }
public:
  char character(size_t index) const {return at(index);}
  size_t size() const {return length;}
  explicit ArdJsonFieldSlices(const String& source):text(&source),flash(nullptr),length(source.length()){}
  explicit ArdJsonFieldSlices(const __FlashStringHelper* source):text(nullptr),flash(source),length(strlen_P(reinterpret_cast<const char*>(source))){}
  ArdJsonFieldSlices(const __FlashStringHelper* source,size_t bytes):text(nullptr),flash(source),length(bytes){}
  String slice(size_t start,size_t count) const {
    if(start>length||count>length-start)return String();
    if(!flash)return text->substring(start,start+count);
    String result;if(!result.reserve(count))return String();for(size_t i=0;i<count;++i)result+=at(start+i);return result.length()==count?result:String();
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
