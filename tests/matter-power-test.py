"""Exercise the real Matter sleep adapter without hardware."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
source = (root / 'src/MatterNativeImpl.h').read_text()
source = source[source.index('inline bool ArdMatter::powerSaving('):source.index('inline String ArdMatter::manualPairingCode()')]
fixture = r'''
#include "ArdPortalFeatures.h"
#include <cassert>
#include <cstdint>
uint32_t fakeMillis=100;
#define now fakeMillis
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
namespace chip {
namespace DeviceLayer {
struct Platform {bool available=true;int locks=0;bool TryLockChipStack(){if(!available)return false;++locks;return true;}void UnlockChipStack(){--locks;}};
Platform& PlatformMgr(){static Platform p;return p;}
}
struct Server {
struct Manager {int wakes=0;void OnNetworkActivity(){++wakes;}void StayActiveRequest(uint32_t ms){assert(ms==1000);}} manager;
static Server& GetInstance(){static Server s;return s;}Manager& GetICDManager(){return manager;}
};
}
class ArdMatter {public:
struct Entry {Entry* next=nullptr;bool dirty=false;};
bool _running=false,online=true;int _mux=0;unsigned _queued=0;Entry* _entries=nullptr;uint32_t _powerActiveAt=0;
bool connected(){return online;}bool powerSaving(bool,bool);
};
int main(){
 ArdMatter m;assert(!m.powerSaving(true,false));m._running=true;
#if CONFIG_ENABLE_ICD_SERVER
 auto& sdk=chip::Server::GetInstance().manager;
 assert(m.powerSaving(true,false));assert(!sdk.wakes);
 m.powerSaving(false,false);assert(sdk.wakes==1);
 now+=100;m.powerSaving(false,false);assert(sdk.wakes==1);
 now+=500;m.powerSaving(true,true);assert(sdk.wakes==2);
 ArdMatter::Entry e;m._entries=&e;e.dirty=true;now+=500;
 assert(!m.powerSaving(true,false));assert(sdk.wakes==3);
 e.dirty=false;
#if ARDPORTAL_MATTER_COMMAND_SUPPORT
 m._queued=1;now+=500;assert(!m.powerSaving(true,false));assert(sdk.wakes==4);
#else
 now+=500;assert(m.powerSaving(true,false));assert(sdk.wakes==3);
 sdk.wakes=4; // Keep the following connectivity assertions common.
#endif
 m._queued=0;m.online=false;now+=500;assert(!m.powerSaving(true,false));assert(sdk.wakes==5);
 now+=500;chip::DeviceLayer::PlatformMgr().available=false;m.powerSaving(false,false);assert(sdk.wakes==5);
 assert(!chip::DeviceLayer::PlatformMgr().locks);
#elif !ARDPORTAL_MATTER_THREAD
 assert(m.powerSaving(true,false));assert(!m.powerSaving(false,false));m.online=false;assert(!m.powerSaving(true,false));
#else
 assert(!m.powerSaving(true,false));
#endif
}
'''
# The method must precede main but follow its declaration.
fixture = fixture.replace('int main(){', source + '\nint main(){')
with tempfile.TemporaryDirectory(prefix='ardportal-matter-power-') as directory:
    cpp=Path(directory)/'test.cpp';cpp.write_text(fixture)
    for thread,icd,controls in ((0,0,1),(1,0,1),(1,1,1),(1,1,0)):
        binary=Path(directory)/'test'
        subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                        '-I'+str(root/'tests/host'),'-I'+str(root/'src'),f'-DARDPORTAL_ENABLE_CONTROLS={controls}',f'-DARDPORTAL_MATTER_THREAD={thread}',f'-DCONFIG_ENABLE_ICD_SERVER={icd}',str(cpp),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
print('PASS: Matter Wi-Fi sleep readiness and Thread ICD active windows, pending reports/commands, lock contention')

# Exercise the actual CMake argument validation and stale-SDK guards.
import os
cmake = (root/'idf/MatterPortal/CMakeLists.txt').read_text()
choice = cmake[cmake.index('set(ARDPORTAL_POWER '):cmake.index('get_filename_component(ARDPORTAL_ROOT')]
guards = cmake[cmake.index('# SDK defaults do not override'):]
with tempfile.TemporaryDirectory(prefix='ardportal-power-cmake-') as directory:
    script=Path(directory)/'power.cmake'
    for profile,power,valid in (('wifi','off',True),('wifi','connected',True),('thread','icd',True),
                               ('thread-wifi','icd',True),('wifi','icd',False),('thread','connected',False),('wifi','invalid',False)):
        script.write_text(f'set(ARDPORTAL_PROFILE {profile})\n'+choice)
        result=subprocess.run([os.environ.get('CMAKE','cmake'),f'-DARDPORTAL_POWER={power}','-P',str(script)],capture_output=True,text=True)
        assert (result.returncode==0)==valid,result.stderr
    for power,pm,tickless,icd,mtd,radio,valid in (
        ('off',False,False,False,False,False,True),
        ('connected',True,True,False,False,False,True),
        ('connected',False,True,False,False,False,False),
        ('connected',True,False,False,False,False,False),
        ('off',True,True,True,True,True,False),
        ('icd',True,True,True,True,True,True),
        ('icd',True,True,False,True,True,False),
        ('icd',True,True,True,False,True,False),
        ('icd',True,True,True,True,False,False)):
        values={'PM_ENABLE':pm,'FREERTOS_USE_TICKLESS_IDLE':tickless,'ENABLE_ICD_SERVER':icd,
                'OPENTHREAD_MTD':mtd,'IEEE802154_SLEEP_ENABLE':radio}
        script.write_text(f'set(ARDPORTAL_POWER {power})\n'+''.join(f'set(CONFIG_{key} {"TRUE" if value else "FALSE"})\n' for key,value in values.items())+guards)
        result=subprocess.run([os.environ.get('CMAKE','cmake'),'-P',str(script)],capture_output=True,text=True)
        assert (result.returncode==0)==valid,result.stderr
print('PASS: actual CMake rejects incompatible transports and stale PM/ICD SDK configurations')
