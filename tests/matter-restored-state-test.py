#!/usr/bin/env python3
"""Exercise actual pre-report cache reconciliation after SDK NVS restoration."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/MatterNativeImpl.h').read_text()
a=s.index('#if ARDPORTAL_MATTER_COMMAND_SUPPORT',s.index('  _localReport = true;',s.index('inline bool ArdMatter::publish')))
b=s.index('#define ARD_MATTER_NATIVE',a)
fragment=s[a:b]
fixture=r'''
#include "MatterFields.h"
#include <cassert>
using Kind=ArdMatterFields::Kind;
uint32_t fakeMillis=0;
struct esp_matter_attr_val_t {unsigned value=0;};
namespace chip {namespace DeviceLayer {
struct Manager {int unlocks=0;void UnlockChipStack(){++unlocks;}} manager;
Manager& PlatformMgr(){return manager;}
}}
struct Endpoint {
 unsigned actual[12]={0,10,20,30,4,5,6,7,8,9,10,11},cache[12]={1,100,200,250,104,105,106,107,108,109,110,111};unsigned reads=0,writes=0;bool okay=true;
 unsigned index(unsigned cluster,unsigned attribute){if(cluster==0x202)return attribute==0?4:5;if(cluster==0x201)return attribute==0x1c?6:attribute==0x12?7:attribute==0x11?8:9;if(cluster==0x102)return attribute==0xe?10:11;return cluster==6?0:cluster==8?1:attribute==0?2:3;}
 unsigned getEndPointId(){return 1;}
 bool getAttributeVal(unsigned cluster,unsigned attribute,esp_matter_attr_val_t* out){++reads;out->value=actual[index(cluster,attribute)];return okay;}
 bool attributeChangeCB(unsigned endpoint,unsigned cluster,unsigned attribute,esp_matter_attr_val_t* val){assert(endpoint==1);cache[index(cluster,attribute)]=val->value;return okay;}
 void set(unsigned index,unsigned value){if(cache[index]==value)return;cache[index]=actual[index]=value;++writes;}
};
struct Entry {Endpoint* endpoint;Kind kind;};
struct Owner {bool _localReport=false;
 bool report(Entry& entry){_localReport=true;auto definition=ArdJSON::JSON.parse(R"({"modes":["heat_cool"]})");
'''+fragment+r'''
 auto* native=entry.endpoint;
 if(entry.kind==Kind::Outlet||entry.kind==Kind::OnOffLight||entry.kind==Kind::Dimmer||entry.kind==Kind::Light)native->set(0,1);
 if(entry.kind==Kind::Dimmer||entry.kind==Kind::Light)native->set(1,100);
 if(entry.kind==Kind::Light){native->set(2,200);native->set(3,250);}
 _localReport=false;return true;
 }};
int main(){
 for(auto kind:{Kind::Outlet,Kind::OnOffLight,Kind::Dimmer,Kind::Light,Kind::Fan,Kind::Climate,Kind::Cover}){
  Owner owner;Endpoint endpoint;Entry entry{&endpoint,kind};
  assert(owner.report(entry));assert(!owner._localReport);if(kind==Kind::Outlet||kind==Kind::OnOffLight||kind==Kind::Dimmer||kind==Kind::Light)assert(endpoint.actual[0]==1);
  if(kind==Kind::Dimmer||kind==Kind::Light)assert(endpoint.actual[1]==100);
  if(kind==Kind::Light)assert(endpoint.actual[2]==200&&endpoint.actual[3]==250);
  if(kind==Kind::Fan)assert(endpoint.cache[4]==4&&endpoint.cache[5]==5);
  if(kind==Kind::Climate)for(int i=6;i<10;++i)assert(endpoint.cache[i]==endpoint.actual[i]);
  if(kind==Kind::Cover)assert(endpoint.cache[10]==10&&endpoint.cache[11]==11);
  auto writes=endpoint.writes;
  assert(owner.report(entry)&&endpoint.writes==writes); // No duplicate attribute writes.
 }
 Owner owner;Endpoint failed;failed.okay=false;Entry entry{&failed,Kind::Dimmer};
 assert(!owner.report(entry)&&!owner._localReport&&chip::DeviceLayer::manager.unlocks==1);
 assert(!failed.writes);
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(fixture)
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'tests/host'),'-I'+str(root/'src'),str(p/'test.cpp'),str(root/'src/JsonStream.cpp'), str(root/'src/JsonDocumentStream.cpp'), '-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: restored on/off, level and RGB caches, unchanged reports and failure lock cleanup')
