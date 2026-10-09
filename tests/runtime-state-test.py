from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1];source=(root/'src/ArdAppControlsImpl.h').read_text();a=source.index('bool ArdAppControls::applyAppState(');b=source.index('\n/**',a);func=source[a:b]
fixture=r'''
#include <cassert>
#define ARDPORTAL_TEST_ALLOCATE testAllocate
#include "ArdJSON.h"
long allocationBudget=-1;
void* testAllocate(size_t bytes) noexcept {if(allocationBudget==0)return nullptr;if(allocationBudget>0)--allocationBudget;return std::malloc(bytes);}
uint32_t fakeMillis=0;
using V=ArdJSON::JSONVar;
enum class ChangeSource{Application};
namespace ArdHa{bool transient(const V& f){return f["transient"].asBool();}}
struct ArdDynamicPages{static bool validValue(const V&,const V& v){return v.isValid();}};
struct Dynamic{V f=V::object();bool missing=false;const V& field(const String&){static V empty;return missing?empty:f;}};
struct Portal{
 Dynamic _dynamic;V defaults=V::object();V* state=nullptr;unsigned _appRevision=0,callbacks=0;
 V getAppConfigValue(const char* key){return state&&state->hasOwnProperty(key)?static_cast<const V&>(*state)[key]:static_cast<const V&>(defaults)[key];}
 std::function<void(const String&,const V&,ChangeSource)> _appChanged;
};
struct ArdAppControls{
 Portal& _portal;V _appState=V::object();unsigned dirty=0;size_t outputLimit=32768;
 explicit ArdAppControls(Portal& p):_portal(p){p.state=&_appState;}
 ArdJSON::Limits snapshotLimits(){ArdJSON::Limits l;l.maxNodes=1024;l.maxOutputBytes=outputLimit;return l;}
 void markDirty(const String&){++dirty;}
 bool applyAppState(const String&,const V&,ChangeSource,bool);
};
'''
main=r'''
int main(){
 Portal p;ArdAppControls c(p);p._appChanged=[&](const String&,const V&,ChangeSource){++p.callbacks;};
 for(unsigned i=0;i<500;++i)p.defaults[String("field")+String(i)]=30;
 for(unsigned round=0;round<3;++round)for(unsigned i=0;i<500;++i)assert(c.applyAppState(String("field")+String(i),V(30),ChangeSource::Application,true));
 assert(c._appState.length()==0&&p._appRevision==0&&p.callbacks==0&&c.dirty==0);
 allocationBudget=0;assert(c.applyAppState("field1",V(30),ChangeSource::Application,true));allocationBudget=-1;
 assert(c.applyAppState("field1",V(31),ChangeSource::Application,true));assert(c._appState.length()==1&&p._appRevision==1&&p.callbacks==1&&c.dirty==1);
 assert(c.applyAppState("field1",V(31),ChangeSource::Application,true));assert(p._appRevision==1&&p.callbacks==1&&c.dirty==1);
 p.defaults["name"]="Pump";assert(c.applyAppState("name",V("pump"),ChangeSource::Application,false));assert(p._appRevision==2&&p.callbacks==2&&c.dirty==1);
 p.defaults["enabled"]=false;assert(c.applyAppState("enabled",V(false),ChangeSource::Application,true));assert(c._appState.length()==2);
 assert(c.applyAppState("enabled",V(true),ChangeSource::Application,false));assert(c._appState.length()==3&&p._appRevision==3&&c.dirty==1);
 assert(c.applyAppState("new-reading",V("first"),ChangeSource::Application,false));assert(c._appState.length()==4&&p._appRevision==4);
 c.outputLimit=1;assert(!c.applyAppState("field1",V(32),ChangeSource::Application,true));assert(c._appState["field1"].asDouble()==31&&p._appRevision==4);c.outputLimit=32768;
 p._dynamic.f["transient"]=true;assert(!c.applyAppState("enabled",V(false),ChangeSource::Application,true));p._dynamic.f.remove("transient");p._dynamic.missing=true;assert(!c.applyAppState("enabled",V(false),ChangeSource::Application,true));
 puts("PASS: actual applyAppState; 1500 unchanged snapshots allocate no runtime members; no-op works with OOM allocator; real changes, types, case, first readings, serialization failure, callbacks and MQTT dirty semantics preserved.");
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-runtime-test-') as tmp:
 tmp=Path(tmp);(tmp/'test.cpp').write_text(fixture+func+main)
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'tests/host'),'-I'+str(root/'src'),str(tmp/'test.cpp'),'-o',str(tmp/'test')],check=True);subprocess.run([str(tmp/'test')],check=True)
