from pathlib import Path
import tempfile,subprocess
root=Path(__file__).resolve().parents[1]
cases=[('#define CONFIG_IDF_TARGET_ESP32H2 1\n#define ARDPORTAL_ENABLE_WIFI 1\n#define ARDPORTAL_ENABLE_WWW 1\n#define ARDPORTAL_ENABLE_MQTT 1\n#define ARDPORTAL_ENABLE_OTA 1\n',0,0,0),('',1,1,1),('#define ARDPORTAL_ENABLE_WWW 0\n#define ARDPORTAL_ENABLE_OTA 1\n',1,0,1),('#define ARDPORTAL_ENABLE_WIFI 0\n#define ARDPORTAL_ENABLE_WWW 1\n#define ARDPORTAL_ENABLE_MQTT 1\n',0,0,0)]
with tempfile.TemporaryDirectory() as d:
 for index,(flags,wifi,www,mqtt) in enumerate(cases):
  p=Path(d)/'test.cpp';p.write_text(flags+'#include "ArdPortalFeatures.h"\n'+f'static_assert(ARDPORTAL_ENABLE_WIFI=={wifi});\nstatic_assert(ARDPORTAL_ENABLE_WWW=={www});\nstatic_assert(ARDPORTAL_ENABLE_MQTT=={mqtt});\n'+('static_assert(!ARDPORTAL_ENABLE_MQTT_TLS&&!ARDPORTAL_ENABLE_HA);\n' if not wifi else '')+('static_assert(!ARDPORTAL_SUPPORT_WEBSOCKET&&!ARDPORTAL_HTTP_OTA_SUPPORT);\nstatic_assert(ARDPORTAL_ENABLE_OTA);\n' if not www else '')+'int main(){}\n')
  subprocess.run(['c++','-std=c++17','-I'+str(root/'tests/host'),'-I'+str(root/'src'),str(p),'-o',str(Path(d)/'test')],check=True)
print('PASS: defaults, independent WWW switch and Wi-Fi override of WWW/MQTT/TLS/HA/WebSockets')
