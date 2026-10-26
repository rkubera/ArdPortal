#!/usr/bin/env python3
# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
"""Exercise the actual bounded queue, including wraparound and supersession."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
source = (root / 'src/MatterNativeImpl.h').read_text()
a = source.index('inline bool ArdMatter::enqueue(')
b = source.index('inline void ArdMatter::dirty(', a)
fixture = r'''
#include <cassert>
#include "MatterFields.h"
uint32_t fakeMillis=0;
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
struct ArdMatter {
 using Operation=ArdMatterFields::Operation;
 using Command=ArdMatterFields::Command;
 struct Entry {uint16_t field;};
 bool _accepting=false;
 int _mux=0;
#if ARDPORTAL_MATTER_COMMAND_SUPPORT
 Command _commands[8]{};
 uint8_t _head=0,_queued=0;
#endif
 bool enqueue(Entry&,Operation,uint32_t);
 bool pop(Command&);
 void discard(uint16_t);
};
int main();
'''
main = r'''
int main(){
 ArdMatter q;
 ArdMatter::Entry a{1},b{2};
 ArdMatter::Command out{};
 auto op=ArdMatter::Operation::Level;
#if ARDPORTAL_MATTER_COMMAND_SUPPORT
 assert(q.enqueue(a,op,999) && !q.pop(out)); // initialization is never a user command
 q._accepting=true;
 for(unsigned round=0;round<1000;++round){
  for(unsigned i=0;i<8;++i)assert(q.enqueue(i%2?a:b,op,i));
  assert(!q.enqueue(a,op,99));
  q.discard(1);
  for(unsigned i=0;i<4;++i){assert(q.pop(out));assert(out.field==2 && out.value==i*2);}
  assert(!q.pop(out));
  for(unsigned i=0;i<5;++i)assert(q.enqueue(a,op,i));
  for(unsigned i=0;i<3;++i){assert(q.pop(out));assert(out.value==i);}
  for(unsigned i=5;i<11;++i)assert(q.enqueue(a,op,i));
  assert(!q.enqueue(a,op,99));
  for(unsigned i=3;i<11;++i){assert(q.pop(out));assert(out.value==i);}
  assert(!q.pop(out));
 }
#else
 assert(!q.enqueue(a,op,999) && !q.pop(out));
 q.discard(b.field);
#endif
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-matter-queue-') as directory:
    path=Path(directory)
    (path/'test.cpp').write_text(fixture+source[a:b]+main)
    for flags in ([], ['-DARDPORTAL_ENABLE_CONTROLS=0', '-DARDPORTAL_ENABLE_CONTROL_TEXT=1']):
        subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',*flags,'-I'+str(root/'tests/host'),'-I'+str(root/'src'),str(path/'test.cpp'),'-o',str(path/'test')],check=True)
        subprocess.run([str(path/'test')],check=True)
print('PASS: actual Matter queue, startup suppression, saturation, FIFO wraparound and field supersession; sensor-only builds omit queue storage')
