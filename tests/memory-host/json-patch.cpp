#include <cassert>
#define ARDPORTAL_TEST_ALLOCATE testAllocate
#include "ArdJSON.h"
long allocationBudget=-1;size_t allocations=0;
void* testAllocate(size_t bytes) noexcept {if(allocationBudget==0)return nullptr;if(allocationBudget>0)--allocationBudget;++allocations;return std::malloc(bytes);}
uint32_t fakeMillis=0;
int main(){using namespace ArdJSON;JSONVar base=JSONVar::object(),patch=JSONVar::object();for(int i=0;i<300;++i)base[String("field")+String(i)]=i;patch["field3"]=99;patch["field4"]=JSONVar();patch["new"]=true;Limits l;l.maxNodes=4096;size_t n=JSON.measureObjectPatch(base,patch,nullptr,l);assert(n>0);String text;for(size_t p=0;p<n;p+=31)text+=JSON.stringifyObjectPatchSlice(base,patch,p,31,nullptr,l);auto merged=JSON.parse(text,nullptr,l);assert(merged.isValid()&&merged.length()==300&&merged["field3"].asDouble()==99&&!merged.hasOwnProperty("field4"));
allocationBudget=0;assert(base.applyObjectPatch(std::move(patch)));allocationBudget=-1;assert(JSON.stringify(base,false,nullptr,l)==text);
// A staged deletion must survive later edits before commit.
JSONVar pending=JSONVar::object(),del=JSONVar::object();del["field3"]=JSONVar();assert(pending.applyObjectPatch(std::move(del),false));assert(pending.length()==1&&pending.isObjectPatchValid());JSONVar add=JSONVar::object();add["field5"]=42;assert(pending.applyObjectPatch(std::move(add),false));assert(base.applyObjectPatch(std::move(pending)));assert(!base.hasOwnProperty("field3")&&base["field5"].asDouble()==42);
// Capacity overflow is rejected by both the writer and atomic commit.
JSONVar full=JSONVar::object();for(unsigned i=0;i<MaxContainerElements;++i)full[String(i)]=0;JSONVar overflow=JSONVar::object();overflow["extra"]=true;l.maxNodes=4096;assert(!JSON.measureObjectPatch(full,overflow,nullptr,l));assert(!full.applyObjectPatch(std::move(overflow)));assert(full.length()==MaxContainerElements&&overflow.length()==1);
// An allocation-failed patch cannot corrupt its destination.
JSONVar invalid=JSONVar::object();allocationBudget=0;invalid["new"]=7;assert(!full.applyObjectPatch(std::move(invalid)));allocationBudget=-1;assert(full.isValid());
}
