// Test the actual asynchronous exporter without Wi-Fi, WWW or the native SDK.
#include <Arduino.h>
#include <cassert>
#include <functional>
#include <memory>
#include <fstream>
#define ARDPORTAL_ZIGBEE_CONVERTER_SUPPORT 1
#define ARDPORTAL_ZHA_QUIRK_SUPPORT 1
#include "ArdJSON.h"
#include "JsonCodec.h"
#include "ZigbeeConverterAssets.h"
#include "ZhaQuirkAssets.h"
uint32_t fakeMillis=0;
namespace ArdZigbeeIdentity {char Model[]="\x1a" "ArdPortal-0123456789abcdef";}
struct ArdZigbee {
 struct Control {unsigned index=0;std::unique_ptr<Control> next;};Control first;
 ArdZigbee(){first.next.reset(new Control);first.next->index=1;}
 bool nativeReady()const{return true;}const Control* controls()const{return &first;}
};
class ArdPortal {public:
 using ZigbeeExportWriter=std::function<bool(const char*,size_t)>;using ZigbeeExportFinished=std::function<void(bool,const String&)>;
 bool _configurationReady=true,_rebootPending=false,_converterLayoutReady=true,_zigbeeBeginTried=true,_zigbeeMappingFailed=false,_appConfigPageRegistration=false;
 unsigned _zigbeeSeenFields=2,_zigbeeBuildIndex=2;struct {unsigned count()const{return 2;}}_dynamic;ArdZigbee _zigbee;ArdJsonCodec _json;
 ZigbeeExportWriter _apiExportWriter;ZigbeeExportFinished _apiExportFinished;const ArdZigbee::Control* _apiExportControl=nullptr;
 String _apiExportText;size_t _apiExportOffset=0;uint32_t _apiExportProgressAt=0;uint8_t _apiExportStage=0;bool _apiExportPython=false,_apiExportFirst=true,_apiExportCancel=false,oom=false;
 bool otaActive()const{return false;}
 String zigbeeConverterControlJson(const ArdZigbee::Control& c){if(oom)return String();return c.index?R"({"schema":1,"kind":"switch","ep":3,"endpoint":"second_ep3","label":"Second"})":R"({"schema":1,"kind":"switch","ep":2,"endpoint":"first_ep2","label":"First"})";}
 bool zigbeeConverterExportReady()const;bool zigbeeConverterExportBusy()const{return _apiExportStage!=0;}
 bool beginZigbeeConverterExport(bool,ZigbeeExportWriter,ZigbeeExportFinished);void finishZigbeeConverterExport(bool,const String&);void serviceZigbeeConverterExport();
 bool startZigbee2MqttConverterExport(ZigbeeExportWriter,ZigbeeExportFinished=nullptr);bool startZhaQuirkExport(ZigbeeExportWriter,ZigbeeExportFinished=nullptr);
};
#include "ZigbeeExportApiImpl.h"
int main(int argc,char** argv){
 ArdPortal p;assert(!p.startZigbee2MqttConverterExport(nullptr));p._converterLayoutReady=false;assert(!p.startZhaQuirkExport([](const char*,size_t){return true;}));p._converterLayoutReady=true;
 for(bool python:{false,true}){
  std::string output;unsigned completions=0,calls=0;bool refuse=true;
  auto writer=[&](const char* data,size_t bytes){assert(bytes&&bytes<=256);++calls;if(refuse){refuse=false;return false;}refuse=true;output.append(data,bytes);return true;};
  auto finished=[&](bool ok,const String& error){assert(ok&&!error.length());++completions;};
  assert(python?p.startZhaQuirkExport(writer,finished):p.startZigbee2MqttConverterExport(writer,finished));
  assert(!p.startZhaQuirkExport(writer,finished));
  p.oom=true;for(unsigned i=0;i<20;++i)p.serviceZigbeeConverterExport();p.oom=false;
  for(unsigned i=0;i<1000&&p.zigbeeConverterExportBusy();++i)p.serviceZigbeeConverterExport();
  assert(!p.zigbeeConverterExportBusy()&&completions==1&&calls>5);
  assert(output.find("first_ep2")!=std::string::npos&&output.find("second_ep3")!=std::string::npos);
  if(argc>1){std::ofstream file(std::string(argv[1])+(python?".py":".mjs"));file<<output;}
 }
 unsigned failures=0;auto finished=[&](bool ok,const String&){assert(!ok);++failures;};
 assert(p.startZhaQuirkExport([](const char*,size_t){return false;},finished));fakeMillis=30000;p.serviceZigbeeConverterExport();assert(failures==1&&!p.zigbeeConverterExportBusy());
 assert(p.startZhaQuirkExport([](const char*,size_t){return true;},finished));p._apiExportCancel=true;p.serviceZigbeeConverterExport();assert(failures==2);
 assert(p.startZhaQuirkExport([](const char*,size_t){return true;},finished));p._zigbeeSeenFields=1;p.serviceZigbeeConverterExport();assert(failures==3);
}
