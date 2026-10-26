#!/usr/bin/env python3
"""Exercise the real empty-inventory startup guard without starting the SDK."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
header=(root/'src/ArdMatter.h').read_text()
enum=header[header.index('  enum class State'):header.index('  struct Options')]
frozen=header[header.index('  bool frozen()'):header.index('  bool commissioned()')]
native=(root/'src/MatterNativeImpl.h').read_text()
start=native[native.index('  if (!_count)',native.index('inline bool ArdMatter::start(')):native.index('  nvs_handle_t handle;',native.index('inline bool ArdMatter::start('))]
service=(root/'src/MatterPortalImpl.h').read_text()
gate=next(line for line in service.splitlines() if 'State::NoEndpoints &&' in line)
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)
 (p/'test.cpp').write_text('''#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstddef>
struct ArdMatter {
'''+enum+frozen+'''
 State _state=State::Waiting;const char* _error="old";unsigned _count=0,_seen=0;
 bool start(){'''+start+'''return true;}
};
struct Portal {
 enum class AppConfigPageRegistrationState {Succeeded,Failed};
 AppConfigPageRegistrationState _appConfigPageRegistrationState=AppConfigPageRegistrationState::Succeeded;
 ArdMatter _matter;bool _appConfigPageRegistration=false;unsigned calls=0;
 struct Dynamic {unsigned size=0;unsigned count(){return size;}} _dynamic;
 void service(){'''+gate+''' ++calls;}
};
int main(){
 ArdMatter m;assert(!m.start());assert(m._state==ArdMatter::State::NoEndpoints);
 assert(!*m._error&&!m.frozen());m._count=1;assert(m.start());
 m._state=ArdMatter::State::Staging;assert(m.frozen());
 m._state=ArdMatter::State::Failed;assert(m.frozen());
 Portal p;p._matter._state=ArdMatter::State::NoEndpoints;
 for(int i=0;i<100;++i){p.service();}
 assert(!p.calls);
 p._dynamic.size=1;p.service();assert(p.calls==1);
 p._dynamic.size=0;p._appConfigPageRegistration=true;p.service();assert(p.calls==2);
 p._appConfigPageRegistration=false;p._appConfigPageRegistrationState=Portal::AppConfigPageRegistrationState::Failed;
 p.service();assert(p.calls==3);
}
''')
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: empty Matter inventory is informational, skips SDK/retries and permits later registration')
