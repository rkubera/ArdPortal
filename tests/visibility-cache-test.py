"""Check real visibility traversal reuse, invalidation and error isolation."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
s = (root/'src/ArdAppControlsImpl.h').read_text()
a = s.index('bool ArdAppControls::dependencyValueMatches(')
b = s.index('\n#else', a)
methods = s[a:b]
fixture = r'''
#include <cassert>
#include "ArdAllocation.h"
#include "DependencyConditions.h"
#include "JsonCodec.h"
uint32_t fakeMillis=0;
using V=ArdJSON::JSONVar;
ArdJSON::Limits snapshotLimits(){return ArdJSON::Limits();}
struct Registry {
 unsigned loads=0;V fields[4];
 size_t count()const{return 4;}
 size_t indexOf(const String& id)const{for(size_t i=0;i<4;++i)if(fields[i]["id"].asString()==id)return i;return 4;}
 bool dependentAt(size_t i)const{return fields[i].hasOwnProperty("visibleWhen");}
 V conditionsAt(size_t i){++loads;return fields[i];}
};
struct ArdPortal {
 Registry _dynamic;uint32_t _appRevision=1;ArdJsonCodec _json;
 V value=true;
 V getAppConfigValue(const char*)const{return value;}
};
struct ArdAppControls {
 struct VisibilityCache {ArdAppConfigFieldMask complete,visible;uint32_t revision=0;};
 ArdPortal& _portal;
 bool dependencyValueMatches(const V&)const;
 bool dependencyFieldVisible(size_t,VisibilityCache* = nullptr)const;
 bool dependencyMatches(const V&,VisibilityCache* = nullptr)const;
 bool appFieldVisible(const V&,VisibilityCache* = nullptr)const;
};
'''
main = r'''
int main(){
 ArdPortal p;ArdAppControls c{p};ArdAppControls::VisibilityCache cache;
 p._dynamic.fields[0]=p._json.parse(R"({"id":"root"})");
 p._dynamic.fields[1]=p._json.parse(R"({"id":"mode","visibleWhen":{"field":"root","equals":true}})");
 p._dynamic.fields[2]=p._json.parse(R"({"id":"a","visibleWhen":{"field":"mode","equals":true}})");
 p._dynamic.fields[3]=p._json.parse(R"({"id":"b","visibleWhen":{"field":"mode","equals":true}})");
 assert(c.appFieldVisible(p._dynamic.fields[2],&cache));unsigned loaded=p._dynamic.loads;assert(loaded>0);
 assert(c.appFieldVisible(p._dynamic.fields[3],&cache));assert(p._dynamic.loads==loaded);
 assert(c.appFieldVisible(p._dynamic.fields[3]));assert(p._dynamic.loads>loaded);
 p.value=false;++p._appRevision;
 assert(!c.dependencyFieldVisible(1,&cache));assert(cache.revision==p._appRevision);
 assert(!c.appFieldVisible(p._dynamic.fields[2],&cache));
 p.value=true;++p._appRevision;assert(c.appFieldVisible(p._dynamic.fields[2],&cache));
 p._dynamic.fields[1]["visibleWhen"]["field"]="mode";++p._appRevision;
 assert(!c.dependencyFieldVisible(1,&cache));assert(!cache.complete.test(1));
 // A failed traversal must not publish intermediate results.
 p._dynamic.fields[1]["visibleWhen"]["field"]="missing";++p._appRevision;
 assert(!c.dependencyFieldVisible(1,&cache));assert(!cache.complete.test(1));
 p._dynamic.fields[1]["visibleWhen"]["field"]="root";++p._appRevision;
 assert(c.dependencyFieldVisible(1,&cache));
 puts("PASS: visibility cache reuse, uncached fallback, revision invalidation, hidden ancestors, cycles and invalid references.");
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-visibility-cache-') as tmp:
    tmp=Path(tmp);(tmp/'test.cpp').write_text(fixture+methods+main)
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'tests/host'),'-I'+str(root/'src'),str(tmp/'test.cpp'),'-o',str(tmp/'test')],check=True)
    subprocess.run([str(tmp/'test')],check=True)
