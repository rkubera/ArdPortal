#include "Arduino.h"
#include "ESP8266WiFi.h"
#include "ArdFSHostVolume.h"
#include <cassert>
#include "Updater.h"
#include "WiFiClientSecure.h"
#define ARDPORTAL_TEST_ALLOCATE testAllocate
#define ARDPORTAL_DEFINE_HA_DATA
#define private public
#include "../../src/ArdPortalDeclarations.h"
#undef private
#undef ARDPORTAL_DEFINE_HA_DATA
#include "ArdPortal.h"
#include "../../src/WebSocketAccept.h"
#include "../../src/ConfigJson.h"
#include "../../src/Language.h"
long allocationBudget=-1;size_t allocations=0;
void* testAllocate(size_t bytes) noexcept {if(allocationBudget==0)return nullptr;if(allocationBudget>0)--allocationBudget;++allocations;return std::malloc(bytes);}
uint32_t fakeMillis = 0;
int ntpStarts = 0;
String lastNtpServer1, lastNtpServer2;
time_t fakeUnixTime = 1800000000;
int WiFiClientSecure::tlsAttempts = 0, WiFiClientSecure::tlsWrites = 0;
String WiFiClientSecure::lastHost;
bool WiFiClientSecure::tlsConnectResult = true;
SerialStub Serial;
WiFiStub WiFi;
EspStub ESP;
UpdateStub Update;
bool failMount = false, failFormat = false;
bool WiFiClient::connectResult = true;
std::deque<WiFiClient> WiFiServer::incoming;
std::map<std::string, std::vector<uint8_t>> fakeFiles;
bool failFileWrite = false, failRename = false;


void drain(ArdPortal& p){if(p._savePending&&!p._saveQueued)p.flushPortalAndAppConfig();for(int i=0;i<10000&&(p._savePending||p._storage.busy()||!p.portalAndAppConfigReady());++i){fakeMillis+=10;p.serviceStorage(fakeMillis);}assert(!p._storage.busy()&&!p._savePending);}
int main(){
  {
    ArdPortal discovery;
    auto field=ArdJSON::JSON.parse(R"({"id":"toggle","name":"Toggle","type":"switch","default":false})");
    auto config=ArdJSON::JSON.parse(discovery._homeAssistant.discoveryConfig(field));
    assert(config.isValid()&&!config.hasOwnProperty("optimistic"));
    field["ha"]["optimistic"]=true;
    config=ArdJSON::JSON.parse(discovery._homeAssistant.discoveryConfig(field));
    assert(config["optimistic"].asBool());
    assert(config["state_topic"].asString().length()>0&&config["command_topic"].asString().length()>0);
    assert(config["icon"].asString()=="mdi:toggle-switch");
    field["icon"]="mdi:fan";
    config=ArdJSON::JSON.parse(discovery._homeAssistant.discoveryConfig(field));
    assert(config["icon"].asString()=="mdi:fan");
  }
using V=ArdJSON::JSONVar;ArdPortal p;assert(p.begin());drain(p);
for(int i=0;i<300;++i)p._appConfig[String("setting")+String(i)]=i;
allocationBudget=12;size_t before=allocations;assert(p.setAppConfigValue("setting1",17));assert(allocations-before<12);allocationBudget=-1;assert(p._pendingAppPatch&&p._pendingApp.length()==1);assert(p.getAppConfigValue("setting1").asDouble()==1);
assert(p.setAppConfigValue("setting2",18));assert(p._pendingApp.length()==2);drain(p);assert(p._appConfig.length()==300&&p.getAppConfigValue("setting1").asDouble()==17);
assert(p.removeAppConfigValue("setting3"));assert(p._pendingApp.length()==1);assert(p.setAppConfigValue("setting4",44));drain(p);assert(!p._appConfig.hasOwnProperty("setting3")&&p._appConfig.length()==299);
// Changing portal settings while field edits are pending must preserve all values.
assert(p.setAppConfigValue("setting5",55));auto config=p.getPortalConfig();config.deviceDescription="changed";assert(p.setPortalConfig(config));drain(p);if(p._pendingReady){p._config=std::move(p._pending);p.applyPendingApp();p._pendingReady=false;}assert(p._appConfig.length()==299&&p.getAppConfigValue("setting5").asDouble()==55);
assert(p.startAppConfigPageRegistration(String(R"({"id":"test","name":"Test","order":0,"fields":[{"id":"setting1","name":"Slider","type":"slider","min":0,"max":100} ]})")));
for(int i=0;i<1000&&p.appConfigPageRegistrationBusy();++i)p.serviceAppConfigPageRegistration();if(p._dynamic.count()!=1){auto e=p.appConfigRegistrationError();fprintf(stderr,"reg %s %s %s busy=%d\n",e.stage,e.fieldId.c_str(),e.reason.c_str(),int(p.appConfigPageRegistrationBusy()));}assert(p._dynamic.count()==1);
int callbacks=0;p.onAppConfigValueChanged([&](const String&,const V&,ArdPortal::ChangeSource){++callbacks;});
p._request="POST /api/app HTTP/1.1\r\nContent-Length: 41\r\n\r\n{\"page\":\"test\",\"values\":{\"setting1\":22}}";p.handleHttp();assert(p._response.indexOf("202")>=0);assert(p._pendingAppPatch&&p._pendingApp.length()==1);assert(p.getAppConfigValue("setting1").asDouble()==22);assert(callbacks==1);p.closeHttp();drain(p);assert(callbacks==1&&p._appConfig["setting1"].asDouble()==22);
// MQTT persists only its affected fields and acknowledges after verification.
assert(p._appControls.acceptMqttState("setting1",V(23)));p._appControls.prepareMqtt(fakeMillis);assert(p._pendingAppPatch&&p._pendingApp.length()==1);drain(p);assert(p._appConfig["setting1"].asDouble()==23);
ArdPortal restored;assert(restored.begin());drain(restored);assert(restored._appConfig.length()==299&&restored.getAppConfigValue("setting1").asDouble()==23&&!restored._appConfig.hasOwnProperty("setting3"));
}
