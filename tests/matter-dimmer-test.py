#!/usr/bin/env python3
"""Exercise actual SDK callback adapter: remembered level must not turn a dimmer on."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
source = (root / 'src/MatterNativeImpl.h').read_text()
a = source.index('inline __attribute__((noinline)) int ArdMatter::prepareCommand')
b = source.index('inline ArdMatter::~ArdMatter()', a)
fixture = r'''
#include <cassert>
#include <type_traits>
#define ARDPORTAL_ENABLE_CONTROLS 0
#define ARDPORTAL_ENABLE_CONTROL_SLIDER 1
#include "MatterFields.h"
uint32_t fakeMillis=0;
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
struct esp_matter_attr_val_t { union { uint32_t u32; uint16_t u16; int16_t i16; uint8_t u8; bool b; } val{}; };
struct MatterDimmablePlugin {
 bool on=true; uint8_t level=195;
 virtual bool attributeChangeCB(uint16_t,uint32_t cluster,uint32_t,esp_matter_attr_val_t* v) {
  if(cluster==6)on=v->val.b; else if(cluster==8)level=v->val.u8;
  return true;
 }
 unsigned getEndPointId(){return 1;}
 bool getAttributeVal(uint32_t cluster,uint32_t,esp_matter_attr_val_t* val){if(cluster==6)val->val.b=on;else val->val.u8=level;return true;}
 bool setOnOff(bool value){on=value;return true;}
 bool setLevel(uint8_t value){level=value;return true;}
 bool getOnOff(){return on;} uint8_t getLevel(){return level;}
};
struct ReadOnlySensor : MatterDimmablePlugin {};
namespace chip { namespace DeviceLayer {
struct Manager { bool TryLockChipStack(){return true;} void UnlockChipStack(){} };
inline Manager& PlatformMgr(){static Manager manager;return manager;}
}}
struct ArdMatter {
 using Operation=ArdMatterFields::Operation;
 int prepareCommand(uint32_t,uint32_t,const esp_matter_attr_val_t*,Operation&,uint32_t&);
 using V=ArdJSON::JSONVar; using Kind=ArdMatterFields::Kind;
 struct Entry { ArdMatter* owner; MatterDimmablePlugin* endpoint=nullptr; Kind kind=Kind::Dimmer; uint16_t field=0; };
 bool publish(Entry&,const V&,const V&);
 template<class T> struct Native;
 bool _localReport=false,_accepting=true; unsigned _queued=0; int _mux=0;
 ArdMatterFields::Command _commands[8]{}; uint8_t _head=0;
 Operation operation{}; uint32_t raw=0;
 bool enqueue(Entry&,Operation op,uint32_t value){operation=op;raw=value;++_queued;return true;}
};
'''
main = r'''
int main(){
 ArdMatter owner; ArdMatter::Entry entry{&owner};
 ArdMatter::Native<MatterDimmablePlugin> dimmer(&entry);
 esp_matter_attr_val_t value{};
 value.val.b=false;
 assert(dimmer.attributeChangeCB(1,6,0,&value));
 assert(owner.operation==ArdMatter::Operation::OnOff && owner.raw==0);
 // HA/SDK restores remembered brightness after switching off: remain off.
 value.val.u8=195;
 assert(dimmer.attributeChangeCB(1,8,0,&value));
 assert(owner.operation==ArdMatter::Operation::Level && owner.raw==0);
 assert(dimmer.getLevel()==195 && !dimmer.getOnOff());
 owner._queued=0;
 entry.endpoint=&dimmer;
 auto definition=ArdJSON::JSON.parse(R"({"max":100})");
 assert(owner.publish(entry,definition,ArdMatter::V(0)));
 assert(dimmer.getLevel()==195 && !dimmer.getOnOff());
 value.val.b=true;
 assert(dimmer.attributeChangeCB(1,6,0,&value));
 assert(owner.operation==ArdMatter::Operation::Level && owner.raw==195);
 owner._queued=0;
 assert(owner.publish(entry,definition,ArdMatter::V(77)));
 assert(dimmer.getLevel()==195); // No quantization echo to 196 during SDK transition.
 value.val.u8=100;
 assert(dimmer.attributeChangeCB(1,8,0,&value));
 assert(owner.raw==100);
 // A newer SDK command must block an older application report under stack lock.
 owner._head=7; owner._queued=2;
 owner._commands[7].field=1; owner._commands[0].field=entry.field;
 assert(!owner.publish(entry,definition,ArdMatter::V(0)));
 assert(dimmer.getOnOff() && dimmer.getLevel()==100 && !owner._localReport);
 owner._commands[0].field=2;
 assert(owner.publish(entry,definition,ArdMatter::V(0)));
 assert(!dimmer.getOnOff() && dimmer.getLevel()==100);
 owner._localReport=true;
 unsigned count=owner._queued;
 value.val.b=false;
 assert(dimmer.attributeChangeCB(1,6,0,&value));
 assert(owner._queued==count);
 owner._localReport=false; owner._queued=8;
 value.val.b=true;
 assert(!dimmer.attributeChangeCB(1,6,0,&value));
 assert(!dimmer.getOnOff());
 // Sensor adapters bypass writable attribute decoding even with a full queue.
 ArdMatter::Native<ReadOnlySensor> sensor(&entry);
 value.val.u8=17;
 assert(sensor.attributeChangeCB(2,8,0,&value));
 assert(owner._queued==8 && sensor.getLevel()==17);
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-matter-dimmer-') as directory:
    path=Path(directory)
    (path/'test.cpp').write_text(fixture+source[a:b]+source[source.index("inline bool ArdMatter::publish("):]+main)
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'tests/host'),'-I'+str(root/'src'),str(path/'test.cpp'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
print('PASS: actual Matter dimmer callback preserves off state and remembered brightness')
