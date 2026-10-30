"""Exercise portal guards while delegating unsubscribe to the selected backend."""
from pathlib import Path
import subprocess, tempfile
root = Path(__file__).resolve().parents[1]
s = (root / 'src/ArdMqttImpl.h').read_text()
a = s.index('bool ArdMqtt::unsubscribe(')
method = s[a:s.index('\n}', a)+2]
fixture = r'''
#include <Arduino.h>
#include <cassert>
class ArdMqtt {public:
 bool online=true;
 struct { bool result=true; unsigned calls=0; bool unsubscribe(const char*){++calls;return result;} } _protocol;
 bool connected(){return online;}
 bool validMqttTopic(const String& topic,bool){return topic=="stat/test/#";}
 bool unsubscribe(const char*);
};
int main(){
 ArdMqtt mqtt;
 assert(!mqtt.unsubscribe(nullptr)&&!mqtt.unsubscribe("bad")&&!mqtt._protocol.calls);
 mqtt.online=false;assert(!mqtt.unsubscribe("stat/test/#")&&!mqtt._protocol.calls);
 mqtt.online=true;assert(mqtt.unsubscribe("stat/test/#")&&mqtt._protocol.calls==1);
 mqtt._protocol.result=false;assert(!mqtt.unsubscribe("stat/test/#")&&mqtt._protocol.calls==2);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.cpp';p.write_text(fixture.replace('int main()', method+'\nint main()'));exe=Path(d)/'test'
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'tests/host'),str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: MQTT portal unsubscribe validation and selected backend delegation')
