"""Exercise the actual HTTP accumulation block with short writes and retries."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/ArdPortalImpl.h').read_text()
a = source.index('        // Reuse the existing transport String:')
b = source.index('\n      }\n      _responseOffset = 0;', a)
block = source[a:b]
dynamic = (root / 'src/DynamicPortalImpl.h').read_text()
skip_start = dynamic.index('  // Commit bounded cursor-only progress')
skip_end = dynamic.index('  if (_httpDynamicField < _dynamic.pages.fields(p)) {', skip_start)
skip_block = dynamic[skip_start:skip_end]
fixture = r'''
#include <cassert>
#include <vector>
#include <Arduino.h>
uint32_t fakeMillis=0;
struct Portal {
 enum class DynamicHttpPart {Ready, RetryLater, Failed, Finished};
 String _response="header", _httpDynamicText;
 size_t _httpDynamicPageOffset=0, _responseOffset=6, next=0;
 bool _httpDynamicPages=true, closed=false, pressure=false, retry=false;
 std::vector<String> fragments;
 bool dynamicHttpMemoryReady() {
   if (_httpDynamicPageOffset < _httpDynamicText.length()) return true;
   _httpDynamicPageOffset=0; _httpDynamicText=String(); return !pressure;
 }
 DynamicHttpPart prepareDynamicHttpPart() {
   if(retry){retry=false;return DynamicHttpPart::RetryLater;}
   assert(next<fragments.size());
   _httpDynamicText=fragments[next++];
   if(next==fragments.size())_httpDynamicPages=false;
   return DynamicHttpPart::Ready;
 }
 void closeHttp(){closed=true;}
 size_t _httpDynamicField=0;
 struct Registry { struct Pages {
   size_t fields(size_t)const{return 40;}
   bool dependent(size_t,size_t f)const{return f==35;}
 } pages; } _dynamic;
 bool skip(){const size_t p=0;
SKIP_BLOCK
 return false;}
 void fill(){
'''
main = r'''
 _responseOffset=0;
 }
};
int main(){
 Portal skips;
 assert(skips.skip() && skips._httpDynamicField==16 && !skips._httpDynamicText.length());
 assert(skips.skip() && skips._httpDynamicField==32);
 assert(!skips.skip() && skips._httpDynamicField==35);
 skips._httpDynamicField=36;assert(!skips.skip() && skips._httpDynamicField==40);
 Portal p; String expected;
 for(unsigned i=0;i<387;++i){String f=i==386?String("}"):String("abcdefghijklmnopqrstuvwxyz");p.fragments.push_back(f);expected+=f;}
 String received;unsigned fills=0;
 while(p._httpDynamicPages || p._responseOffset<p._response.length()){
   if(p._responseOffset<p._response.length()){
     size_t n=p._response.length()-p._responseOffset;if(n>7)n=7;
     received+=p._response.substring(p._responseOffset,p._responseOffset+n);p._responseOffset+=n;
   }else{
     if(fills==1){p.pressure=true;p.fill();assert(p._responseOffset==p._response.length());p.pressure=false;}
     if(fills==2){p.retry=true;p.fill();}
     p.fill();++fills;assert(p._response.length()<=256);
   }
   assert(!p.closed);
 }
 assert(received==expected);assert(fills<60);assert(p.next==387);
 Portal large;large.fragments={String(std::string(700,'x')),String("END")};
 String output;
 while(large._httpDynamicPages || large._responseOffset<large._response.length()){
   if(large._responseOffset==large._response.length())large.fill();
   output+=large._response.substring(large._responseOffset);large._responseOffset=large._response.length();
 }
 assert(output==String(std::string(700,'x'))+"END");
 puts("PASS: HTTP coalescing, bounded buffers, final fragment, large fragments, short writes and memory retries.");
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-http-coalescing-') as tmp:
    tmp = Path(tmp)
    (tmp / 'test.cpp').write_text(fixture.replace('SKIP_BLOCK', skip_block) + block + main)
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', '-I'+str(root/'tests/host'), str(tmp/'test.cpp'), '-o', str(tmp/'test')], check=True)
    subprocess.run([str(tmp/'test')], check=True)
