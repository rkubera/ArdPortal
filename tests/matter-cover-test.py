#!/usr/bin/env python3
"""Exercise the actual cover configuration and state report against SDK doubles."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/MatterNativeImpl.h').read_text()
a=s.index('          using Status = chip::app::Clusters::WindowCovering::ConfigStatus;')
b=s.index('          native->onOpen',a)
config=s[a:b]
a=s.index('        result = native->setCurrentLiftPercent100ths',s.index('inline bool ArdMatter::publish'))
b=s.index('        break;',a)
report=s[a:b]
fixture=r'''
#include "MatterFields.h"
#include <cassert>
uint32_t fakeMillis=0;
namespace chip {namespace app {namespace Clusters {namespace WindowCovering {
enum class ConfigStatus:unsigned char {kOperational=1,kLiftPositionAware=8,kTiltPositionAware=16};
}}}}
struct esp_matter_attr_val_t {struct {uint8_t u8=0;} val;};
using Attr=esp_matter_attr_val_t;Attr esp_matter_bitmap8(uint8_t value){Attr result;result.val.u8=value;return result;}
struct Native {
 unsigned lift=0,tilt=0,status=255,flags=0,configWrites=0;bool okay=true;
 bool setCurrentLiftPercent100ths(unsigned value){lift=value;return okay;}
 bool setCurrentTiltPercent100ths(unsigned value){tilt=value;return okay;}
 bool setOperationalStatus(unsigned value){status=value;return okay;}
 bool getAttributeVal(unsigned cluster,unsigned attribute,Attr* value){assert(cluster==0x102&&attribute==7);value->val.u8=flags;return okay;}
 bool setAttributeVal(unsigned cluster,unsigned attribute,Attr* value){assert(cluster==0x102&&attribute==7);++configWrites;if(flags==value->val.u8)return false;flags=value->val.u8;return okay;}
} sdk;
bool configure(){auto* native=&sdk;bool created=true;
'''+config+r'''
return created;}
bool publish(ArdJSON::JSONVar value){auto* native=&sdk;bool result=false;
'''+report+r'''
return result;}
int main(){
 assert(configure()&&sdk.flags==25);assert(configure()&&sdk.configWrites==1);sdk.okay=false;assert(!configure());sdk.okay=true;
 auto v=ArdJSON::JSON.parse(R"({"position":75,"tilt":25,"state":"open"})");
 assert(publish(v)&&sdk.lift==2500&&sdk.tilt==7500&&sdk.status==0);
 v["state"]="opening";assert(publish(v)&&sdk.status==5);
 v["state"]="closing";assert(publish(v)&&sdk.status==10);
 v["state"]="closed";v["position"]=0;v["tilt"]=0;
 assert(publish(v)&&sdk.lift==10000&&sdk.tilt==10000&&sdk.status==0);
 sdk.okay=false;assert(!publish(v));
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(fixture)
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'tests/host'),'-I'+str(root/'src'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: cover operational/position-aware flags, inverted lift/tilt, motion completion and SDK failures')
