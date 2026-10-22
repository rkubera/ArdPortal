"""Exercise the actual HTTP factory-reset branch with host-side collaborators."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "src/ArdPortalImpl.h").read_text()
start = source.index('  if (post && ardFlashEqual(path,F("/api/factory-reset"))) {')
end = source.index('  if (post && ardFlashEqual(path,F("/api/storage/format"))) {', start)
branch = source[start:end]
fixture = r'''
#include <cassert>
#include <functional>
#include "Arduino.h"
#include "FlashText.h"
namespace ArdJSON { struct JSONVar { static JSONVar object() { return {}; } }; }
namespace ArdUILanguage { enum class Key { s_132, s_124, s_107 }; }
enum class ChangeSource { FactoryReset };
struct PortalConfig { String deviceName, apName; };
enum class ZigbeeOperation { FactoryReset };
struct ZigbeeMock { unsigned resets=0; bool request(ZigbeeOperation,int){++resets;return true;} };
struct Portal {
 ZigbeeMock _zigbee;int _zigbeeOptions=0;
 void log(const String&){}

 bool _configurationReady=true, _rebootPending=false, busy=false, trial=false;
 bool _resetAfterSave=false, _httpWaitingStorage=false, accept=true;
 String _request="POST /api/factory-reset HTTP/1.1\r\n\r\nconfirm=RESET";
 std::function<void()> _beforeFactoryReset;
 int status=0, phase=0, scheduled=0;
 bool portalAndAppConfigBusy(){return busy;}
 bool mqttTrialActive(){return trial;}
 String defaultDeviceName(){return "device";}
 void replyMessage(int code, ArdUILanguage::Key){status=code;}
 void closeMqtt(){phase=2;}
 bool scheduleConfig(const PortalConfig& defaults, ArdJSON::JSONVar, ChangeSource, bool immediate){
   assert(defaults.deviceName=="device" && defaults.apName=="device" && immediate);
   assert(phase==2); ++scheduled; return accept;
 }
 void request(const String& method="POST", const String& path="/api/factory-reset") {
  const bool post=ardFlashEqual(method,F("POST"));
'''
main = r'''
 }
};
int main(){
 // No comparison with existing/default values may suppress reset notification.
 Portal p;int calls=0;
 p._beforeFactoryReset=[&]{assert(p.phase==0 && p.scheduled==0); ++calls;p.phase=1;};
 p.request();assert(calls==1 && p.scheduled==1 && p._resetAfterSave && p._httpWaitingStorage);
#if ARDPORTAL_ENABLE_ZIGBEE
 assert(p._zigbee.resets==1);
#endif
 for(int guard=0;guard<5;++guard){
   Portal q;q._beforeFactoryReset=[&]{++calls;};
   if(guard==0)q._configurationReady=false;
   if(guard==1)q.busy=true;
   if(guard==2)q.trial=true;
   if(guard==3)q._rebootPending=true;
   if(guard==4)q._request="POST / HTTP/1.1\r\n\r\nconfirm=WRONG";
   q.request();assert(q.scheduled==0 && calls==1 && q._zigbee.resets==0);
 }
 Portal noHandler;noHandler.request();assert(noHandler.scheduled==1);
 Portal failure;failure.accept=false;failure._beforeFactoryReset=[&]{++calls;};
 failure.request();assert(calls==2 && failure.status==500 && !failure._resetAfterSave && failure._zigbee.resets==0);
 Portal other;other._beforeFactoryReset=[&]{++calls;};
 other.request("POST","/api/storage/format");other.request("POST","/api/restart");
 assert(calls==2 && other.scheduled==0);
 puts("PASS: factory-reset callback ordering, unchanged defaults, guards, missing handler, save rejection and other routes");
}
'''
with tempfile.TemporaryDirectory(prefix="ardportal-reset-test-") as tmp:
    tmp = Path(tmp)
    (tmp / "test.cpp").write_text(fixture + branch + main)
    for zigbee in (0, 1):
        subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                        "-DARDPORTAL_ENABLE_ZIGBEE=" + str(zigbee),
                        "-fsanitize=address,undefined", "-I" + str(root / "tests/host"), "-I" + str(root / "src"),
                        str(tmp / "test.cpp"), "-o", str(tmp / "test")], check=True)
        subprocess.run([str(tmp / "test")], check=True)
