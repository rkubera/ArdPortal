from pathlib import Path
import subprocess, tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/HaEntities.h').read_text()
a = source.index('inline String stateTopic(')
b = source.index('\n/**', a)
helper = source[a:b]
fixture = r'''
#include <cassert>
#include "ArdJSON.h"
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
 }
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-state-topic-') as tmp:
    tmp=Path(tmp)
    (tmp/'test.cpp').write_text(fixture+helper+main)
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'tests/host'),'-I'+str(root/'src'),str(tmp/'test.cpp'),'-o',str(tmp/'test')],check=True)
    subprocess.run([str(tmp/'test')],check=True)
print('PASS: configured state and event topics, missing/empty/$state fallbacks across entity types')
