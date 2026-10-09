#include "Arduino.h"
#include "ESP8266WiFi.h"
#include "ArdFSHostVolume.h"
#include <cassert>
#define ARDPORTAL_ENABLE_DYNAMIC_PAGES 0
#define ARDPORTAL_ENABLE_OTA 0
#define ARDPORTAL_ENABLE_MQTT 0
#define ARDPORTAL_ENABLE_HA 1
#define ARDPORTAL_ENABLE_DEPENDENCIES 1
#define ARDPORTAL_ENABLE_CONTROL_SLIDER 1
#define private public
#include "ArdPortalDeclarations.h"
#undef private
#include "ArdPortal.h"
static_assert(!ARDPORTAL_ENABLE_HA && !ARDPORTAL_ENABLE_DEPENDENCIES && !ARDPORTAL_ENABLE_CONTROL_SLIDER,"Dynamic page dependencies must be forced off");
uint32_t fakeMillis=0;int ntpStarts=0;String lastNtpServer1,lastNtpServer2;time_t fakeUnixTime=1800000000;
SerialStub Serial;WiFiStub WiFi;EspStub ESP;
bool failMount=false,failFormat=false,failFileWrite=false;
std::map<std::string,std::vector<uint8_t>> fakeFiles;
std::deque<WiFiClient> WiFiServer::incoming;
bool WiFiClient::connectResult=true;
int main() {
  ArdPortal portal;
  portal._request="GET / HTTP/1.1\r\nHost: device\r\n\r\n";
  portal.handleHttp();
  assert(portal._response.indexOf("Content-Encoding: gzip")>=0);
  assert(portal._assetChunks&&portal._pageLength>2);
  assert(static_cast<unsigned char>(portal._page[0])==0x1f&&static_cast<unsigned char>(portal._page[1])==0x8b);
  portal.closeHttp();
  assert(!portal.startAppConfigPageRegistration(String("{}")));
  assert(!portal.setAppConfigStateValue("a",1));
  assert(!portal.emitAppConfigEvent("a",1));
  int changes=0;
  portal.onAppConfigValueChanged([&](const String& key,const ArdJSON::JSONVar& value,ArdPortal::ChangeSource) {
    assert(key=="a"&&value.asDouble()==42);++changes;
  });
  auto app=ArdJSON::JSONVar::object();app["a"]=42;
  portal.applyAppConfig(app);
  assert(portal.getAppConfigValue("a").asDouble()==42&&changes==1);
  portal.applyAppConfig(app);assert(changes==1);
  portal.onAppConfigValueChanged({});
  // Generic application settings still persist through ArdFS without any form.
  assert(portal.begin());
  for(int i=0;i<1000;++i){fakeMillis+=10;portal.loop();}
  assert(portal.portalAndAppConfigReady());
  assert(portal.setAppConfigValue("saved",17));
  for(int i=0;i<1000;++i){fakeMillis+=10;portal.loop();}
  assert(portal.getAppConfigValue("saved").asDouble()==17);
  ArdPortal restored;assert(restored.begin());
  for(int i=0;i<1000;++i){fakeMillis+=10;restored.loop();}
  assert(restored.portalAndAppConfigReady());
  assert(restored.getAppConfigValue("saved").asDouble()==17);
}
