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
  {ArdPortal p;assert(String(p.getFirmwareVersion())=="1.0");
   auto f=ArdJSON::JSON.parse(R"({"id":"version_test","type":"switch","name":"Relay"})");
   auto d=ArdJSON::JSON.parse(p._homeAssistant.discoveryConfig(f));assert(d["device"]["sw_version"].asString()=="1.0");
   assert(!p.setFirmwareVersion(nullptr)&&!p.setFirmwareVersion("")&&!p.setFirmwareVersion("12345678901234567"));
   assert(p.setFirmwareVersion("2.3.4-beta"));d=ArdJSON::JSON.parse(p._homeAssistant.discoveryConfig(f));assert(d["device"]["sw_version"].asString()=="2.3.4-beta");
   p._started=true;assert(!p.setFirmwareVersion("3.0"));assert(String(p.getFirmwareVersion())=="2.3.4-beta");
  }

  {ArdPortal p;auto f=ArdJSON::JSON.parse(R"({"id":"room","type":"climate","name":"Room","min":10,"max":35,"step":0.5,"modes":["off","heat","cool","heat_cool"]})");
   auto value=ArdDynamicPages::initial(f);assert(value["temperature_low"].asDouble()==10&&value["temperature_high"].asDouble()==35);
   value["mode"]="heat_cool";value["temperature_low"]=20;value["temperature_high"]=25;assert(ArdDynamicPages::validValue(f,value));
   auto discovery=ArdJSON::JSON.parse(p._homeAssistant.discoveryConfig(f));assert(discovery["temperature_low_command_topic"].asString().indexOf("room_temperature_low")>=0);assert(discovery["temperature_high_state_template"].asString()=="{{ value_json.temperature_high }}");
   value["temperature_low"]=26;assert(!ArdDynamicPages::validValue(f,value));value["temperature_low"]=20.25;assert(!ArdDynamicPages::validValue(f,value));value.remove("temperature_low");assert(!ArdDynamicPages::validValue(f,value));value["mode"]="cool";assert(ArdDynamicPages::validValue(f,value));
  }

  {ArdPortal::PortalConfig bandConfig;bandConfig.deviceName="Band test";bandConfig.apBand=5;
   String encoded=ArdPortalJson::encode(bandConfig);ArdPortal::PortalConfig restored;
   assert(encoded.length()&&ArdPortalJson::decode(encoded,restored)&&restored.apBand==5);
   auto legacy=ArdPortalJson::value(bandConfig);legacy.remove("apBand");assert(ArdPortalJson::decodeValue(legacy,restored)&&restored.apBand==24);
   auto invalid=ArdPortalJson::value(bandConfig);invalid["apBand"]=6;assert(!ArdPortalJson::decodeValue(invalid,restored));}

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
{
  ArdPortal p;
  auto plain=ArdJSON::JSON.parse(R"({"id":"reading","name":"Reading","type":"text"})");
  assert(ArdDynamicPages::initial(plain).asString()=="");
  auto config=ArdJSON::JSON.parse(p._homeAssistant.discoveryConfig(plain));
  assert(!config.hasOwnProperty("device_class")&&!config.hasOwnProperty("state_class"));
  for(auto type:{"number","temperature","humidity","pressure","illuminance","flow","carbon_dioxide","pm25","wind_speed","voltage","current","power"}){
    auto field=plain;field["textType"]=type;
    assert(ArdDynamicPages::initial(field).isNull());
    assert(ArdDynamicPages::validValue(field,nullptr)&&ArdDynamicPages::validValue(field,22.5)&&!ArdDynamicPages::validValue(field,"22.5"));
    config=ArdJSON::JSON.parse(p._homeAssistant.discoveryConfig(field));
    auto pattern=ArdText::pattern(field);
    assert(config["state_class"].asString()=="measurement");
    assert(config["device_class"].asString()==pattern.device&&config["unit_of_measurement"].asString()==pattern.unit);
    field["ha"]["value_template"]="{{ value | float * 2 }}";
    config=ArdJSON::JSON.parse(p._homeAssistant.discoveryConfig(field));
    assert(config["value_template"].asString()=="{{ value | float * 2 }}");
  }
  plain["textType"]="bogus";
  assert(!p._dynamic.validateField(plain,ArdJSON::JSONVar::array(),false));
  assert(String(p._dynamic.error.stage)=="field"&&p._dynamic.error.fieldId=="reading"&&p._dynamic.error.reason=="unsupported textType");

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
