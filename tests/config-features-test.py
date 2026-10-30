"""Exercise real config codecs and API across feature switches and older full files."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
fixture = '#define ARDPORTAL_ENABLE_DYNAMIC_PAGES 0\n#define ARDPORTAL_ENABLE_OTA 0\n' + (root / 'tests/memory-host/mqtt-state-topic.cpp').read_text().split('int main() {', 1)[0].replace('../../src/', str(root / 'src') + '/')
main = r'''
int main() {
 ArdPortal::PortalConfig c, loaded;
 c.ssid="Home";c.password="wifi-password";c.apName="Setup";c.apPassword="ap-password";
 c.host="broker";c.port=8883;c.user="user";c.mqttPassword="mqtt-password";
 c.mqttTls=true;c.caCert="-----BEGIN CERTIFICATE-----\nTEST\n-----END CERTIFICATE-----\n";
 c.deviceName="Device";
 for(bool backup:{false,true}) {
  auto document=ArdPortalJson::value(c,backup);
  for(const char* key:{"ssid","password","apName","apPassword","apBand"})
   assert(document.hasOwnProperty(key)==bool(ARDPORTAL_ENABLE_WIFI));
  for(const char* key:{"host","port","user","mqttPassword"})
   assert(document.hasOwnProperty(key)==bool(ARDPORTAL_ENABLE_MQTT));
  for(const char* key:{"mqttTls","caCert"})
   assert(document.hasOwnProperty(key)==bool(ARDPORTAL_ENABLE_MQTT_TLS));
  String json=ArdPortalJson::encode(c,ArdJsonCodec(),backup);
  assert(json.length());
  assert(ArdPortalJson::decode(json,loaded,ArdJsonCodec(),backup));
  assert(ArdPortalJson::equal(c,loaded));
 }
 // Legacy full schema is accepted, disabled fields are discarded, unknown keys rejected.
 auto legacy=ArdJSON::JSONVar::object();legacy["version"]=1;
 legacy["ssid"]=c.ssid;legacy["password"]=c.password;
 legacy["apName"]=c.apName;legacy["apPassword"]=c.apPassword;legacy["apBand"]=24;
 legacy["host"]=c.host;legacy["port"]=c.port;legacy["user"]=c.user;
 legacy["mqttPassword"]=c.mqttPassword;legacy["mqttTls"]=c.mqttTls;legacy["caCert"]=c.caCert;
 legacy["deviceName"]=c.deviceName;
 assert(ArdPortalJson::decodeValue(legacy,loaded));
 assert(ArdPortalJson::equal(c,loaded));
#if !ARDPORTAL_ENABLE_WIFI
 assert(!loaded.ssid.length()&&!loaded.password.length());
 legacy["password"]=false;legacy["apBand"]="ignored";
#endif
#if !ARDPORTAL_ENABLE_MQTT
 assert(!loaded.host.length()&&!loaded.user.length()&&!loaded.mqttPassword.length());
 legacy["host"]=false;legacy["port"]="ignored";
#endif
#if !ARDPORTAL_ENABLE_MQTT_TLS
 assert(!loaded.mqttTls&&!loaded.caCert.length());
 legacy["mqttTls"]="ignored";legacy["caCert"]=false;
#endif
 assert(ArdPortalJson::decodeValue(legacy,loaded));
 legacy["unknown"]=1;assert(!ArdPortalJson::decodeValue(legacy,loaded));
 // Enabling a feature after a lean config uses defaults for its absent fields.
 auto minimal=ArdJSON::JSONVar::object();minimal["version"]=1;minimal["deviceName"]="Device";
 assert(ArdPortalJson::decodeValue(minimal,loaded));
 assert(loaded.port==1883&&!loaded.mqttTls&&!loaded.ssid.length());
#if ARDPORTAL_ENABLE_WWW
 ArdPortal portal;assert(portal.begin());
 for(unsigned i=0;i<1000;++i){fakeMillis+=10;portal.loop();}
 assert(portal._configurationReady&&!portal.portalAndAppConfigBusy());portal._config=c;
 portal.handleBuiltinHttp("GET","/api/config");
 auto api=ArdJSON::JSON.parse(portal._response.substring(portal._response.indexOf("\r\n\r\n")+4));
 assert(api.isValid());
 assert(api.hasOwnProperty("host")==bool(ARDPORTAL_ENABLE_MQTT));
 assert(api.hasOwnProperty("mqttName")==bool(ARDPORTAL_ENABLE_MQTT));
 assert(api.hasOwnProperty("caCert")==bool(ARDPORTAL_ENABLE_MQTT_TLS));
 assert(api.hasOwnProperty("mqttTls")==bool(ARDPORTAL_ENABLE_MQTT_TLS));
 assert(!api.hasOwnProperty("password")&&!api.hasOwnProperty("mqttPassword"));
#if !ARDPORTAL_ENABLE_MQTT_TLS
 for(const char* body:{"mqttTls=0","caCert=certificate"}) {
  portal.closeHttp();portal._request=String("POST /api/config HTTP/1.1\r\n\r\n")+body;
  portal.handleBuiltinHttp("POST","/api/config");
  assert(portal._response.indexOf("404")>=0&&!portal._savePending);
 }
#endif
#endif
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-config-features-') as directory:
    work = Path(directory)
    (work / 'test.cpp').write_text(fixture + main)
    for wifi, mqtt, tls in ((0, 0, 0), (1, 0, 0), (1, 1, 0), (1, 1, 1)):
        subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                        '-DESP8266', '-DARDFS_HOST_TEST', f'-DARDPORTAL_ENABLE_WIFI={wifi}',
                        f'-DARDPORTAL_ENABLE_MQTT={mqtt}', f'-DARDPORTAL_ENABLE_MQTT_TLS={tls}',
                        '-I' + str(root / 'tests/memory-host'), '-I' + str(root / 'src'), str(work / 'test.cpp'),
                        *[str(root / 'src' / name) for name in ('ArdStorage.cpp', 'JsonStream.cpp', 'JournalReader.cpp', 'JsonDocumentStream.cpp')],
                        '-o', str(work / 'test')], check=True)
        subprocess.run([str(work / 'test')], check=True)
print('PASS: config/API/backup fields follow Wi-Fi/MQTT/TLS; legacy fields ignored; feature defaults; strict unknown keys')
