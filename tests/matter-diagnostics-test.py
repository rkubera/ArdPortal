"""Exercise bounded Matter diagnostics and the real SDK event routing."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'src/MatterNativeImpl.h').read_text()
a=source.index('inline void ArdMatter::diagnosticEvent(')
b=source.index('\n#endif',a)
method=source[a:b]
events=['CommissioningWindowOpened','CommissioningSessionStarted','CommissioningSessionStopped','CommissioningComplete','FailSafeTimerExpired','CommissioningWindowClosed','FabricRemoved','SecureSessionEstablished','BLEDeinitialized','FactoryReset']
portal=(root/'src/MatterPortalImpl.h').read_text()
a=portal.index('void ArdPortal::serviceMatterDiagnostics()')
b=portal.index('\n#endif',a)
service=portal[a:b]
fixture='''
#include "MatterDiagnostics.h"
#include "LanguageLiterals.h"
#include <Arduino.h>
#include <vector>
#include <cassert>
#define portENTER_CRITICAL(x) (++locks)
#define portEXIT_CRITICAL(x) (--locks)
int locks=0;
namespace chip {namespace DeviceLayer {
namespace DeviceEventType {enum {'''+','.join('k'+e for e in events)+''',kUnrelated};}
struct ChipDeviceEvent {int Type;};
}}
struct ArdMatter {
 enum class State:uint8_t {Waiting,Staging,Commissioning,Commissioned,Connected,Failed,Resetting,NoEndpoints};
 State current=State::Waiting;State state(){return current;}const char* error(){return "test failure";}
 int _mux=0;ArdMatterDiagnostics _diagnostics;
static void diagnosticEvent(const chip::DeviceLayer::ChipDeviceEvent*,intptr_t);};
'''+method+'''
struct ArdPortal {ArdMatter _matter;std::vector<String> messages;void log(const String& text){messages.push_back(text);}void serviceMatterDiagnostics();};
'''+service+'''
int main(){
 using D=ArdMatterDiagnostics;using E=D::Event;
 static_assert(sizeof(D)<=12,"Diagnostic storage must remain bounded and compact");
 D queue;E out;assert(!queue.pop(out));
 for(unsigned i=0;i<8;++i)queue.push(E(i));
 queue.push(E::FactoryReset);assert(queue.takeOverflow());assert(!queue.takeOverflow());
 for(unsigned i=0;i<4;++i){assert(queue.pop(out));assert(out==E(i));}
 for(unsigned i=0;i<4;++i)queue.push(E(i));
 for(unsigned i=4;i<8;++i){assert(queue.pop(out));assert(out==E(i));}
 for(unsigned i=0;i<4;++i){assert(queue.pop(out));assert(out==E(i));}
 assert(!queue.pop(out));
 ArdMatter owner;
 for(int i=0;i<10;++i){
  chip::DeviceLayer::ChipDeviceEvent event{i};
  ArdMatter::diagnosticEvent(&event,reinterpret_cast<intptr_t>(&owner));
  assert(!locks&&owner._diagnostics.pop(out)&&out==E(i));
 }
 chip::DeviceLayer::ChipDeviceEvent ignored{chip::DeviceLayer::DeviceEventType::kUnrelated};
 ArdMatter::diagnosticEvent(&ignored,reinterpret_cast<intptr_t>(&owner));
 assert(!owner._diagnostics.pop(out));
 ArdPortal portal;portal.serviceMatterDiagnostics();assert(portal.messages.size()==1);
 portal.serviceMatterDiagnostics();assert(portal.messages.size()==1); // No periodic state spam.
 portal._matter.current=ArdMatter::State::Connected;portal.serviceMatterDiagnostics();assert(portal.messages.size()==2);
 portal._matter._diagnostics.push(E::WindowOpened);portal._matter._diagnostics.push(E::SessionStarted);
 portal.serviceMatterDiagnostics();assert(portal.messages.size()==3);
 portal.serviceMatterDiagnostics();assert(portal.messages.size()==4);
 portal._matter.current=ArdMatter::State::Failed;portal.serviceMatterDiagnostics();assert(portal.messages.size()==5);
 assert(portal.messages.back().indexOf("test failure")>=0);portal.serviceMatterDiagnostics();assert(portal.messages.size()==5);
 portal._matter.current=ArdMatter::State::NoEndpoints;portal.serviceMatterDiagnostics();assert(portal.messages.size()==6);
 assert(portal.messages.back().indexOf("No Matter controls")>=0);portal.serviceMatterDiagnostics();assert(portal.messages.size()==6);
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-matter-diagnostics-') as directory:
 cpp=Path(directory)/'test.cpp';cpp.write_text(fixture);binary=Path(directory)/'test'
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'src'),'-I'+str(root/'tests/host'),str(cpp),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
# No logging/String allocation from the SDK callback and no replacement of user callbacks.
assert 'log(' not in method and 'String' not in method
assert 'Matter.onEvent(' not in source.replace('Matter.onEvent().','')
portal=(root/'src/MatterPortalImpl.h').read_text()
start=portal.index('void ArdPortal::serviceMatterDiagnostics()')
end=portal.index('\n#endif',start)
assert 'manualPairingCode' not in portal[start:end] and 'onboardingUrl' not in portal[start:end]
print('PASS: compact bounded FIFO, overflow recovery, wraparound, SDK event routing and callback/credential isolation')
