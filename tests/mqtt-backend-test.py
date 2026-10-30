"""Check compile-time exclusion, feature signatures and retained header observer."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
flags=['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'tests/mqtt-host'),'-I'+str(root/'tests/memory-host'),'-I'+str(root/'src')]
source=r'''
#include "ArdPortalMqttBackend.h"
#include <cassert>
#include <deque>
#include <type_traits>
uint32_t fakeMillis=0;
#if ARDPORTAL_ENABLE_MQTT
#if ARDPORTAL_USE_PUBSUBCLIENT
static_assert(std::is_same<ArdPortalMqttBackend,ArdPubSubClient>::value, "backend");
static_assert((ARDPORTAL_FEATURE_MASK & (uint64_t(1)<<54))!=0, "signature");
struct Socket : Client {
 std::deque<uint8_t> rx;
 uint8_t connected() override{return true;} int available() override{return rx.size();}
 int read() override{if(rx.empty())return -1;auto b=rx.front();rx.pop_front();return b;}
 size_t write(const uint8_t*,size_t n) override{return n;} void stop() override{}
};
#else
static_assert(std::is_same<ArdPortalMqttBackend,ArdMQTTClient<ArdPortalMqttIO>>::value, "backend");
static_assert((ARDPORTAL_FEATURE_MASK & (uint64_t(1)<<54))==0, "signature");
#endif
#endif
int main(){
#if ARDPORTAL_ENABLE_MQTT && ARDPORTAL_USE_PUBSUBCLIENT
 Socket socket;ArdPubSubTransport transport;transport.client=&socket;
 socket.rx={0x31,0x82,1}; // fragmented retained PUBLISH, remaining length 130
 assert(transport.read()==0x31&&transport.retained&&transport.receiveBusy());
 assert(transport.read()==0x82&&transport.receiveBusy());
 assert(transport.read()==1&&transport.receiveBusy());
 for(unsigned i=0;i<130;++i)socket.rx.push_back(0x30);
 uint8_t bytes[130];assert(transport.read(bytes,130)==130&&!transport.receiveBusy()&&transport.retained);
 socket.rx={0x30,0};assert(transport.read()==0x30&&!transport.retained);assert(transport.read()==0&&!transport.receiveBusy());
 uint8_t ping[]={0xc0,0};assert(transport.write(ping,2)==2&&transport.receiveBusy());
 socket.rx={0xd0,0};assert(transport.read()==0xd0&&transport.receiveBusy());assert(transport.read()==0&&!transport.receiveBusy());
 transport.reset();assert(!transport.receiveBusy()&&!transport.retained);
#endif
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.cpp';p.write_text(source)
 for backend,mqtt in [(0,1),(1,1),(0,0),(1,0)]:
  options=[f'-DARDPORTAL_USE_PUBSUBCLIENT={backend}',f'-DARDPORTAL_ENABLE_MQTT={mqtt}']
  deps=subprocess.check_output(flags+options+['-MM',str(p)],text=True)
  assert ('src/ArdMQTT.h' in deps)==bool(mqtt and not backend),deps
  assert ('mqtt-host/PubSubClient.h' in deps)==bool(mqtt and backend),deps
  exe=Path(d)/f'test-{backend}-{mqtt}'
  subprocess.run(flags+options+[str(p),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
 for console in (0, 1):
  p.write_text('#include "ArdPortalMqttBackend.h"\n'
               'static_assert(!ARDPORTAL_ENABLE_MQTT && !ARDPORTAL_ENABLE_MQTT_TLS && !ARDPORTAL_ENABLE_HA, "MQTT exclusion");\n'
               'static_assert(ARDPORTAL_ENABLE_LOG_CALLBACK && ARDPORTAL_ENABLE_CONSOLE_MESSAGES, "headless logging");\n'
               f'static_assert(ARDPORTAL_ENABLE_CONSOLE == {console}, "independent console");\n')
  options=['-DARDPORTAL_ENABLE_LOG_CALLBACK=1', '-DARDPORTAL_ENABLE_MQTT=0',
           '-DARDPORTAL_ENABLE_DYNAMIC_PAGES=0', f'-DARDPORTAL_ENABLE_CONSOLE={console}']
  deps=subprocess.check_output(flags+options+['-MM',str(p)],text=True)
  assert 'src/ArdMQTT.h' not in deps and 'mqtt-host/PubSubClient.h' not in deps
  subprocess.run(flags+options+['-fsyntax-only',str(p)],check=True)
 p.write_text('#include "ArdPortalFeatures.h"\n')
 assert subprocess.run(flags+['-DARDPORTAL_USE_PUBSUBCLIENT=2','-fsyntax-only',str(p)],capture_output=True).returncode
print('PASS: mutually exclusive MQTT backends, disabled MQTT, feature signature, invalid define, retained/fragmented headers and ping sleep blocker')
