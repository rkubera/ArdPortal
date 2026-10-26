from pathlib import Path
import tempfile,subprocess
root=Path(__file__).resolve().parents[1]
cases=[('#define CONFIG_IDF_TARGET_ESP32H2 1\n#define ARDPORTAL_ENABLE_WIFI 1\n#define ARDPORTAL_ENABLE_WWW 1\n#define ARDPORTAL_ENABLE_MQTT 1\n#define ARDPORTAL_ENABLE_OTA 1\n',0,0,0),('',1,1,1),('#define ARDPORTAL_ENABLE_WWW 0\n#define ARDPORTAL_ENABLE_OTA 1\n',1,0,1),('#define ARDPORTAL_ENABLE_WIFI 0\n#define ARDPORTAL_ENABLE_WWW 1\n#define ARDPORTAL_ENABLE_MQTT 1\n',0,0,0)]
with tempfile.TemporaryDirectory() as d:
 for index,(flags,wifi,www,mqtt) in enumerate(cases):
  p=Path(d)/'test.cpp';p.write_text(flags+'#include "ArdPortalFeatures.h"\n'+f'static_assert(ARDPORTAL_ENABLE_WIFI=={wifi});\nstatic_assert(ARDPORTAL_ENABLE_WWW=={www});\nstatic_assert(ARDPORTAL_ENABLE_MQTT=={mqtt});\n'+('static_assert(!ARDPORTAL_ENABLE_MQTT_TLS&&!ARDPORTAL_ENABLE_HA);\n' if not wifi else '')+('static_assert(!ARDPORTAL_SUPPORT_WEBSOCKET&&!ARDPORTAL_HTTP_OTA_SUPPORT);\nstatic_assert(ARDPORTAL_ENABLE_OTA);\n' if not www else '')+'int main(){}\n')
  subprocess.run(['c++','-std=c++17','-I'+str(root/'tests/host'),'-I'+str(root/'src'),str(p),'-o',str(Path(d)/'test')],check=True)
print('PASS: defaults, independent WWW switch and Wi-Fi override of WWW/MQTT/TLS/HA/WebSockets')

# Power management defaults on, can be disabled, and rejects invalid flags.
with tempfile.TemporaryDirectory() as d:
 for value in (None, 0, 1, 2):
  p=Path(d)/'power.cpp'
  flags='' if value is None else f'#define ARDPORTAL_ENABLE_POWER_MANAGEMENT {value}\n'
  p.write_text(flags+'#include "ArdPortalFeatures.h"\n'+f'static_assert(ARDPORTAL_ENABLE_POWER_MANAGEMENT=={1 if value is None else value});\nint main(){{}}\n')
  result=subprocess.run(['c++','-std=c++17','-I'+str(root/'tests/host'),'-I'+str(root/'src'),str(p),'-o',str(Path(d)/'power')],capture_output=True)
  assert (result.returncode==0)==(value!=2),result.stderr.decode()
print('PASS: power management defaults, overrides and validation')

# Compile the actual startup storage selection: Matter's SDK owns persisted
# credentials/fail-safe rollback, while standalone portal Wi-Fi stays RAM-only.
implementation=(root/'src/ArdPortalImpl.h').read_text()
start=implementation.index('#if ARDPORTAL_ENABLE_MATTER && !ARDPORTAL_MATTER_THREAD',implementation.index('bool ArdPortal::begin('))
# Select the startup block, rather than later connection ownership checks.
end=implementation.index('  WiFi.setAutoReconnect(false);',start)
selection=implementation[start:end]
assert 'WiFi.persistent(true)' in selection
with tempfile.TemporaryDirectory() as d:
 for matter,thread in ((0,0),(0,1),(1,0),(1,1)):
  p=Path(d)/'storage.cpp'
  p.write_text(f'#define ARDPORTAL_ENABLE_MATTER {matter}\n#define ARDPORTAL_MATTER_THREAD {thread}\n'
   +'struct { bool saved=false; void persistent(bool value){saved=value;} } WiFi;\nvoid begin(){\n'+selection+'}\n'
   +f'int main(){{begin();return WiFi.saved!={int(matter and not thread)};}}\n')
  subprocess.run(['c++','-std=c++17',str(p),'-o',str(Path(d)/'storage')],check=True)
  subprocess.run([str(Path(d)/'storage')],check=True)
