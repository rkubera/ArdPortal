"""Validate binary MQTT transmit framing and allocation failure atomically."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/ArdMQTT.h').read_text()
start = source.index('bool ArdMQTTClient<IO>::queuePacket(')
method = source[start:source.index('\ntemplate<class IO>', start)]
method = method.replace('ArdMQTTClient<IO>::', 'ArdMQTT::')
fixture = r'''
#include <Arduino.h>
#include <cassert>
#include <cstring>
uint32_t fakeMillis=123;
class ArdMQTT {public:
 static constexpr size_t PacketCapacity=2048;
 String _tx;
 size_t _txSize=0,_txOffset=0;
 uint32_t _txSince=0;
 bool queuePacket(uint8_t,const String&);
};
'''
main = r'''
int main() {
 ArdMQTT mqtt;
 String body;
 for(size_t i=0;i<130;++i)body+=char(i);
 assert(mqtt.queuePacket(0x30,body));
 assert(mqtt._txSize==133&&mqtt._tx.length()==133&&mqtt._txSince==123);
 assert(uint8_t(mqtt._tx[0])==0x30&&uint8_t(mqtt._tx[1])==0x82&&uint8_t(mqtt._tx[2])==1);
 assert(!memcmp(mqtt._tx.c_str()+3,body.c_str(),body.length()));
 assert(!mqtt.queuePacket(0xc0,String())&&mqtt._txSize==133);
 mqtt._tx=String();mqtt._txSize=0;
 String::reserveFailCountdownForTest()=0;
 assert(!mqtt.queuePacket(0x30,body)&&!mqtt._txSize&&!mqtt._tx.length());
 String::reserveFailCountdownForTest()=SIZE_MAX;
 assert(mqtt.queuePacket(0xc0,String())&&mqtt._txSize==2);
 assert(uint8_t(mqtt._tx[0])==0xc0&&uint8_t(mqtt._tx[1])==0);
 mqtt._tx=String();mqtt._txSize=0;
 String maximum;
 for(size_t i=0;i<ArdMQTT::PacketCapacity-5;++i)maximum+='x';
 assert(mqtt.queuePacket(0x30,maximum)&&mqtt._txSize==2046);
 mqtt._tx=String();mqtt._txSize=0;maximum+='x';
 assert(!mqtt.queuePacket(0x30,maximum)&&!mqtt._txSize&&!mqtt._tx.length());
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / 'test.cpp'
    path.write_text(fixture + method + main)
    executable = Path(directory) / 'test'
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-I' + str(root / 'tests/memory-host'),
                    str(path), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
print('PASS: MQTT binary framing, variable length, packet capacity, busy and allocation failure')
