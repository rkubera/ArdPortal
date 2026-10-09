from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/DynamicPages.h').read_text();a=s.index('    bool explicitInitial(');b=s.index('    /** @brief Aggregate source/output',a);methods=s[a:b]
fixture=r'''
#include <cassert>
#include "TopicTemplates.h"
#include "JsonCodec.h"
uint32_t fakeMillis=0;
struct Pages {
 using V=ArdJSON::JSONVar;
 struct Field {unsigned offset=0,length=0;bool extended=false,persist=false;};
 struct Entry {String source,condition;const __FlashStringHelper* flash=nullptr;size_t sourceLength=0,count=1;Field fields[1];};
 Entry entry;Entry* entries[1]={&entry};size_t size=1;String topicDevice="unit";ArdJsonCodec codec;
 const char* fieldId(size_t,size_t)const{return "x";}
 V fieldValue(size_t,size_t)const{V v=codec.parse(entry.source);ArdTopicTemplates::resolve(v,topicDevice);if(entry.fields[0].extended){v["extended"]=true;if(!v.hasOwnProperty("persist"))v["persist"]=entry.fields[0].persist;}if(entry.condition.length())v["_pageVisibleWhen"]=codec.parse(entry.condition);return v;}
'''
main=r'''
};
int main(){
 Pages p;using V=ArdJSON::JSONVar;
 p.entry.source=R"({"id":"x","type":"slider","default":25,"ha":{"state_topic":"stat/$device/x"},"visibleWhen":{"field":"mode","equals":"manual"}})";
 p.entry.fields[0].length=p.entry.source.length();p.entry.sourceLength=p.entry.source.length();
 V initial;assert(p.explicitInitial("x",initial)&&initial.asDouble()==25);assert(!p.explicitInitial("missing",initial));
 V conditions=p.fieldConditions(0,0);assert(conditions.isValid()&&conditions.hasOwnProperty("visibleWhen")&&!conditions.hasOwnProperty("ha"));
 V expected=p.fieldValue(0,0);expected.remove("_pageVisibleWhen");assert(ArdJSON::JSON.stringify(ArdJSON::JSON.parse(p.fieldHttpText(0,0)))==ArdJSON::JSON.stringify(expected));
 p.entry.condition=R"({"field":"enabled","equals":true})";conditions=p.fieldConditions(0,0);assert(conditions.hasOwnProperty("_pageVisibleWhen"));
 p.entry.flash=FPSTR(p.entry.source.c_str());assert(p.explicitInitial("x",initial)&&initial.asDouble()==25);assert(p.fieldHttpText(0,0).length());
 p.entry.flash=nullptr;p.entry.fields[0].extended=true;assert(!p.explicitInitial("x",initial));assert(p.fieldHttpText(0,0).length());
 p.entry.fields[0].extended=false;p.entry.source=R"({"id":"x","type":"text"})";p.entry.fields[0].length=p.entry.source.length();assert(!p.explicitInitial("x",initial));
 p.entry.source=R"({"default":true})";p.entry.fields[0].length=p.entry.source.length();assert(p.explicitInitial("x",initial)&&initial.asBool());
 p.entry.source=R"({"default":"Pump"})";p.entry.fields[0].length=p.entry.source.length();assert(p.explicitInitial("x",initial)&&initial.asString()=="Pump");
 p.entry.fields[0].length+=10;assert(!p.explicitInitial("x",initial)&&p.fieldConditions(0,0).isUndefined()&&!p.fieldHttpText(0,0).length());
 puts("PASS: actual page methods; RAM/flash definitions, native explicit defaults, selective visibility rules, topic equivalence, extended fallback and invalid slices.");
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-page-stream-') as tmp:
 tmp=Path(tmp);(tmp/'test.cpp').write_text(fixture+methods+main)
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'tests/host'),'-I'+str(root/'src'),str(tmp/'test.cpp'),'-o',str(tmp/'test')],check=True)
 subprocess.run([str(tmp/'test')],check=True)
