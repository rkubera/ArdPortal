#!/usr/bin/env python3
# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
"""Run actual Matter command-to-app validation/persistence with selective controls."""
import subprocess
import tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
source=(root/'src/MatterPortalImpl.h').read_text()
a=source.index('#if ARDPORTAL_MATTER_COMMAND_SUPPORT\nbool ArdPortal::acceptMatterCommand(')
b=source.index('void ArdPortal::serviceMatter(',a)
fixture=r'''
#define ARDPORTAL_DEFINE_CONTROL_DATA
#define ARDPORTAL_ENABLE_CONTROLS 0
#define ARDPORTAL_ENABLE_CONTROL_SWITCH 1
#define ARDPORTAL_ENABLE_CONTROL_SLIDER 1
#define ARDPORTAL_ENABLE_CONTROL_TEXT 1
#define ARDPORTAL_ENABLE_CONTROL_CLIMATE 1
#include <cassert>
#include "MatterFields.h"
#include "DynamicPages.h"
uint32_t fakeMillis=0;
using V=ArdJSON::JSONVar;
enum class ChangeSource {Application,Matter};
struct Dynamic {
 V definitions=V::array();
 size_t count()const{return definitions.length();}
 V at(size_t i)const{return definitions[i];}
};
struct Controls {
 V state=V::object(); unsigned calls=0;ChangeSource source=ChangeSource::Application;
 bool applyAppState(const String& key,const V& value,ChangeSource s){state[key]=value;++calls;source=s;return true;}
};
struct ArdPortal {
 Dynamic _dynamic;Controls _appControls;bool busy=false;unsigned writes=0;V patch;
 V getAppConfigValue(const char* key){return static_cast<const V&>(_appControls.state)[key];}
 bool scheduleAppPatch(V p,ChangeSource s){if(busy)return false;assert(s==ChangeSource::Matter);patch=std::move(p);++writes;return true;}
 bool acceptMatterCommand(const ArdMatterFields::Command&);
};
'''
main=r'''
int main(){
 using namespace ArdMatterFields;
 ArdPortal p;
 p._dynamic.definitions=ArdJSON::JSON.parse(R"([
 {"id":"s","type":"switch","default":false,"persist":true},
 {"id":"l","type":"slider","class":"brightness","min":0,"max":100,"step":5,"default":0,"persist":false},
 {"id":"r","type":"switch","readonly":true,"default":false},
 {"id":"t","type":"text","class":"temperature","default":20,"persist":false},
 {"id":"c","type":"climate","min":7,"max":30,"step":0.5,"modes":["off","heat"],"default":{"temperature":20,"mode":"off"},"persist":false}
 ])");
 p._appControls.state=ArdJSON::JSON.parse(R"({"s":false,"l":0,"r":false,"t":20,"c":{"temperature":20,"mode":"off"}})");
 assert(p.acceptMatterCommand({0,Operation::OnOff,1}));
 assert(p.writes==1&&p.patch["s"].asBool()&&p._appControls.state["s"].asBool());
 assert(p._appControls.source==ChangeSource::Matter);
 p.busy=true;
 assert(!p.acceptMatterCommand({0,Operation::OnOff,0}));
 assert(p._appControls.state["s"].asBool());
 assert(p.acceptMatterCommand({1,Operation::Level,127}));
 assert(p._appControls.state["l"].asDouble()==50 && p.writes==1);
 assert(p.acceptMatterCommand({1,Operation::OnOff,0}));
 assert(p._appControls.state["l"].asDouble()==0);
 assert(p.acceptMatterCommand({1,Operation::OnOff,1}));
 assert(p._appControls.state["l"].asDouble()==100);
 assert(p.acceptMatterCommand({4,Operation::Heat,2250}));
 assert(p._appControls.state["c"]["temperature"].asDouble()==22.5 && p._appControls.state["c"]["mode"].asString()=="off");
 assert(!p.acceptMatterCommand({4,Operation::Cool,2300}));
 assert(!p.acceptMatterCommand({4,Operation::Mode,3}));
 assert(p.acceptMatterCommand({4,Operation::Mode,4}));
 unsigned before=p._appControls.calls;
 assert(!p.acceptMatterCommand({0,Operation::OnOff,2}));
 assert(!p.acceptMatterCommand({0,Operation::Level,2}));
 assert(!p.acceptMatterCommand({1,Operation::Level,255}));
 assert(!p.acceptMatterCommand({2,Operation::OnOff,1}));
 assert(!p.acceptMatterCommand({3,Operation::Level,1}));
 assert(!p.acceptMatterCommand({99,Operation::OnOff,1}));
 assert(p._appControls.calls==before);
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-matter-command-') as directory:
    path=Path(directory);(path/'test.cpp').write_text(fixture+source[a:b]+main)
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'tests/host'),'-I'+str(root/'src'),str(path/'test.cpp'),str(root/'src/JsonStream.cpp'), str(root/'src/JsonDocumentStream.cpp'), '-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
print('PASS: actual Matter commands, bounds, class mapping, read-only rejection, steps, source and persistence admission')
