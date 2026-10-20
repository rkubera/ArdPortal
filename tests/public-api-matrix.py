# Generate Arduino sketches with compile-time public API presence/absence assertions.
# Usage: python3 tests/public-api-matrix.py OUTPUT_DIRECTORY
from pathlib import Path
import sys
root=Path(sys.argv[1]);root.mkdir(parents=True,exist_ok=True)
cases={
'all':{},
'no_www':{'WWW':0},
'no_network':{'WIFI':0,'OTA':0,'DYNAMIC_PAGES':0,'CONSOLE_MESSAGES':0},
'no_dynamic':{'DYNAMIC_PAGES':0},
'no_actions':{'CONTROLS':0},
'local_dynamic':{'MQTT':0,'HA':0,'CONSOLE_MESSAGES':1,'DEPENDENCIES':0},
}
methods={'connectWifiStation':'ARDPORTAL_ENABLE_WIFI','startAccessPoint':'ARDPORTAL_ENABLE_WIFI','wifiConnected':'ARDPORTAL_ENABLE_WIFI','mqtt':'ARDPORTAL_ENABLE_MQTT','zigbeeConverterExportReady':'ARDPORTAL_ZIGBEE_EXPORT_SUPPORT','mqttConnected':'ARDPORTAL_ENABLE_MQTT','tlsClockReady':'ARDPORTAL_ENABLE_MQTT_TLS','ota':'ARDPORTAL_HTTP_OTA_SUPPORT','homeAssistant':'ARDPORTAL_ENABLE_HA','appConfigPageRegistrationBusy':'ARDPORTAL_ENABLE_DYNAMIC_PAGES','appConfigRegistrationError':'ARDPORTAL_ENABLE_DYNAMIC_PAGES'}
extra={'startZigbee2MqttConverterExport':('std::declval<T&>().startZigbee2MqttConverterExport(nullptr)','ARDPORTAL_ZIGBEE_CONVERTER_SUPPORT'),'startZhaQuirkExport':('std::declval<T&>().startZhaQuirkExport(nullptr)','ARDPORTAL_ZHA_QUIRK_SUPPORT'),'logMessage':('std::declval<T&>().logMessage(std::declval<const String&>())','ARDPORTAL_ENABLE_CONSOLE_MESSAGES'), 'publish':('std::declval<T&>().publish("stat/test/value","1")','ARDPORTAL_ENABLE_MQTT'), 'queueAppConfigStatePublish':('std::declval<T&>().queueAppConfigStatePublish("test")','ARDPORTAL_ENABLE_DYNAMIC_PAGES && ARDPORTAL_ENABLE_MQTT'), 'startAppConfigPageRegistration':('std::declval<T&>().startAppConfigPageRegistration(std::declval<const String&>())','ARDPORTAL_ENABLE_DYNAMIC_PAGES'), 'addAppConfigEntity':('std::declval<T&>().addAppConfigEntity(std::declval<const String&>())','ARDPORTAL_ENABLE_DYNAMIC_PAGES && ARDPORTAL_ENABLE_HA'), 'emitAppConfigEvent':('std::declval<T&>().emitAppConfigEvent("test",std::declval<const ArdJSON::JSONVar&>())','ARDPORTAL_ENABLE_DYNAMIC_PAGES && ARDPORTAL_CONTROL_SUPPORT_EVENTS'), 'onAppConfigCommand':('std::declval<T&>().onAppConfigCommand(nullptr)','ARDPORTAL_ENABLE_DYNAMIC_PAGES && ARDPORTAL_CONTROL_SUPPORT_ACTIONS')}
for case,flags in cases.items():
 d=root/case;d.mkdir(exist_ok=True);s=''.join(f'#define ARDPORTAL_ENABLE_{key} {value}\n' for key,value in flags.items());s+='#include <ArdPortal.h>\n#include <type_traits>\n#include <utility>\n'
 probes={key:(f'std::declval<T&>().{key}()',flag) for key,flag in methods.items()};probes.update(extra)
 for key,(expr,flag) in probes.items():
  s+=f'template<class T,class=void> struct has_{key}:std::false_type{{}};\ntemplate<class T> struct has_{key}<T,std::void_t<decltype({expr})>>:std::true_type{{}};\nstatic_assert(has_{key}<ArdPortal>::value==bool({flag}),"{key} availability");\n'
 s+='ArdPortal portal;\nvoid setup(){portal.begin();}\nvoid loop(){portal.loop();}\n';(d/(case+'.ino')).write_text(s)
print(root)
