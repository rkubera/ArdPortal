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


void drain(ArdPortal& p){if(p._savePending&&!p._saveQueued)p.flushPortalAndAppConfig();for(int i=0;i<10000&&(p.portalAndAppConfigBusy()||!p.portalAndAppConfigReady());++i){fakeMillis+=10;p.serviceStorage(fakeMillis);}assert(!p._storage.busy()&&!p._savePending);}
int main(){
 ArdPortal p;assert(p.begin());drain(p);
 assert(p.startAppConfigPageRegistration(String(R"({"id":"mqtt","name":"MQTT","order":0,"fields":[{"id":"pump","name":"Pump","type":"switch","mqtt":{"state_topic":"custom/$device/pump"},"ha":{"state_topic":"legacy/pump"}}]})")));
 for(int i=0;i<1000&&p.appConfigPageRegistrationBusy();++i)p.serviceAppConfigPageRegistration();
 if(p._dynamic.count()!=1){auto e=p.appConfigRegistrationError();fprintf(stderr,"%s: %s\n",e.stage,e.reason.c_str());}
 assert(p._dynamic.count()==1);
 const auto& field=p._dynamic.at(0);
 String topic=field["mqtt"]["state_topic"].asString();
 assert(topic.indexOf("custom/")==0&&topic.indexOf("$device")<0);
 auto& mqtt=p._mqttClient;WiFi.state=WL_CONNECTED;p._config.host="broker";
 mqtt._mqttState=ArdPortal::MqttState::Connected;assert(mqtt._mqtt.connect(IPAddress(),1883));
 p._appControls.markDirty("pump");assert(p._appControls.publishState());
 // Decode the actual retained PUBLISH packet queued for the socket.
 const String& packet=mqtt._tx;assert(uint8_t(packet[0])==0x31);
 size_t pos=1;while(uint8_t(packet[pos++])&128){}
 size_t length=(size_t(uint8_t(packet[pos]))<<8)|uint8_t(packet[pos+1]);pos+=2;
 assert(length==topic.length()&&packet.substring(pos,pos+length)==topic);
 assert(packet.substring(pos+length)=="OFF");
 assert(p._appControls._stateDirty.firstSet()>=p._dynamic.count());
#if ARDPORTAL_ENABLE_HA
 auto discovery=ArdJSON::JSON.parse(p._homeAssistant.discoveryConfig(field));
 assert(discovery["state_topic"].asString()==topic);
#endif
 puts("PASS: registered MQTT field, device expansion, retained state wire topic and discovery agreement");
}
