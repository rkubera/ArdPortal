from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/ZigbeePortalImpl.h').read_text();start=s.index('void ArdPortal::serviceZigbeeControls(');end=s.index('\n}\n#endif',start)+2
service=s[start:end]
start=s.index('void ArdPortal::markZigbeeStateDirty(');end=s.index('\n}',start)+2
mark=s[start:end]
fixture=r'''
#include <cassert>
#include <vector>
#include <deque>
#include <memory>
#include "ZigbeeFields.h"
#include "AppConfigFields.h"
uint32_t fakeMillis=0;
using V=ArdJSON::JSONVar;
using namespace ArdZigbeeFields;
enum class ChangeSource {Zigbee};
namespace ArdHeap {bool allowed=true;bool permits(size_t,size_t){return allowed;}}
namespace ArdHa {bool normalize(V& f,bool){if(f["type"].asString()=="fan"&&!f.hasOwnProperty("controls"))f["controls"]=ArdJSON::JSON.parse(R"([{"key":"state","type":"switch","on":"ON","off":"OFF"},{"key":"percentage","type":"slider","min":0,"max":100,"step":1},{"key":"oscillation","type":"switch","on":"oscillate_on","off":"oscillate_off"},{"key":"direction","type":"select","options":["forward","reverse"]}])");return true;}bool readonly(const V& f){return f["readonly"].asBool();}bool extended(const V&){return true;}bool validValue(const V&,const V&){return true;}}
struct ArdDynamicPages {std::vector<V> fields;size_t count(){return fields.size();}size_t fieldWork(size_t){return 0;}size_t fieldBlock(size_t){return 0;}V at(size_t i){return fields[i];}size_t indexOf(const String& id){for(size_t i=0;i<fields.size();++i)if(fields[i]["id"].asString()==id)return i;return fields.size();}static bool validValue(const V&,const V&){return true;}};
struct Native {
 enum class State {Connected};
 struct Control {Spec spec;uint16_t pendingReports=0;bool reportQueued=false,reportForce=false,reportRetry=false,reportIncomplete=false;uint32_t reportRetryAt=0;std::unique_ptr<Control> next;};std::unique_ptr<Control> head;
 std::vector<uint8_t> sent;std::deque<Command> incoming;bool fail=false;unsigned count=0;
  void queueReports(size_t field,bool force=false){for(auto* entry=head.get();entry;entry=entry->next.get())if(field==size_t(-1)||entry->spec.field==field){entry->reportQueued=true;if(force){entry->reportForce=true;entry->reportRetry=false;entry->pendingReports=0;}}}
  /** @brief Select the next ready endpoint fairly. @param now Current time. @param after Previous endpoint cursor. @return Borrowed queued endpoint or nullptr. */
  const Control* nextReport(uint32_t now,uint8_t after)const{const Control* wrap=nullptr;for(auto* entry=head.get();entry;entry=entry->next.get())if(entry->reportQueued&&(!entry->reportRetry||int32_t(now-entry->reportRetryAt)>=0)){if(entry->spec.endpoint>after)return entry;if(!wrap)wrap=entry;}return wrap;}
  /** @brief Complete one report attempt without blocking other endpoints. @param endpoint Attempted endpoint. @param success All attributes queued successfully. @param now Current time. @return No value. */
  void finishReport(uint8_t endpoint,bool success,uint32_t now){for(auto* entry=head.get();entry;entry=entry->next.get())if(entry->spec.endpoint==endpoint){if(success){entry->reportQueued=false;entry->reportForce=false;entry->reportRetry=false;}else if(entry->reportIncomplete){entry->reportRetry=false;}else{entry->reportRetry=true;entry->reportRetryAt=now+5000;}return;}}

 const Control* controls(){return head.get();}State state(){return State::Connected;}
 bool addControl(Spec s,const Values&,const String&){s.endpoint=++count+1;auto entry=std::make_unique<Control>();entry->spec=s;auto* at=&head;while(*at)at=&(*at)->next;*at=std::move(entry);return true;}
 bool publish(uint8_t endpoint,const Values&,bool){if(fail)return false;sent.push_back(endpoint);return true;}
 bool nextCommand(Command& out){if(incoming.empty())return false;out=incoming.front();incoming.pop_front();return true;}
 void diagnostic(const String&){}String error(){return String();}unsigned controlCount(){return count;}bool begin(int){return true;}
};
using ZigbeeState=Native::State;
class ArdPortal;
struct Controls {ArdPortal* portal;bool appFieldVisible(const V&){return false;}bool appControl(const V&,size_t,const V&,ChangeSource);};
class ArdPortal {public:
 ArdDynamicPages _dynamic;Native _zigbee;Controls _appControls{this};V values=V::object(),_zigbeeAppQueue=V::object();
 bool _configurationReady=true,_rebootPending=false,_httpWaitingStorage=false,_zigbeeDrainingRestart=false,_zigbeeRetryPending=false,_zigbeeBeginTried=false,_zigbeeMappingFailed=false,_zigbeeWasConnected=false,_zigbeeHasCommand=false;
 void* _appConfigPageRegistration=nullptr;size_t _zigbeeBuildIndex=0,_zigbeeSeenFields=0,_zigbeeReportField=size_t(-1);uint16_t _zigbeeBuildPart=0;uint8_t _zigbeeReportEndpoint=0;uint32_t _zigbeeRegistrationSince=0,_zigbeeRetryAt=0,_zigbeeReportReadyAt=0;int _zigbeeOptions=0;
 ArdAppConfigFieldMask _zigbeeDirty,_zigbeeForce;Command _zigbeePendingCommand;unsigned actions=0;
 bool otaActive(){return false;}bool portalAndAppConfigBusy(){return false;}bool scheduleAppPatch(const V&,ChangeSource){return true;}
 V getAppConfigValue(const char* key){return values[key];}bool acceptZigbeeState(const String& id,const V& value){values[id]=value;return true;}
 void markZigbeeStateDirty(const String&);void serviceZigbeeControls(uint32_t);
};
bool Controls::appControl(const V& f,size_t part,const V& value,ChangeSource){const V& c=f["controls"][part];String key=c["key"].asString();if(c["type"].asString()=="action"){++portal->actions;return true;}portal->values[f["id"].asString()][key]=value;portal->markZigbeeStateDirty(f["id"].asString());return true;}
V json(const char* text){return ArdJSON::JSON.parse(text);}
'''
main=r'''
int main(){
 ArdPortal p;p._dynamic.fields.push_back(json(R"({"id":"fan","type":"fan","name":"Fan","controls":[{"key":"state","type":"switch","on":"ON","off":"OFF"},{"key":"percentage","type":"slider","min":0,"max":100,"step":1},{"key":"direction","type":"select","options":["forward","reverse"]},{"key":"","type":"action","payload":"START"}]})"));
 p.values["fan"]=json(R"({"state":"OFF","percentage":10,"direction":"forward"})");p._zigbeeSeenFields=1;
 for(unsigned i=0;i<10;++i){unsigned previous=p._zigbee.count;p.serviceZigbeeControls(2000+i*100);assert(p._zigbee.count-previous<=1);}
 assert(p._zigbee.count==4&&p._zigbeeBeginTried);for(unsigned i=0;i<8;++i)p.serviceZigbeeControls(3000+i*100);assert(!p._zigbee.nextReport(2010,0));p._zigbee.sent.clear();p._zigbeeReportEndpoint=0;p._zigbeeReportReadyAt=0;
 p.markZigbeeStateDirty("fan");p.serviceZigbeeControls(5000);assert(p._zigbee.sent.size()==1&&p._zigbee.sent.back()==2);
 p.values["fan"]["percentage"]=20;p.markZigbeeStateDirty("fan");p.serviceZigbeeControls(5100);assert(p._zigbee.sent.back()==3);
 p._zigbee.fail=true;size_t sent=p._zigbee.sent.size();p.serviceZigbeeControls(5200);assert(p._zigbee.sent.size()==sent);
 p._zigbee.fail=false;p.serviceZigbeeControls(5300);assert(p._zigbee.sent.size()==sent+1&&p._zigbee.sent.back()==5);
 p.serviceZigbeeControls(5400);assert(p._zigbee.sent.back()==2);
 sent=p._zigbee.sent.size();p.serviceZigbeeControls(10199);assert(p._zigbee.sent.size()==sent);
 p.serviceZigbeeControls(10200);assert(p._zigbee.sent.back()==4&&!p._zigbee.nextReport(8002,0));
 Command c;c.endpoint=2;c.cluster=6;c.value=1;p._zigbee.incoming.push_back(c);p.serviceZigbeeControls(11000);assert(p.values["fan"]["state"].asString()=="ON");
 c.endpoint=5;p._zigbee.incoming.push_back(c);p.serviceZigbeeControls(11100);p._zigbee.incoming.push_back(c);p.serviceZigbeeControls(11200);assert(p.actions==2);

 ArdPortal raw;raw._dynamic.fields.push_back(json(R"({"id":"raw","type":"fan","name":"Raw fan"})"));raw.values["raw"]=json(R"({"state":"OFF","percentage":0,"oscillation":"oscillate_off","direction":"forward"})");raw._zigbeeSeenFields=1;
 for(unsigned i=0;i<16;++i)raw.serviceZigbeeControls(2000+i*100);
 Command incoming;incoming.endpoint=2;incoming.cluster=6;incoming.value=1;raw._zigbee.incoming.push_back(incoming);raw.serviceZigbeeControls(4000);assert(raw.values["raw"]["state"].asString()=="ON");
 puts("PASS: actual composite bridge; bounded staging, every child reported, mid-pass invalidation, report backoff, projected settings and repeated actions");
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-composite-test-') as work:
 work=Path(work);(work/'test.cpp').write_text(fixture+service+'\n'+mark+'\n'+main)
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'tests/host'),'-I'+str(root/'src'),str(work/'test.cpp'),'-o',str(work/'test')],check=True)
 subprocess.run([str(work/'test')],check=True)
