"""Verify that accepted newer intents supersede deferred Zigbee persistence."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/ArdPortalImpl.h').read_text();start=s.index('bool ArdPortal::scheduleAppPatch(');end=s.index('\nString ArdPortal::saveDocumentSlice',start)
fixture=r'''
#include <cassert>
#include "ArdJSON.h"
#include "AppConfigFields.h"
#define ARDPORTAL_ENABLE_ZIGBEE 1
#define ARDPORTAL_ENABLE_DYNAMIC_PAGES 1
uint32_t fakeMillis=0;
using V=ArdJSON::JSONVar;
enum class ChangeSource{Zigbee,Portal,Application};
struct Registry{size_t indexOf(const String& key){return key=="relay"?0:1;}String idAt(size_t field){return field==0?"relay":"other";}};
namespace ArdDeviceName{String ap(const String& name){return name;}}
class ArdPortal{public:
 static constexpr size_t MaxAppConfigBytes=65536;
 bool _configurationWritable=true,_saveRetryPending=false,_resetAfterSave=false;
 bool _configurationReady=true,_saveQueued=false,_pendingReady=false,_httpWaitingStorage=false,_rebootPending=false,_beforeRestartRunning=false,_zigbeeDrainingRestart=false,_savePending=false,_pendingAppPatch=false;
 struct Storage{bool mounted(){return true;}}_storage;
 struct Config{String deviceName,apName;}_pending,_config;
 Registry _dynamic;V _deferredApp=V::object(),_zigbeeAppQueue=V::object(),_pendingApp=V::object(),_appConfig=V::object();ChangeSource _saveSource=ChangeSource::Portal;uint32_t _dirtySince=0;
 bool otaActive(){return false;}bool mqttTrialActive(){return false;}ArdJSON::Limits snapshotLimits(){return ArdJSON::Limits();}
 bool scheduleAppPatch(V patch,ChangeSource source,bool coalesceSource=false);
};
'''
fixture+=s[start:end]+r'''
int main(){
 ArdPortal p;p._zigbeeAppQueue["relay"]=false;p._zigbeeAppQueue["other"]=7;V patch=V::object();patch["relay"]=true;
 assert(p.scheduleAppPatch(patch,ChangeSource::Portal));assert(!p._zigbeeAppQueue.hasOwnProperty("relay"));assert(p._zigbeeAppQueue["other"].asDouble()==7);assert(p._pendingApp["relay"].asBool());
 p._zigbeeAppQueue["relay"]=false;p._saveQueued=true;assert(!p.scheduleAppPatch(patch,ChangeSource::Portal));assert(p._zigbeeAppQueue.hasOwnProperty("relay"));
 p._saveQueued=false;p._savePending=false;assert(p.scheduleAppPatch(patch,ChangeSource::Zigbee));assert(p._zigbeeAppQueue.hasOwnProperty("relay"));
 p._configurationWritable=false;assert(!p.scheduleAppPatch(patch,ChangeSource::Application));assert(p._zigbeeAppQueue.hasOwnProperty("relay"));
 p._configurationWritable=true;p._saveRetryPending=true;patch["relay"]=false;
 assert(p.scheduleAppPatch(patch,ChangeSource::Application));assert(!p._zigbeeAppQueue.hasOwnProperty("relay"));
 assert(!p._deferredApp["relay"].asBool()&&p._pendingApp["relay"].asBool());
}
'''
with tempfile.TemporaryDirectory() as d:
 path=Path(d);(path/'test.cpp').write_text(fixture)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(root/'tests/memory-host'),'-I'+str(root/'src'),str(path/'test.cpp'),str(root/'src/JsonStream.cpp'), str(root/'src/JsonDocumentStream.cpp'), '-o',str(path/'test')],check=True)
 subprocess.run([str(path/'test')],check=True)
print('PASS: newer accepted intents remove only superseded Zigbee fields; rejected changes preserve queued persistence')
