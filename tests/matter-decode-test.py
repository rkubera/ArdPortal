#!/usr/bin/env python3
"""Exercise shared command decoder and compile-time removal of control families."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/MatterNativeImpl.h').read_text()
a=s.index('inline __attribute__((noinline)) int ArdMatter::prepareCommand')
b=s.index('template<class T>',a)
fixture=r'''
#include "MatterFields.h"
#include <cassert>
#undef ARDPORTAL_MATTER_LOG_SUPPORT
#define ARDPORTAL_MATTER_LOG_SUPPORT 1
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
struct esp_matter_attr_val_t {union {uint32_t u32;uint16_t u16;int16_t i16;uint8_t u8;bool b;} val{};};
struct ArdMatter {
 using Operation=ArdMatterFields::Operation;
 bool _localReport=false,_accepting=true;uint8_t _queued=0,_commandDrops=0;int _mux=0;
 int prepareCommand(uint32_t,uint32_t,const esp_matter_attr_val_t*,Operation&,uint32_t&);
};
'''
main=r'''
int main(){
 ArdMatter owner;ArdMatter::Operation op{};uint32_t raw=0;esp_matter_attr_val_t v{};
 auto decode=[&](uint32_t cluster,uint32_t attr){return owner.prepareCommand(cluster,attr,&v,op,raw);};
 v.val.u8=100;
#if ARDPORTAL_ENABLE_CONTROL_SWITCH || ARDPORTAL_ENABLE_CONTROL_SLIDER || ARDPORTAL_ENABLE_CONTROL_LIGHT || ARDPORTAL_ENABLE_CONTROL_FAN
 v.val.b=true;assert(decode(6,0)==1&&op==ArdMatter::Operation::OnOff&&raw==1);
 assert(decode(6,1)==0);
#else
 assert(decode(6,0)==0);
#endif
#if ARDPORTAL_ENABLE_CONTROL_SLIDER || ARDPORTAL_ENABLE_CONTROL_LIGHT
 v.val.u8=100;assert(decode(8,0)==1&&op==ArdMatter::Operation::Level&&raw==100);
 owner._queued=8;assert(decode(8,0)==-1&&owner._commandDrops==1);
 owner._accepting=false;assert(decode(8,0)==1);owner._queued=0;owner._accepting=true;
#else
 assert(decode(8,0)==0);
#endif
#if ARDPORTAL_ENABLE_CONTROL_LIGHT
 v.val.u16=65279;assert(decode(0x300,3)==1&&op==ArdMatter::Operation::X&&raw==65279);
 assert(decode(0x300,4)==1&&op==ArdMatter::Operation::Y);
 v.val.u8=125;assert(decode(0x300,0)==1&&op==ArdMatter::Operation::Hue&&raw==125);
 assert(decode(0x300,1)==1&&op==ArdMatter::Operation::Saturation);
#else
 assert(decode(0x300,0)==0);
#endif
#if ARDPORTAL_ENABLE_CONTROL_FAN
 assert(decode(0x202,0)==1&&op==ArdMatter::Operation::FanMode);
 assert(decode(0x202,2)==1&&op==ArdMatter::Operation::Speed);
#else
 assert(decode(0x202,0)==0);
#endif
#if ARDPORTAL_ENABLE_CONTROL_CLIMATE
 v.val.i16=-1234;assert(decode(0x201,0x11)==1&&op==ArdMatter::Operation::Cool&&int32_t(raw)==-1234);
 assert(decode(0x201,0x12)==1&&op==ArdMatter::Operation::Heat);
 assert(decode(0x201,0x1c)==1&&op==ArdMatter::Operation::Mode);
#else
 assert(decode(0x201,0x11)==0);
#endif
 assert(decode(0x300,9)==0&&decode(0x201,9)==0&&decode(0x202,9)==0&&decode(0x999,0)==0);
 owner._localReport=true;assert(decode(6,0)==0);
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-matter-decode-') as directory:
 path=Path(directory);(path/'test.cpp').write_text(fixture+s[a:b]+main)
 for flags in ([],['-DARDPORTAL_ENABLE_CONTROLS=0','-DARDPORTAL_ENABLE_CONTROL_SLIDER=1'],['-DARDPORTAL_ENABLE_CONTROLS=0']):
  subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',*flags,'-I'+str(root/'tests/host'),'-I'+str(root/'src'),str(path/'test.cpp'),'-o',str(path/'test')],check=True)
  subprocess.run([str(path/'test')],check=True)
print('PASS: shared native decoder, signed values, queue admission, drops and control-family defines')
