from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'src/ZigbeePortalImpl.h').read_text();a=source.index('bool ArdPortal::acceptZigbeeState(');b=source.index('\n/**',a);function=source[a:b]
extra=''
for name in ['bool ArdPortal::setZigbeeOtaProgress(', 'void ArdPortal::markZigbeeStateDirty(']:
    first=source.index(name);last=source.index('\n}',first)+2;extra+='\n'+source[first:last]
fixture=r'''
#include <cassert>
#define ARDPORTAL_TEST_ALLOCATE testAllocate
#include "ArdJSON.h"
#include "AppConfigFields.h"
long budget=-1;
void* testAllocate(size_t bytes) noexcept {if(budget==0)return nullptr;if(budget>0)--budget;return std::malloc(bytes);}
uint32_t fakeMillis=0;
using V=ArdJSON::JSONVar;
enum class ChangeSource{Zigbee};
namespace ArdZigbeeFields {enum class Kind {Switch,Update};}
namespace ArdHeap {bool allowed=true;bool permits(size_t,size_t){return allowed;}}
struct Native {struct Control {struct {size_t field=0;ArdZigbeeFields::Kind kind=ArdZigbeeFields::Kind::Update;}spec;std::unique_ptr<Control> next;};Control first;Native(){first.next.reset(new Control);first.next->spec.field=1;first.next->spec.kind=ArdZigbeeFields::Kind::Switch;}const Control* controls(){return &first;}ArdAppConfigFieldMask queued;void queueReports(size_t field,bool=false){for(const Control* c=controls();c;c=c->next.get())if(c->spec.field==field)queued|=ArdAppConfigFieldMask::forField(field);}};
struct Registry {V definition=V::object();const V& field(const String&)const{return definition;}V at(size_t)const{return definition;}size_t fieldWork(size_t){return 0;}size_t fieldBlock(size_t){return 0;}size_t indexOf(const String& key){return key=="firmware"?0:key=="relay"?1:3;}};
struct Controls {bool accepted=true;unsigned changes=0;V last;bool applyAppState(const String&,const V& value,ChangeSource){if(accepted){++changes;last=value;}return accepted;}};
class ArdPortal {public:
 size_t _zigbeeReportField=size_t(-1);uint8_t _zigbeeReportEndpoint=0;Registry _dynamic;Controls _appControls;V _zigbeeAppQueue=V::object();bool _rebootPending=false,ota=false,trial=false;
 bool otaActive(){return ota;}bool mqttTrialActive(){return trial;}
 Native _zigbee;ArdAppConfigFieldMask _zigbeeDirty;
 V getAppConfigValue(const char*){return ArdJSON::JSON.parse(R"({"installed_version":"1.0.0","latest_version":"1.0.0","in_progress":false})");}
 bool setZigbeeOtaProgress(bool);void markZigbeeStateDirty(const String&);
 bool acceptZigbeeState(const String&,const V&);
};
'''
main=r'''
int main(){
 ArdPortal p;p._dynamic.definition["persist"]=false;assert(p.acceptZigbeeState("relay",true)&&!p._zigbeeAppQueue.length()&&p._appControls.changes==1);
 p._dynamic.definition["persist"]=true;assert(p.acceptZigbeeState("relay",true));assert(p._zigbeeAppQueue["relay"].asBool());
 assert(p.acceptZigbeeState("relay",false)&&p._zigbeeAppQueue.length()==1&&!p._zigbeeAppQueue["relay"].asBool());
 assert(p.acceptZigbeeState("other",true)&&p._zigbeeAppQueue.length()==2);
 p._appControls.accepted=false;assert(!p.acceptZigbeeState("relay",true)&&!p._zigbeeAppQueue["relay"].asBool());p._appControls.accepted=true;
 unsigned count=p._appControls.changes;budget=0;assert(!p.acceptZigbeeState("relay",true));budget=-1;assert(p._zigbeeAppQueue.isObjectPatchValid()&&!p._zigbeeAppQueue["relay"].asBool()&&p._appControls.changes==count);
 for(unsigned guard=0;guard<3;++guard){p._rebootPending=guard==0;p.ota=guard==1;p.trial=guard==2;assert(!p.acceptZigbeeState("relay",true));}
 assert(p._appControls.changes==count);
 p._dynamic.definition["id"]="firmware";assert(p.setZigbeeOtaProgress(true)&&p._appControls.last["in_progress"].asBool());assert(p._zigbeeAppQueue.length()==2);
 ArdHeap::allowed=false;assert(!p.setZigbeeOtaProgress(false));ArdHeap::allowed=true;assert(p.setZigbeeOtaProgress(false)&&!p._appControls.last["in_progress"].asBool());
 p.markZigbeeStateDirty("relay");assert(p._zigbee.queued.test(1)&&!p._zigbee.queued.test(0));auto dirty=p._zigbee.queued;p.markZigbeeStateDirty("clock");assert(p._zigbee.queued==dirty);
 puts("PASS: actual Zigbee persistence path; RAM-only fields, coalescing, rejected commands, OOM recovery OTA/restart guards, RAM-only progress and selective endpoint invalidation");
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-zigbee-persist-') as tmp:
    tmp=Path(tmp);(tmp/'test.cpp').write_text(fixture+function+extra+main)
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'tests/host'),'-I'+str(root/'src'),str(tmp/'test.cpp'),str(root/'src/JsonStream.cpp'), str(root/'src/JsonDocumentStream.cpp'), '-o',str(tmp/'test')],check=True)
    subprocess.run([str(tmp/'test')],check=True)
