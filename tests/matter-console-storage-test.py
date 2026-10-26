"""Exercise actual independent bounded console storage for Matter traffic."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'src/ArdPortalImpl.h').read_text()
a=source.index('void ArdPortal::consoleLine(uint8_t channel, const String& text)')
b=source.index('\n}\n#else',a)+2
method=source[a:b]
fixture=r'''
#include <Arduino.h>
#include "LanguageLiterals.h"
#include <cassert>
#include <ctime>
uint32_t fakeMillis=1;time_t fakeUnixTime=0;
namespace ArdHeap {struct Sample{unsigned free8=50000,block8=20000;};Sample sample(){return {};}}
bool utcTimestamp(time_t,char*,size_t){return false;}
struct ArdPortal {
 struct ConsoleLine{uint32_t id=0;uint8_t channel=0;String text;};
 static constexpr size_t ConsoleCapacity=16,MatterConsoleCapacity=8;
#if ARDPORTAL_ENABLE_MQTT
 ConsoleLine _console[ConsoleCapacity];uint32_t _consoleMqttCount=0;
#endif
#if ARDPORTAL_ENABLE_CONSOLE_MESSAGES
 ConsoleLine _consoleMessages[ConsoleCapacity];uint32_t _consoleMessagesCount=0;
#endif
#if ARDPORTAL_ENABLE_MATTER_CONSOLE
 ConsoleLine _matterConsole[MatterConsoleCapacity];uint32_t _matterConsoleCount=0;
#endif
 size_t _consoleHistoryBudget=4096;uint32_t _consoleId=0;
 bool tlsClockReady(){return false;}void consoleLine(uint8_t,const String&);
};
'''+method+r'''
int main(){
 ArdPortal p;
 p.consoleLine(2,"Received: Desk socket — Turn on — Accepted");
#if ARDPORTAL_ENABLE_MATTER_CONSOLE
 assert(p._matterConsoleCount==1&&p._matterConsole[0].channel==2);
 for(int i=0;i<40;++i)p.consoleLine(2,"State updated: Desk socket — On; repeated long diagnostic record for bounded storage");
 p.consoleLine(2,"Last command");size_t bytes=0;bool last=false;
 for(auto& line:p._matterConsole){bytes+=line.text.length();last|=line.text.indexOf("Last command")>=0;}
 assert(bytes<=1024&&last);
#else
 assert(!p._consoleId);
#endif
#if ARDPORTAL_ENABLE_MQTT
 assert(!p._consoleMqttCount);p.consoleLine(1,"MQTT");assert(p._consoleMqttCount==1);
#endif
#if ARDPORTAL_ENABLE_CONSOLE_MESSAGES
 assert(!p._consoleMessagesCount);p.consoleLine(0,"Lifecycle");assert(p._consoleMessagesCount==1);
#endif
 auto count=p._consoleId;p.consoleLine(3,"invalid");assert(p._consoleId==count);
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-matter-console-storage-') as directory:
 cpp=Path(directory)/'test.cpp';cpp.write_text(fixture);exe=Path(directory)/'test'
 for matter,messages,mqtt in ((1,1,1),(1,0,0),(0,1,1)):
  subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
   f'-DARDPORTAL_ENABLE_MATTER_CONSOLE={matter}',f'-DARDPORTAL_ENABLE_CONSOLE_MESSAGES={messages}',f'-DARDPORTAL_ENABLE_MQTT={mqtt}',
   '-I'+str(root/'src'),'-I'+str(root/'tests/host'),str(cpp),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True)
print('PASS: independent Matter history, 1024-byte text limit, FIFO eviction, separate MQTT/Messages and disabled feature')
