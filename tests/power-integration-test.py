"""Run the actual portal sleep admission against pending module work."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'src/ArdPowerImpl.h').read_text().replace('#pragma once','').replace('#include "ArdPortalDeclarations.h"','')
fixture=r'''
#include "PowerPolicy.h"
#include <cassert>
using namespace ArdPowerDetail;
static unsigned closes=0,sleeps=0;
struct Backend {
 bool supportsSleep(ArdPowerSleepType){return true;}
 ArdPowerState lightSleep(uint32_t){return ArdPowerState::LightSleep;}
 bool wakePinsActive(){return false;}bool interruptOnly(){return false;}bool hasSignals(){return false;}
 bool hasSources(){return false;}bool deepSourcesReady(){return true;}
 uint32_t takeSignals(){return 0;}bool describeSignal(uint8_t,ArdPowerEvent&){return false;}

 static uint32_t now(){return 0;}
 static bool validDeepSleep(uint32_t){return true;}
 ArdPowerState apply(ArdPowerSleepType,bool,bool){return ArdPowerState::Awake;}
 void idle(uint32_t){}
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
 Manager<Backend> _power;
 bool _started=true,_configurationReady=true,storage=false,ota=false,_saveQueued=false,_saveRetryPending=false;
 bool _httpWaitingStorage=false,_rebootPending=false,_beforeRestartRunning=false,_apActive=false;
 bool _attempting=false,_scanning=false,_scanRequested=false,_httpConfigUpload=false,trial=false,registration=false;
 Socket _http;
 struct {Socket client;} _eventWs,_consoleWs,_zigbeeWs;
 enum class MqttState {Connected,Connecting};
 struct Mqtt {
   bool saving=false,sending=false,_pingPending=false;unsigned _rxSize=0;
   MqttState _mqttState=MqttState::Connected;
   unsigned incoming=0;
   Mqtt& mqttTransport(){return *this;}unsigned available(){return incoming;}
   bool trialSavePending(){return saving;}bool sendBusy(){return sending;}
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
#else
 assert(p._power.lock());p.servicePower();assert(!sleeps&&!closes);
 assert(p._power.unlock());p.servicePower();assert(sleeps==1&&closes==1);
#endif
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-power-integration-') as directory:
 directory=Path(directory);cpp=directory/'test.cpp';cpp.write_text(fixture+source+main)
 for network,zigbee in ((0,0),(1,0),(1,1)):
  flags=[f'-DARDPORTAL_ENABLE_ZIGBEE={zigbee}']
  for flag in ('WIFI','WWW','MQTT','DYNAMIC_PAGES','CONSOLE'):
   flags.append(f'-DARDPORTAL_ENABLE_{flag}={network}')
  flags.append(f'-DARDPORTAL_SUPPORT_WEBSOCKET={network}')
  flags.append(f'-DARDPORTAL_ENABLE_HA={network}')
  executable=directory/'test'
  subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                  '-I'+str(root/'src'),*flags,str(cpp),'-o',str(executable)],check=True)
  subprocess.run([str(executable)],check=True)
print('PASS: actual portal power guards for startup, saves, OTA, AP, sockets, MQTT, application locks and mixed Zigbee/Wi-Fi')
