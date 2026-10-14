from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/ArdMqttImpl.h').read_text()
a=s.index('bool ArdMqtt::unsubscribe(');b=s.index('\n/**',a);method=s[a:b]
fixture=r'''
#include <Arduino.h>
#include <cassert>
uint32_t fakeMillis=123;
void appendText(String& packet,const String& text){packet+=char(text.length()>>8);packet+=char(text.length()&255);packet+=text;}
class ArdMqtt {public:
 static constexpr size_t PacketCapacity=2048;
 bool online=true,_subscriptionPending=false,_unsubscribePending=false,busy=false;uint16_t _packetId=0,_subscriptionId=0;uint32_t _subscriptionSince=0;uint8_t type=0;String packet;
 bool connected(){return online;}bool validMqttTopic(const String& topic,bool){return topic=="stat/test/#";}
 bool queuePacket(uint8_t kind,const String& body){if(busy)return false;type=kind;packet=body;return true;}
 bool unsubscribe(const char*);
};
'''
main=r'''
int main(){
 ArdMqtt mqtt;assert(!mqtt.unsubscribe(nullptr)&&!mqtt.unsubscribe("bad"));mqtt.online=false;assert(!mqtt.unsubscribe("stat/test/#"));mqtt.online=true;
 mqtt.busy=true;assert(!mqtt.unsubscribe("stat/test/#")&&!mqtt._subscriptionPending);mqtt.busy=false;
 assert(mqtt.unsubscribe("stat/test/#")&&mqtt.type==0xa2&&mqtt._subscriptionPending&&mqtt._unsubscribePending);
 const String& bytes=mqtt.packet;assert(bytes.length()==15);assert(uint8_t(bytes[2])==0&&uint8_t(bytes[3])==11);assert(bytes.substring(4)=="stat/test/#");
 assert((uint16_t(uint8_t(bytes[0]))<<8|uint8_t(bytes[1]))==mqtt._subscriptionId);
 assert(!mqtt.unsubscribe("stat/test/#"));
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.cpp';p.write_text(fixture+method+main);exe=Path(d)/'test'
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'tests/host'),str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: MQTT unsubscribe wire format, packet ID, pending/busy and invalid/disconnected guards')
