#!/usr/bin/env python3
"""Check Thread role configuration, stale SDK rejection and startup capability checks."""
import os
import subprocess
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
cmake = (root / 'idf/MatterPortal/CMakeLists.txt').read_text()
choice = cmake[cmake.index('# Thread role selects'):cmake.index('get_filename_component(ARDPORTAL_ROOT')]
guards = cmake[cmake.index('# Thread role guards:'):cmake.index('# SDK defaults do not override')]
native = (root / 'src/MatterNativeImpl.h').read_text()
start = native.index('#if ARDPORTAL_MATTER_THREAD_ROUTER\n', native.index('  Matter.begin();'))
end = native.index('  // OpenThread initialization alone', start)
startup = native[start:end]
assert native.index('  if (!SDK::ready())') < start
assert 'otThreadBecomeRouter' not in native, 'Never force a network role'
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    for profile, power, role, valid in [('wifi','off','mtd',True),('thread','off','router',True),
        ('thread-wifi','off','router',True),('wifi','off','router',False),('thread','icd','router',False),
        ('thread-wifi','icd','router',False),('thread','off','invalid',False)]:
        script = path / 'role.cmake'
        script.write_text(f'set(ARDPORTAL_PROFILE {profile})\nset(ARDPORTAL_POWER {power})\n'+choice)
        result = subprocess.run([os.environ.get('CMAKE','cmake'),f'-DARDPORTAL_THREAD_ROLE={role}','-P',str(script)],capture_output=True,text=True)
        assert (result.returncode == 0) == valid, result.stderr
    for role,ftd,mtd,icd,sleep,valid in [('router',1,0,0,0,True),('router',0,1,0,0,False),
        ('router',1,0,1,0,False),('router',1,0,0,1,False),('mtd',0,1,0,0,True),('mtd',1,0,0,0,False)]:
        script.write_text(f'set(ARDPORTAL_PROFILE thread-wifi)\nset(ARDPORTAL_THREAD_ROLE {role})\n'+''.join(
            f'set(CONFIG_{key} {value})\n' for key,value in dict(OPENTHREAD_FTD=ftd,OPENTHREAD_MTD=mtd,ENABLE_ICD_SERVER=icd,IEEE802154_SLEEP_ENABLE=sleep).items())+guards)
        result = subprocess.run([os.environ.get('CMAKE','cmake'),'-P',str(script)],capture_output=True,text=True)
        assert (result.returncode == 0) == valid, result.stderr
    cpp = path / 'startup.cpp'
    cpp.write_text(r'''
#include <cassert>
#include <string>
namespace chip { namespace DeviceLayer {
struct ConnectivityManager { enum {kThreadDeviceType_Router=1,kThreadDeviceType_FullEndDevice=2}; };
struct Manager {int type=1;int GetThreadDeviceType(){return type;}};
Manager& ConnectivityMgr(){static Manager manager;return manager;}
struct Platform {int locks=0;void LockChipStack(){++locks;}void UnlockChipStack(){--locks;}};
Platform& PlatformMgr(){static Platform manager;return manager;}
}}
struct Adapter {
 std::string error;void fail(const char* message){error=message;}
 bool start(){
'''+startup+r'''
 return true;}
};
int main(){
 for(int type:{0,1,2}){
  chip::DeviceLayer::ConnectivityMgr().type=type;
  Adapter adapter;const bool expected=!ARDPORTAL_MATTER_THREAD_ROUTER||type==1||type==2;
  assert(adapter.start()==expected);assert(adapter.error.empty()==expected);
  assert(!chip::DeviceLayer::PlatformMgr().locks);
 }
}
''')
    for router in (0,1):
        subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',f'-DARDPORTAL_MATTER_THREAD_ROUTER={router}',str(cpp),'-o',str(path/'test')],check=True)
        subprocess.run([str(path/'test')],check=True)
    # Precompiled SDKs cannot gain FTD support through a library define.
    (path/'soc').mkdir();(path/'soc/soc_caps.h').write_text('#pragma once\n')
    for thread,ftd,flag,icd,valid in [(1,1,None,0,True),(1,0,None,0,True),(1,0,1,0,False),
        (1,1,0,0,False),(1,1,1,1,False),(0,0,1,0,False),(1,1,2,0,False)]:
        flags='#define ESP32 1\n#define CONFIG_IDF_TARGET_ESP32C6 1\n#define ARDPORTAL_ENABLE_MATTER 1\n'
        flags+=f'#define ARDPORTAL_MATTER_THREAD {thread}\n#define CONFIG_ENABLE_ICD_SERVER {icd}\n'
        if ftd:flags+='#define CONFIG_OPENTHREAD_FTD 1\n'
        if flag is not None:flags+=f'#define ARDPORTAL_MATTER_THREAD_ROUTER {flag}\n'
        cpp.write_text(flags+'#include "ArdPortalFeatures.h"\nint main(){}\n')
        result=subprocess.run(['c++','-std=c++17','-I'+str(path),'-I'+str(root/'tests/host'),'-I'+str(root/'src'),str(cpp),'-o',str(path/'test')],capture_output=True,text=True)
        assert (result.returncode==0)==valid,result.stderr
print('PASS: Thread MTD/router profiles, ICD/radio-sleep rejection, SDK feature guards and unlocked startup validation')
