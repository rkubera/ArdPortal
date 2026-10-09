from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1];source=(root/'src/DynamicPortalImpl.h').read_text();a=source.index('bool ArdPortal::dynamicHttpMemoryReady()');b=source.index('ArdPortal::DynamicHttpPart',a);func=source[source.index('bool ArdPortal::reserveDynamicHttpMemory('):b]
fixture=r'''
#include <cassert>
#include <Arduino.h>
uint32_t fakeMillis=0;
uint32_t fakeFree(),fakeBlock();unsigned heapReads=0;uint32_t blockPenalty=0;
struct HeapStub{uint32_t getFreeHeap(){++heapReads;return fakeFree();}uint32_t getMaxFreeBlockSize(){return fakeBlock();}}testHeap;
#define ESP testHeap
#include "ArdJSON.h"
#include "HeapMemory.h"
#define ARDPORTAL_ENABLE_CONSOLE 1
#define ARDPORTAL_ENABLE_CONSOLE_MESSAGES 1
struct Pages{size_t work=4000;size_t length(){return 9;}size_t fields(size_t){return 1;}size_t fieldIndex(size_t,size_t){return 0;}size_t fieldWork(size_t){return work;}size_t fieldHttpWork(size_t){return work;}size_t fieldBlock(size_t){return 2000;}};
struct ArdPortal{
 unsigned _httpDynamicMode=1;bool _httpPortalTail=false;size_t _httpDynamicPageOffset=0,_httpDynamicPageIndex=0,_httpDynamicField=0;String _httpDynamicText;
 struct {Pages pages;} _dynamic;
 struct ConsoleLine{uint32_t id=0;String text;};static constexpr size_t ConsoleCapacity=16;ConsoleLine _console[16],_consoleMessages[16];
 bool reserveDynamicHttpMemory(size_t,size_t);bool dynamicHttpMemoryReady();
}portal;
uint32_t pressure=0,credit=0;
uint32_t historyBytes(){uint32_t n=0;for(auto& x:portal._console)n+=x.text.length();for(auto& x:portal._consoleMessages)n+=x.text.length();return n;}
uint32_t fakeFree(){return uint32_t(portal._dynamic.pages.work+ARDPORTAL_NETWORK_HEAP_RESERVE-portal._httpDynamicText.length()-pressure-historyBytes()+credit);}
uint32_t fakeBlock(){return uint32_t(portal._dynamic.pages.work+ARDPORTAL_NETWORK_BLOCK_RESERVE-portal._httpDynamicText.length()-pressure-blockPenalty);}
'''
main=r'''
int main(){
 assert(ArdHeap::fieldWork(600,0,48)==ArdHeap::fieldWork(600));
 assert(ArdHeap::fieldWork(600,40,48)==4144);
 assert(ArdHeap::fieldWork(SIZE_MAX,40,48)==SIZE_MAX);
 assert(ArdHeap::fieldWork(600,SIZE_MAX,48)==SIZE_MAX);
 assert(ArdJSON::JSONVar::memberAllocationBytes()>=sizeof(ArdJSON::JSONVar));
 // A complete response fragment itself consumes the exact headroom needed for its successor.
 portal._httpDynamicText=String(std::string(1000,'x'));portal._httpDynamicPageOffset=1000;
 assert(!ArdHeap::permits(4000));assert(portal.dynamicHttpMemoryReady());
 assert(!portal._httpDynamicText.length()&&portal._httpDynamicPageOffset==0);
 // An allocated but partly-unsent fragment must drain even when reserving a second full field would fail.
 portal._httpDynamicText=String(std::string(1000,'x'));portal._httpDynamicPageOffset=256;heapReads=0;
 assert(portal.dynamicHttpMemoryReady()&&heapReads==0&&portal._httpDynamicText.length()==1000&&portal._httpDynamicPageOffset==256);
 // Real pressure still pauses new work after reclaiming the old fragment; recovery resumes without losing offsets.
 portal._httpDynamicPageOffset=1000;pressure=1;assert(!portal.dynamicHttpMemoryReady());assert(!portal._httpDynamicText.length()&&portal._httpDynamicPageOffset==0);
 pressure=0;assert(portal.dynamicHttpMemoryReady());
 blockPenalty=1400;assert(!ArdHeap::permits(4000));assert(ArdHeap::permits(4000,2000));blockPenalty=0;
 portal._httpPortalTail=true;pressure=1000;assert(portal.dynamicHttpMemoryReady());
 // Under pressure, old traffic yields memory while recent messages stay available.
 pressure=0;credit=600;blockPenalty=0;portal._httpDynamicText=String();
 for(unsigned i=0;i<8;++i){portal._console[i].id=i+1;portal._console[i].text=std::string(200,'m').c_str();}
 assert(portal.reserveDynamicHttpMemory(portal._dynamic.pages.work,2000));assert(historyBytes()<=600);assert(portal._console[7].text.length()==200);
 for(auto& x:portal._console)x=ArdPortal::ConsoleLine();credit=600;
 for(unsigned i=0;i<8;++i){portal._consoleMessages[i].id=i+1;portal._consoleMessages[i].text=std::string(200,'d').c_str();}
 assert(portal.reserveDynamicHttpMemory(portal._dynamic.pages.work,2000));assert(historyBytes()==600);assert(portal._consoleMessages[5].text.length()&&portal._consoleMessages[6].text.length()&&portal._consoleMessages[7].text.length());
 pressure=1;assert(!portal.reserveDynamicHttpMemory(portal._dynamic.pages.work,2000));assert(historyBytes()==600);
 puts("PASS: actual HTTP memory gate; consumed fragments reclaimed before admission, pending chunks drain, pressure/recovery preserves network reserves; aggregate versus contiguous allocation, structural estimates/fallback/overflow checked.");
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-http-memory-') as tmp:
 tmp=Path(tmp);(tmp/'test.cpp').write_text(fixture+func+main)
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'tests/host'),'-I'+str(root/'src'),str(tmp/'test.cpp'),'-o',str(tmp/'test')],check=True);subprocess.run([str(tmp/'test')],check=True)
