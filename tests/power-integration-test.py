"""Run the actual portal sleep admission against pending module work."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'src/ArdPowerImpl.h').read_text().replace('#pragma once','').replace('#include "ArdPortalDeclarations.h"','')
fixture=r'''
#include "PowerPolicy.h"
#include <Arduino.h>
#include <cassert>
#if ARDPORTAL_ENABLE_MATTER && ARDPORTAL_ENABLE_CONSOLE_MESSAGES
#include "PowerDiagnostics.h"
#endif
using namespace ArdPowerDetail;
static unsigned closes=0,sleeps=0,idles=0;
uint32_t fakeMillis=0;
#define tick fakeMillis
struct Backend {
 bool supportsSleep(ArdPowerSleepType){return true;}
 ArdPowerState lightSleep(uint32_t){return ArdPowerState::LightSleep;}
 bool wakePinsActive(){return false;}bool interruptOnly(){return false;}bool hasSignals(){return false;}
 bool hasSources(){return false;}bool deepSourcesReady(){return true;}
 uint32_t takeSignals(){return 0;}bool describeSignal(uint8_t,ArdPowerEvent&){return false;}

 static uint32_t now(){return tick;}
 static bool validDeepSleep(uint32_t){return true;}
 ArdPowerState apply(ArdPowerSleepType,bool,bool connected){return connected?ArdPowerState::ConnectedSleep:ArdPowerState::Awake;}
 void idle(uint32_t){++idles;}
 bool deepSleep(uint32_t){assert(closes==sleeps+1);++sleeps;return true;}
};
#define WIFI_OFF 0
#define WL_CONNECTED 3
struct Wifi {int mode=1,statusCode=WL_CONNECTED;int getMode(){return mode;}int status(){return statusCode;}} WiFi;
struct Socket {bool value=false;explicit operator bool()const{return value;}};
struct Text {unsigned size=0;unsigned length()const{return size;}};
class ArdPortal {public:
 using ArdPower=Manager<Backend>;
 struct {bool ready=false,requested=false,deep=false,locked=false;
 bool powerSaving(bool enable,bool wantDeep){requested=enable;deep=wantDeep;return enable&&ready;}
 bool canDeepSleep(){return ready&&deep;}
 bool prepareDeepSleep(){locked=canDeepSleep();return locked;}
 void cancelDeepSleep(){locked=false;}
 } _zigbee;
 struct {bool ready=false,selected=false,busy=false;
 bool connected(){return ready;}
 bool powerSaving(bool enable,bool blocked){selected=enable;busy=blocked;return enable&&ready;}
 } _matter;
 Manager<Backend> _power;
#if ARDPORTAL_ENABLE_MATTER && ARDPORTAL_ENABLE_CONSOLE_MESSAGES
 ArdPowerDiagnostics _powerDiagnostics;
 void consoleLine(uint8_t channel,const String& text){assert(channel==0&&text.length());}
#endif
 uint32_t _httpSince=0;
 bool _started=true,_configurationReady=true,storage=false,ota=false,_saveQueued=false,_saveRetryPending=false;
 bool _httpWaitingStorage=false,_rebootPending=false,_beforeRestartRunning=false,_apActive=false;
 bool _attempting=false,_scanning=false,_scanRequested=false,_httpConfigUpload=false,trial=false,registration=false;
 Socket _http;
 struct {Socket client;} _eventWs,_consoleWs,_zigbeeWs,_matterWs;
 enum class MqttState {Connected,Connecting};
 struct Mqtt {
   bool saving=false,sending=false,_pingPending=false;unsigned _rxSize=0;
   MqttState _mqttState=MqttState::Connected;
   unsigned incoming=0;
   Mqtt& mqttTransport(){return *this;}unsigned available(){return incoming;}
   bool receiveBusy(){return _rxSize || _pingPending;}bool trialSavePending(){return saving;}bool sendBusy(){return sending;}
 } _mqttClient;
 struct {bool _stateDirty=false,_mqttAckPending=false,_mqttAckInFlight=false;} _appControls;
 struct {Text host;} _config;
 struct {unsigned _haDiscovery=0;} _homeAssistant;
 struct {unsigned fields=0;unsigned count(){return fields;}} _dynamic;
 bool portalAndAppConfigBusy(){return storage;}bool otaActive(){return ota;}
 bool mqttTrialActive(){return trial;}bool appConfigPageRegistrationBusy(){return registration;}
 void closeMqtt(){++closes;}
 void servicePower();void prepareZigbeePower();
};
'''
main=r'''
int main(){
 ArdPortal p;
 Manager<Backend>::Options options;options.policy=ArdPowerPolicy::Manual;options.sleep=ArdPowerSleepType::DeepSleep;assert(p._power.configure(options));
 bool* guards[]={&p.storage,&p.ota,&p._saveQueued,&p._saveRetryPending,&p._httpWaitingStorage,
 &p._rebootPending,&p._beforeRestartRunning,&p._apActive,
#if ARDPORTAL_ENABLE_WIFI
 &p._attempting,&p._scanning,&p._scanRequested,
#endif
#if ARDPORTAL_ENABLE_WWW
 &p._http.value,&p._httpConfigUpload,
#endif
#if ARDPORTAL_SUPPORT_WEBSOCKET
 &p._eventWs.client.value,&p._consoleWs.client.value,
#if ARDPORTAL_ENABLE_MATTER_CONSOLE
 &p._matterWs.client.value,
#endif
#endif
#if ARDPORTAL_ENABLE_MQTT
 &p.trial,&p._mqttClient.saving,&p._mqttClient.sending,&p._mqttClient._pingPending,
#endif
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
 &p.registration,
#endif
 };
 for(auto* guard:guards){
   *guard=true;assert(p._power.requestSleep(1000));p.servicePower();
   assert(!sleeps&&!closes&&p._power.sleepPending()&&p._power.blockers());
   p._power.cancelSleep();*guard=false;
 }
 #if ARDPORTAL_ENABLE_MQTT
 p._mqttClient.incoming=1;assert(p._power.requestSleep(1000));p.servicePower();assert(!sleeps);p._mqttClient.incoming=0;p._power.cancelSleep();
#endif
#if ARDPORTAL_ENABLE_HA
 p._config.host.size=1;p._dynamic.fields=1;
 assert(p._power.requestSleep(1000));p.servicePower();assert(!sleeps);
 p._homeAssistant._haDiscovery=1;p._appControls._stateDirty=true;
 p.servicePower();assert(!sleeps);p._appControls._stateDirty=false;
 p._power.cancelSleep();p._config.host.size=0;
#endif
#if ARDPORTAL_ENABLE_WWW
 p._httpSince=1;tick=500;assert(p._power.requestSleep(1000));p.servicePower();
 assert(!sleeps&&p._power.blockedBy(ArdPowerBlocker::Http));p._power.cancelSleep();p._httpSince=0;tick=0;
#endif
 p._started=false;assert(p._power.requestSleep(1000));p.servicePower();assert(!sleeps);
 p._started=true;p._configurationReady=false;p.servicePower();assert(!sleeps);
 p._configurationReady=true;
#if ARDPORTAL_ENABLE_ZIGBEE
 p.servicePower();assert(!sleeps&&p._power.blockedBy(ArdPowerBlocker::Zigbee));
 assert(p._zigbee.requested&&p._zigbee.deep);
 options.sleep=ArdPowerSleepType::ConnectedSleep;assert(p._power.configure(options));
 p.servicePower();assert(p._zigbee.requested&&p._power.blockedBy(ArdPowerBlocker::Zigbee));
 p._zigbee.ready=true;p.servicePower();assert(!p._power.blockedBy(ArdPowerBlocker::Zigbee));
 p.ota=true;p.servicePower();assert(!p._zigbee.requested&&p._power.blockedBy(ArdPowerBlocker::Ota));
 p.ota=false;assert(p._power.setPolicy(ArdPowerPolicy::AlwaysOn));
 p.servicePower();assert(!p._zigbee.requested);
#elif ARDPORTAL_ENABLE_MATTER
 p._matter.ready=true;p.servicePower();assert(!sleeps&&p._power.blockedBy(ArdPowerBlocker::Network));
 options.sleep=ArdPowerSleepType::ConnectedSleep;assert(p._power.configure(options));
 assert(p._power.requestSleep(1000));p.servicePower();
#if !ARDPORTAL_MATTER_THREAD || CONFIG_ENABLE_ICD_SERVER
 assert(!p._power.blockers()&&p._matter.selected);
 assert(p._power.lock());p.servicePower();assert(!p._matter.selected&&p._matter.busy);assert(p._power.unlock());
 p._power.cancelSleep();p.servicePower();assert(!p._matter.selected); // Manual policy needs a request.
 options.connectedSleep.allowCpuSleep=false;assert(p._power.configure(options));
 assert(p._power.requestSleep(1000));p.servicePower();assert(p._matter.selected); // Radio-only also admits ICD.
 options.connectedSleep.allowCpuSleep=true;options.policy=ArdPowerPolicy::EventDriven;options.sleepDelayMs=0;
 assert(p._power.configure(options));auto before=idles;p.servicePower();assert(idles>before);
 options.connectedSleep.allowCpuSleep=false;assert(p._power.configure(options));before=idles;
 p.servicePower();assert(idles==before&&p._matter.selected);
#else
 assert(p._power.blockedBy(ArdPowerBlocker::Network)&&!p._matter.selected);
#endif
 options.sleep=ArdPowerSleepType::LightSleep;assert(p._power.configure(options));
 p.servicePower();assert(p._power.blockedBy(ArdPowerBlocker::Network)&&!p._matter.selected&&!sleeps);
#else
 assert(p._power.lock());p.servicePower();assert(!sleeps&&!closes);
 assert(p._power.unlock());p.servicePower();assert(sleeps==1&&closes==1);
#endif
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-power-integration-') as directory:
 directory=Path(directory);cpp=directory/'test.cpp';cpp.write_text(fixture+source+main)
 for network,zigbee,matter,thread,icd,router in ((0,0,0,0,0,0),(1,0,0,0,0,0),(1,1,0,0,0,0),(1,0,1,0,0,0),(0,0,1,1,0,0),(0,0,1,1,1,0),(1,0,1,1,1,0),(0,0,1,1,0,1),(1,0,1,1,0,1)):
  flags=[f'-DARDPORTAL_MATTER_THREAD_ROUTER={router}',f'-DARDPORTAL_ENABLE_CONSOLE_MESSAGES={matter}',f'-DARDPORTAL_ENABLE_MATTER_CONSOLE={int(bool(matter and network))}',f'-DARDPORTAL_ENABLE_ZIGBEE={zigbee}',f'-DARDPORTAL_ENABLE_MATTER={matter}',f'-DARDPORTAL_MATTER_THREAD={thread}',f'-DCONFIG_ENABLE_ICD_SERVER={icd}']
  for flag in ('WIFI','WWW','MQTT','DYNAMIC_PAGES','CONSOLE'):
   flags.append(f'-DARDPORTAL_ENABLE_{flag}={network}')
  flags.append(f'-DARDPORTAL_SUPPORT_WEBSOCKET={network}')
  flags.append(f'-DARDPORTAL_ENABLE_HA={network}')
  executable=directory/'test'
  subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                  '-I'+str(root/'tests/host'),'-I'+str(root/'src'),*flags,str(cpp),'-o',str(executable)],check=True)
  subprocess.run([str(executable)],check=True)
print('PASS: actual portal power guards for startup, saves, OTA, AP, sockets, MQTT, application locks and mixed Zigbee/Wi-Fi')
