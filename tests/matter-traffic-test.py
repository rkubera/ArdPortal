"""Verify human-facing Matter command/state text with actual control values."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=r'''
#define ARDPORTAL_DEFINE_CONTROL_DATA
#define ARDPORTAL_ENABLE_CONTROLS 1
#include "MatterTraffic.h"
#include <cassert>
uint32_t fakeMillis=0;
int main(){
 using namespace ArdMatterTraffic;String longName="Z";for(int i=0;i<50;++i)longName+="ą";assert(clip(longName,80).length()==79);
 using O=ArdMatterFields::Operation;
 V outlet=ArdJSON::JSON.parse(R"({"id":"outlet","name":"Desk socket","type":"switch","class":"outlet"})");
 auto text=received(outlet,{0,O::OnOff,1},V(true),true);
 assert(text.indexOf("Desk socket")>=0&&text.indexOf("On")>=0&&text.indexOf("OK")>=0);
 assert(received(outlet,{0,O::OnOff,1},V(false),false).indexOf("Rejected")>=0);
 assert(reported(outlet,V(false)).indexOf("Off")>=0);
 V temp=ArdJSON::JSON.parse(R"({"id":"temp","name":"Room","type":"text","class":"temperature"})");
 assert(summary(temp,V(21.5))=="21.5 °C");assert(summary(temp,V(nullptr))=="Unavailable");
 V climate=ArdJSON::JSON.parse(R"({"type":"climate","name":"Heating"})");
 auto targets=ArdJSON::JSON.parse(R"({"mode":"heat","temperature":22,"temperature_low":20,"temperature_high":25})");
 text=summary(climate,targets);assert(text.indexOf("Target: 22 °C")>=0&&text.indexOf("Heat: 20 °C")>=0&&text.indexOf("Cool: 25 °C")>=0);
 auto light=ArdJSON::JSON.parse(R"({"state":"ON","brightness":128,"color":{"r":255,"g":0,"b":0}})");
 text=summary(V::object(),light);assert(text.indexOf("Brightness: 50%")>=0&&text.indexOf("Color: 255, 0, 0")>=0);
 for(unsigned i=0;i<=unsigned(O::Stop);++i)assert(*action(O(i),1));
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-matter-traffic-') as directory:
 cpp=Path(directory)/'test.cpp';cpp.write_text(source);exe=Path(directory)/'test'
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'src'),'-I'+str(root/'tests/host'),str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: human Matter actions, accepted/rejected commands, named controls, units, climate targets and color/brightness')