print('PASS: actual Wi-Fi storage startup selection for Matter Wi-Fi, Thread and standalone profiles')

# Source-built C5 SDK profiles use Kconfig, not Arduino's precompiled board-menu overrides.
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'c5.cpp'
 (Path(d)/'soc').mkdir()
 (Path(d)/'soc/soc_caps.h').write_text('#pragma once\n')
 base='#define ESP32 1\n#define CONFIG_IDF_TARGET_ESP32C5 1\n#define ARDPORTAL_ENABLE_MATTER 1\n#define ARDPORTAL_MATTER_THREAD 1\n'
 sdk='#define ESP_PLATFORM 1\n#define CONFIG_ARDPORTAL_MATTER 1\n'
 for flags,valid in (('',False),('#define ARDUINO_MATTER_NETWORK_THREAD 1\n',True),
                     (sdk,False),(sdk+'#define CONFIG_ENABLE_MATTER_OVER_THREAD 1\n#define CONFIG_ENABLE_WIFI_STATION 1\n',True)):
  p.write_text(base+flags+'#include "ArdPortalFeatures.h"\nint main(){}\n')
  result=subprocess.run(['c++','-std=c++17','-I'+d,'-I'+str(root/'tests/host'),'-I'+str(root/'src'),str(p),'-o',str(Path(d)/'c5')],capture_output=True,text=True)
  assert (result.returncode==0)==valid,result.stderr
print('PASS: C5 source SDK Thread + Wi-Fi accepts Kconfig; Arduino menu and missing SDK transport remain validated')

# Mixed Thread/portal Wi-Fi gets bounded buffers without reinitializing sketch-owned Wi-Fi.
a=implementation.index('#if ARDPORTAL_ENABLE_MATTER && ARDPORTAL_MATTER_THREAD\n  // The first Wi-Fi init')
b=implementation.index('\n#endif',a)+len('\n#endif')
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'buffers.cpp'
 p.write_text('''
#include <cassert>
using wifi_mode_t=int;
constexpr int ESP_OK=0,ESP_ERR_WIFI_NOT_INIT=-1;
struct wifi_init_config_t {int static_rx_buf_num=10,dynamic_rx_buf_num=32,tx_buf_type=0,static_tx_buf_num=16,dynamic_tx_buf_num=32,cache_tx_buf_num=16,rx_ba_win=6;};
#define WIFI_INIT_CONFIG_DEFAULT() wifi_init_config_t{}
bool initialized=false;int calls=0,result=ESP_OK;wifi_init_config_t applied;
int esp_wifi_get_mode(wifi_mode_t*){return initialized?ESP_OK:ESP_ERR_WIFI_NOT_INIT;}
int esp_wifi_init(const wifi_init_config_t* config){++calls;applied=*config;return result;}
bool begin(){
'''+implementation[a:b]+'''
return true;}
int main(){
 assert(begin() && calls==1);
 assert(applied.static_rx_buf_num==4 && applied.dynamic_rx_buf_num==8 && applied.dynamic_tx_buf_num==8);
 assert(applied.tx_buf_type==1 && !applied.static_tx_buf_num && applied.cache_tx_buf_num==4);
 assert(applied.rx_ba_win<=2*applied.static_rx_buf_num && applied.rx_ba_win<=applied.dynamic_rx_buf_num);
 initialized=true;assert(begin() && calls==1);
 initialized=false;result=-2;assert(!begin() && calls==2);
}
''')
 subprocess.run(['c++','-std=c++17','-DARDPORTAL_ENABLE_MATTER=1','-DARDPORTAL_MATTER_THREAD=1',str(p),'-o',str(Path(d)/'buffers')],check=True)
 subprocess.run([str(Path(d)/'buffers')],check=True)
print('PASS: Thread + portal Wi-Fi buffer bounds, existing driver preservation and initialization failure')
