#!/usr/bin/env python3
# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
"""Exercise Thread startup admission without launching the SDK on low DMA heap."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/MatterPortalImpl.h').read_text()
a=s.index('#if ARDPORTAL_MATTER_THREAD\n#if ARDPORTAL_ENABLE_WIFI\n    // Let station')
b=s.index('    return;',s.index('    _matter.start(',a))
fixture=r'''
#include <cassert>
#include <string>
namespace ArdHeap {
bool available=false;
bool permits(unsigned bytes,unsigned block){assert(bytes==(ARDPORTAL_ENABLE_WIFI ? 61440u : 40960u) && block==8192);return available;}
}
struct Owner {
 enum class AppConfigPageRegistrationState {Succeeded,Failed};
 AppConfigPageRegistrationState _appConfigPageRegistrationState=AppConfigPageRegistrationState::Succeeded;
 struct Matter {const char* _error="";int starts=0,count=1;int endpointCount(){return count;}void fail(const char* error){_error=error;}void start(const char*,const char*){++starts;}} _matter;
 struct Config {std::string deviceName="Device",deviceManufacturer="Manufacturer";} _config;
#if ARDPORTAL_ENABLE_WIFI
 bool _attempting=false,_pendingReady=false,_savePending=false;
 struct Storage {bool active=false;bool busy(){return active;}} _storage;
#endif
 void service(){
'''
main=r'''
 }
};
int main(){
 Owner o;
 o.service();assert(!o._matter.starts && std::string(o._matter._error)=="Waiting for Matter startup memory");
 ArdHeap::available=true;
#if ARDPORTAL_ENABLE_WIFI
 for(auto* blocker:{&o._attempting,&o._pendingReady,&o._savePending,&o._storage.active}){
  *blocker=true;o.service();assert(!o._matter.starts);*blocker=false;
 }
#endif
 o.service();assert(o._matter.starts==1 && !*o._matter._error);
 Owner bad;bad._matter.count=0;bad._appConfigPageRegistrationState=Owner::AppConfigPageRegistrationState::Failed;
 bad.service();assert(!bad._matter.starts && std::string(bad._matter._error)=="App page registration failed");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.cpp';p.write_text(fixture+s[a:b]+main)
 for wifi in (0,1):
  subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-DARDPORTAL_MATTER_THREAD=1',f'-DARDPORTAL_ENABLE_WIFI={wifi}',str(p),'-o',str(Path(d)/'test')],check=True)
  subprocess.run([str(Path(d)/'test')],check=True)
print('PASS: Thread startup defers under memory/association/storage pressure, retries and clears the transient status')
