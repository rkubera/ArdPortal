"""Exercise independent policy/sleep axes, admission, deadlines and error recovery."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
fixture=r'''
#include "PowerPolicy.h"
#include <cassert>
using namespace ArdPowerDetail;
using Policy=ArdPowerPolicy;using Sleep=ArdPowerSleepType;using State=ArdPowerState;using Blocker=ArdPowerBlocker;
struct Backend {
 static uint32_t tick,idled,slept;static unsigned prepares,deepCalls,lightCalls;
 static bool failApply,failSleep,lightSupported;
 bool wakePinsActive(){return false;}bool interruptOnly(){return false;}bool hasSignals(){return false;}
 bool hasSources(){return false;}bool deepSourcesReady(){return true;}
 uint32_t takeSignals(){return 0;}bool describeSignal(uint8_t,ArdPowerEvent&){return false;}
 bool supportsSleep(Sleep type){return type!=Sleep::LightSleep||lightSupported;}
 static uint32_t now(){return tick;}
 static bool validDeepSleep(uint32_t ms){return ms&&ms<=100000;}
 State apply(Sleep type,bool,bool connected){return failApply?State::Error:type==Sleep::None?State::Awake:connected?State::ConnectedSleep:State::ModemSleep;}
 void idle(uint32_t ms){idled=ms;tick+=ms;}
 State lightSleep(uint32_t ms){++lightCalls;tick+=ms;return State::LightSleep;}
 bool deepSleep(uint32_t ms){++deepCalls;slept=ms;return !failSleep;}
};
uint32_t Backend::tick=0,Backend::idled=0,Backend::slept=0;
unsigned Backend::prepares=0,Backend::deepCalls=0,Backend::lightCalls=0;
bool Backend::failApply=false,Backend::failSleep=false,Backend::lightSupported=true;
using Power=Manager<Backend>;
void prepare(void*){++Backend::prepares;}
bool allowPrepare=false;unsigned admissions=0,cleanups=0;
bool admit(void*){++admissions;return allowPrepare;}
void cleanup(void*){++cleanups;}
Power::Options options(Policy policy,Sleep sleep){Power::Options o;o.policy=policy;o.sleep=sleep;o.sleepDelayMs=0;o.deepSleepMs=1000;return o;}
int main(){
 Context online;online.wifiActive=true;online.wifiConnected=true;online.beforeDeepSleep=prepare;
 Power p;p.service(online);assert(p.policy()==Policy::AlwaysOn&&p.sleepType()==Sleep::None&&p.state()==State::Awake);
 // AlwaysOn suppresses every configured sleep type and refuses explicit requests.
 for(unsigned type=0;type<=unsigned(Sleep::DeepSleep);++type){
  assert(p.configure(options(Policy::AlwaysOn,Sleep(type))));p.service(online);
  assert(p.state()==State::Awake&&!p.requestSleep(1)&&!Backend::deepCalls&&!Backend::lightCalls&&!Backend::idled);
 }
 // HTTP/WebSocket/AP traffic must disable radio power saving, including radio-only mode.
 auto interactiveOptions=options(Policy::EventDriven,Sleep::ConnectedSleep);interactiveOptions.connectedSleep.allowCpuSleep=false;
 assert(p.configure(interactiveOptions));
 Blocker interactiveBlockers[]={Blocker::Http,Blocker::WebSocket,Blocker::AccessPoint};
 for(auto blocker:interactiveBlockers){
  online.blockers=uint16_t(blocker);p.service(online);assert(p.state()==State::Awake&&!Backend::idled);
 }
 online.blockers=0;p.service(online);assert(p.state()==State::ModemSleep);
 // Event-driven modem sleep affects the radio; connected sleep waits on deadlines.
 auto radioOnly=options(Policy::EventDriven,Sleep::ConnectedSleep);radioOnly.connectedSleep.allowCpuSleep=false;
 assert(p.configure(radioOnly));assert(p.wakeIn(7));p.service(online);
 assert(!Backend::idled&&p.state()==State::ModemSleep);
 assert(p.setSleepType(Sleep::ConnectedSleep));p.service(online);assert(!Backend::idled&&p.state()==State::ModemSleep);
 assert(p.setPolicy(Policy::Manual)&&p.requestSleep(1));p.service(online);
 assert(p.state()==State::ModemSleep&&Backend::idled==1);p.cancelSleep();Backend::idled=0;
 radioOnly.connectedSleep.allowCpuSleep=true;assert(p.configure(radioOnly));p.service(online);assert(!Backend::idled);
 assert(p.wakeIn(7));p.service(online);assert(Backend::idled==7&&p.state()==State::ConnectedSleep);
 Backend::idled=0;p.service(online);assert(!Backend::idled);
 p.notifyActivity();p.service(online);assert(!Backend::idled);
 Backend::tick=UINT32_MAX-3;assert(p.wakeIn(7));p.service(online);assert(Backend::idled==7);
 assert(!p.wakeIn(uint32_t(INT32_MAX)+1));
 // Manual connected sleep only adds bounded waits after an explicit request.
 assert(p.configure(options(Policy::Manual,Sleep::ConnectedSleep)));Backend::idled=0;p.service(online);assert(!Backend::idled);
 assert(p.requestSleep(25));p.service(online);assert(Backend::idled==20&&p.sleepPending());
 p.service(online);assert(Backend::idled==5);Backend::idled=0;p.service(online);assert(!p.sleepPending()&&!Backend::idled&&p.state()!=State::ConnectedSleep);
 // Every portal blocker defers queued manual deep sleep without closing transports.
 assert(p.configure(options(Policy::Manual,Sleep::DeepSleep)));
 for(uint16_t bit=1;bit<=uint16_t(Blocker::Event);bit<<=1){
  online.blockers=bit;assert(p.requestSleep(1000));p.service(online);
  assert(p.sleepPending()&&!Backend::deepCalls&&!Backend::prepares);p.cancelSleep();
 }
 online.blockers=0;
 assert(p.lock()&&p.lock()&&p.requestSleep(1000));p.service(online);assert(p.blockedBy(Blocker::Application));
 assert(p.unlock());p.service(online);assert(p.sleepPending());assert(p.unlock()&&!p.unlock());
 p.service(online);assert(!p.sleepPending()&&Backend::slept==1000&&Backend::prepares==1);
 assert(!p.requestSleep(100001));assert(p.requestSleep()&&!p.requestSleep());p.cancelSleep();
 // Automatic deep sleep uses its configured period, shortened by a wake deadline.
 Backend::tick=0;auto automatic=options(Policy::EventDriven,Sleep::DeepSleep);automatic.sleepDelayMs=50;automatic.connectedSleep.allowCpuSleep=false;
 assert(p.configure(automatic));assert(p.wakeIn(200));unsigned count=Backend::deepCalls;
 Backend::tick=49;p.service(online);assert(Backend::deepCalls==count);
 Backend::tick=50;p.service(online);assert(Backend::deepCalls==count+1&&Backend::slept==150);
 assert(p.configure(options(Policy::EventDriven,Sleep::DeepSleep)));assert(p.wakeIn(0));count=Backend::deepCalls;p.service(online);assert(Backend::deepCalls==count);
 p.notifyActivity();p.service(online);assert(Backend::slept==1000);
 // RAM timers cannot be silently reset on every automatic deep-sleep boot.
 assert(!p.setTimer(1,10));assert(p.setSleepType(Sleep::ConnectedSleep));assert(p.setTimer(1,10));
 assert(!p.setSleepType(Sleep::DeepSleep)&&p.sleepType()==Sleep::ConnectedSleep);assert(p.cancelTimer(1));
 assert(p.setSleepType(Sleep::DeepSleep));
 // Zigbee retains ownership of CPU sleep, including automatic deep sleep.
 online.zigbee=true;count=Backend::deepCalls;p.service(online);assert(Backend::deepCalls==count&&p.blockedBy(Blocker::Zigbee));online.zigbee=false;
 // Connected Zigbee admits cooperative sleep, but standalone/deep stay blocked.
 Context sleepy;sleepy.zigbee=true;sleepy.zigbeeSleeping=true;
 assert(p.configure(options(Policy::EventDriven,Sleep::ConnectedSleep)));assert(p.wakeIn(5));
 p.service(sleepy);assert(!p.blockedBy(Blocker::Zigbee)&&p.state()==State::ConnectedSleep);
 auto zradio=options(Policy::EventDriven,Sleep::ConnectedSleep);zradio.connectedSleep.allowCpuSleep=false;
 assert(p.configure(zradio));Backend::idled=0;p.service(sleepy);assert(p.state()==State::ModemSleep&&!Backend::idled);
 Backend::failApply=true;p.service(sleepy);assert(p.state()==State::Error);Backend::failApply=false;
 sleepy.wifiActive=true;sleepy.wifiConnected=false;zradio.connectedSleep.allowCpuSleep=true;
 assert(p.configure(zradio));p.service(sleepy);assert(p.state()!=State::ConnectedSleep);
 sleepy.wifiConnected=true;p.service(sleepy);assert(p.state()==State::ConnectedSleep);
 assert(p.setSleepType(Sleep::LightSleep));p.service(sleepy);assert(!p.blockedBy(Blocker::Zigbee)&&p.blockedBy(Blocker::Network));
 sleepy.wifiActive=false;unsigned lightBefore=Backend::lightCalls;assert(p.wakeIn(2));p.service(sleepy);
 assert(!p.blockers()&&Backend::lightCalls==lightBefore); // Native stack owns light sleep.
 sleepy.zigbeeDeepSleep=true;
 assert(p.setSleepType(Sleep::DeepSleep));count=Backend::deepCalls;p.service(sleepy);assert(Backend::deepCalls==count+1);
 sleepy.zigbeeDeepSleep=false;
 assert(p.setSleepType(Sleep::DeepSleep));p.service(sleepy);assert(p.blockedBy(Blocker::Zigbee));
 // Failed deep sleep is not retried on every loop; activity or a new request rearms it.
 Backend::failSleep=true;p.service(online);assert(p.state()==State::Error);count=Backend::deepCalls;
 p.service(online);assert(Backend::deepCalls==count&&p.state()==State::Error);
 Backend::failSleep=false;p.notifyActivity();p.service(online);assert(Backend::deepCalls==count+1);
 // Standalone light sleep requires the Wi-Fi radio to be off.
 auto standalone=options(Policy::EventDriven,Sleep::LightSleep);standalone.connectedSleep.allowCpuSleep=false;assert(p.configure(standalone));assert(p.wakeIn(10));
 p.service(online);assert(!Backend::lightCalls&&p.blockedBy(Blocker::Network));
 Context offline;p.service(offline);assert(Backend::lightCalls==1&&p.state()==State::LightSleep);
 Backend::lightSupported=false;assert(!p.configure(options(Policy::Manual,Sleep::LightSleep)));Backend::lightSupported=true;
 auto invalid=options(Policy::Manual,Sleep::None);invalid.policy=Policy(255);assert(!p.configure(invalid));
 invalid=options(Policy::Manual,Sleep::None);invalid.sleep=Sleep(255);assert(!p.configure(invalid));
 invalid=options(Policy::Manual,Sleep::None);invalid.maxIdleMs=101;assert(!p.configure(invalid));
 invalid.maxIdleMs=0;assert(!p.configure(invalid));invalid.maxIdleMs=20;invalid.sleepDelayMs=UINT32_MAX;assert(!p.configure(invalid));
 invalid=options(Policy::Manual,Sleep::DeepSleep);invalid.deepSleepMs=0;assert(!p.configure(invalid));
 // A failed native admission keeps the request queued without closing MQTT.
 Context guarded;guarded.zigbee=true;guarded.zigbeeDeepSleep=true;
 guarded.prepareDeepSleep=admit;guarded.afterDeepSleepAttempt=cleanup;guarded.beforeDeepSleep=prepare;
 Power z;assert(z.configure(options(Policy::Manual,Sleep::DeepSleep)));assert(z.requestSleep(1000));
 count=Backend::deepCalls;unsigned closed=Backend::prepares;
 z.service(guarded);assert(admissions==1&&!cleanups&&z.sleepPending()&&Backend::prepares==closed&&Backend::deepCalls==count);
 allowPrepare=true;Backend::failSleep=true;z.service(guarded);
 assert(cleanups==1&&!z.sleepPending()&&z.state()==State::Error&&Backend::deepCalls==count+1);
 z.service(guarded);assert(cleanups==1&&Backend::deepCalls==count+1);
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-power-') as directory:
 d=Path(directory);source=d/'test.cpp';source.write_text(fixture);executable=d/'test'
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'src'),str(source),'-o',str(executable)],check=True)
 subprocess.run([str(executable)],check=True)
print('PASS: independent policy/sleep axes, manual requests, automatic deep sleep, blockers, deadlines, light sleep admission, timer compatibility and failure recovery')
