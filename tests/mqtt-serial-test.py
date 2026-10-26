# Exercise actual MQTT Serial code with the web Console compiled out.
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parent.parent;s=(root/'src/ArdPortalImpl.h').read_text();start=s.index('static void ardMqttSerialDiagnostic');end=s.index('\n#endif',start);helper=s[start:end]
start=s.index('void ArdPortal::consoleLine(uint8_t channel, const String& text)',s.index('void ArdPortal::consoleLine(uint8_t channel, const String& text)')+1);end=s.index('\n}\n#endif',start)+2;line=s[start:end]
source='''#include <Arduino.h>
#include <cassert>
#define ARDPORTAL_MQTT_DEBUG_SERIAL 1
uint32_t fakeMillis=10;SerialStub Serial;
'''+helper+'\nclass ArdPortal {public:void consoleLine(uint8_t,const String&);};\n'+line+'''
int main(){ArdPortal p;p.consoleLine(true,"RX topic = ON");assert(Serial.output.find("MQTT RX topic = ON")!=std::string::npos);auto saved=Serial.output;p.consoleLine(false,"WiFi");assert(Serial.output==saved);Serial.capacity=0;p.consoleLine(true,"MQTT disconnected");assert(Serial.output==saved);Serial.capacity=-1;p.consoleLine(true,"MQTT disconnected");assert(Serial.output==saved);Serial.capacity=256;ardMqttSerialDiagnostic("MQTT connecting");assert(Serial.output.find("MQTT connecting")!=std::string::npos);}
'''
source = '#include "' + str(root / "src/LanguageLiterals.h") + '"\n' + source
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.cpp';p.write_text(source);exe=Path(d)/'test';subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(root/'tests/host'),str(p),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('PASS: MQTT Serial with Console disabled, selective routing, no wait on full/invalid buffers')
