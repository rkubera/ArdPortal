// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "JsonFieldSlices.h"

// A bounded structural scanner. Each emitted value is subsequently validated by
// ArdJSON; no whole-page DOM or whole PROGMEM source copy is required.
class ArdAppConfigPageScanner {
public:
  enum Event { Waiting, Member, Field, Complete, Failed };
  String key,reason;
  size_t offset=0,start=0,bytes=0,fieldCount=0;
  bool fieldsSeen=false;
private:
  enum State { Open, Key, KeyToken, Colon, Value, ValueToken, MemberEnd, FieldStart, FieldToken, FieldEnd, Tail, Done } state=Open;
  bool quoted=false,escape=false,stringToken=false,composite=false,afterComma=false;
  size_t tokenStart=0,depth=0;
  char closing[32];
  /**
   * @brief Initialize the structural scanner state for the value at the current offset.
   * @param source Input source or origin of a configuration change, as indicated by its type.
   * @return No value; scanner token state is initialized.
   */
  template<class Source> void beginToken(const Source& source) {
    tokenStart=offset;char c=source.character(offset);quoted=false;escape=false;depth=0;
    stringToken=c=='"';composite=c=='{'||c=='[';
  }
  /**
   * @brief Consume a JSON token within the remaining byte budget, recording malformed input.
   * @param source Input source or origin of a configuration change, as indicated by its type.
   * @param budget Maximum work or byte budget available to this call.
   * @param finished Output flag indicating that the current token is complete.
   * @return True for a valid token or partial token; false for malformed JSON. finished distinguishes completion from exhausted budget.
   */
  template<class Source> bool token(const Source& source,size_t& budget,bool& finished) {
    finished=false;
    while(offset<source.size()&&budget){char c=source.character(offset);
      if(!composite&&!stringToken&&(c==','||c==']'||c=='}'||c==' '||c=='\t'||c=='\r'||c=='\n')){finished=true;break;}
      --budget;++offset;
      if(quoted){if(escape)escape=false;else if(c=='\\')escape=true;else if(c=='"'){quoted=false;if(stringToken){finished=true;break;}}continue;}
      if(c=='"'){quoted=true;continue;}
      if(composite){if(c=='{'||c=='['){if(depth>=32){reason="depth limit";return false;}closing[depth++]=c=='{'?'}':']';}
        else if(c=='}'||c==']'){if(!depth||closing[depth-1]!=c){reason="mismatched bracket";return false;}if(!--depth){finished=true;break;}}}
    }
    if(offset==source.size()&&!finished){if(quoted||depth||stringToken){reason="unfinished JSON value";return false;}finished=true;}
    if(finished&&offset==tokenStart){reason="expected value";return false;}return true;
  }
public:
  /**
   * @brief Restore the component state to its initial values.
   * @return No value; state is reset for reuse.
   */
  void reset(){*this=ArdAppConfigPageScanner();}
  /**
   * @brief Advance structural scanning until an event, failure or exhausted byte budget.
   * @param source Input source or origin of a configuration change, as indicated by its type.
   * @param budget Maximum work or byte budget available to this call.
   * @return Waiting when more work is needed, Member or Field for a discovered value, Complete at end of input, or Failed with reason set.
   */
  template<class Source> Event step(const Source& source,size_t budget=256) {
    if(state==Done)return Complete;
    auto fail=[&](const char* text){reason=String(text)+" at byte "+String(offset);return Failed;};
    while(budget){
      if(state==KeyToken||state==ValueToken||state==FieldToken){bool finished;if(!token(source,budget,finished))return Failed;if(!finished)return Waiting;
        start=tokenStart;bytes=offset-start;
        if(state==KeyToken){String error;String text=source.slice(start,bytes);if(text.length()!=bytes){reason="out of memory copying key";return Failed;}auto value=ArdJSON::JSON.parse(text,&error);if(value.type()!=ArdJSON::JSONVar::Type::String){reason=error.length()?error:String("expected key string");return Failed;}key=value.asString();state=Colon;continue;}
        if(state==FieldToken){++fieldCount;state=FieldEnd;return Field;}
        state=MemberEnd;return Member;
      }
      if(offset==source.size()){if(state==Tail){state=Done;return Complete;}return fail("unexpected end");}
      char c=source.character(offset);
      if(c==' '||c=='\t'||c=='\r'||c=='\n'){++offset;--budget;continue;}
      --budget;
      switch(state){
        case Open:if(c!='{')return fail("page must be an object");++offset;state=Key;break;
        case Key:if(c=='}'&&!afterComma){++offset;state=Tail;break;}if(c!='"')return fail("expected member key");beginToken(source);state=KeyToken;break;
        case Colon:if(c!=':')return fail("expected colon");++offset;state=Value;break;
        case Value:
          if(key=="fields"){if(fieldsSeen)return fail("duplicate fields key");fieldsSeen=true;if(c!='[')return fail("fields must be an array");++offset;afterComma=false;state=FieldStart;}
          else {beginToken(source);state=ValueToken;}break;
        case MemberEnd:if(c==','){++offset;afterComma=true;state=Key;}else if(c=='}'){++offset;state=Tail;}else return fail("expected comma or closing object");break;
        case FieldStart:if(c==']'&&!afterComma){++offset;state=MemberEnd;break;}beginToken(source);state=FieldToken;break;
        case FieldEnd:if(c==','){++offset;afterComma=true;state=FieldStart;}else if(c==']'){++offset;state=MemberEnd;}else return fail("expected comma or closing fields array");break;
        case Tail:return fail("trailing data");
        default:return fail("invalid scanner state");
      }
    }return Waiting;
  }
};
