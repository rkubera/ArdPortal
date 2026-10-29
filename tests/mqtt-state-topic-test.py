from pathlib import Path
import subprocess, tempfile

root = Path(__file__).resolve().parents[1]
fixture = r'''
#include <cassert>
#include "MqttFieldTopics.h"
using ArdMqttFieldTopics::stateTopic;
uint32_t fakeMillis=0;
using V=ArdJSON::JSONVar;
'''
main = r'''
int main() {
 for(const char* type : {"sensor", "switch", "light", "event", "tag", "device_trigger", "lawn_mower"}) {
  V f=V::object();f["type"]=type;f["id"]="reading";
  const char* key=(String(type)=="tag"||String(type)=="device_trigger")?"topic":String(type)=="lawn_mower"?"activity_state_topic":"state_topic";
  assert(stateTopic(f,"stat/device/reading")=="stat/device/reading");
  f["ha"]=V::object();f["ha"][key]="";
  assert(stateTopic(f,"stat/device/reading")=="stat/device/reading");
  f["ha"][key]="$state";
  assert(stateTopic(f,"stat/device/reading")=="stat/device/reading");
  f["ha"][key]="custom/device/state";
  assert(stateTopic(f,"stat/device/reading")=="custom/device/state");
  f["mqtt"]=V::object();f["mqtt"]["state_topic"]="mqtt/device/state";
  assert(stateTopic(f,"stat/device/reading")=="mqtt/device/state");
  f["mqtt"]["state_topic"]="$state";
  assert(stateTopic(f,"stat/device/reading")=="stat/device/reading");
  f["mqtt"]["state_topic"]="";
  assert(stateTopic(f,"stat/device/reading")=="stat/device/reading");
  f.remove("ha");f["mqtt"]["state_topic"]="mqtt/only";
  assert(stateTopic(f,"stat/device/reading")=="mqtt/only");
 }
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-state-topic-') as tmp:
    tmp=Path(tmp)
    (tmp/'test.cpp').write_text(fixture+main)
    for ha in (0, 1):
        subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-DARDPORTAL_ENABLE_MQTT=1','-DARDPORTAL_ENABLE_HA='+str(ha),'-I'+str(root/'tests/host'),'-I'+str(root/'src'),str(tmp/'test.cpp'),str(root/'src/JsonStream.cpp'), str(root/'src/JsonDocumentStream.cpp'), '-o',str(tmp/'test')],check=True)
        subprocess.run([str(tmp/'test')],check=True)
print('PASS: configured state and event topics, missing/empty/$state fallbacks across entity types')
