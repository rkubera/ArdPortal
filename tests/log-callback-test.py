#!/usr/bin/env python3
"""Exercise actual callback dispatch and no-WWW log producers, without history."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/ArdPortalImpl.h').read_text()
def function(signature,last=False):
 a=s.rindex(signature) if last else s.index(signature)
 return s[a:s.index('\n}',a)+2]
fixture=r'''
#define ARDPORTAL_ENABLE_WWW 0
#define ARDPORTAL_ENABLE_LOG_CALLBACK 1
#include "ArdPortalFeatures.h"
#include <Arduino.h>
#include <cassert>
#include <vector>
uint32_t fakeMillis=0;
namespace ArdUILanguage {enum class Key{s_178};String text(Key){return "Binary payload, bytes: ";}}
struct ArdPortal {
 enum class LogChannel:uint8_t {Messages,Mqtt,Matter,Zigbee};
 using LogCallback=void(*)(LogChannel,const String&);
 LogCallback _logCallback=nullptr;bool _dispatchingLog=false;
 void onLog(LogCallback cb){_logCallback=cb;}
 void emitLog(LogChannel,const String&);
 void consoleLine(uint8_t,const String&);
 void log(const String&);
 void logMqtt(const String&,const String&,const uint8_t*,size_t);
};
static_assert(!ARDPORTAL_ENABLE_CONSOLE&&!ARDPORTAL_SUPPORT_WEBSOCKET,"no WWW means no history/socket");
static_assert(ARDPORTAL_ENABLE_CONSOLE_MESSAGES,"callback enables Messages by default");
ArdPortal portal;
std::vector<std::pair<ArdPortal::LogChannel,String>> lines;
void sink(ArdPortal::LogChannel ch,const String& message){
 lines.emplace_back(ch,message);
 portal.log("nested log is not forwarded recursively");
}
'''
main=r'''
int main(){
 portal.log("without subscriber");assert(lines.empty());
 portal.onLog(sink);
 portal.log("startup");portal.consoleLine(2,"Matter command");portal.emitLog(ArdPortal::LogChannel::Zigbee,"Zigbee command");
 const uint8_t payload[]={'O','N'};
 portal.logMqtt("RX","outlet",payload,2);
 assert(lines.size()==4);
 assert(lines[0].first==ArdPortal::LogChannel::Messages&&lines[0].second=="startup");
 assert(lines[1].first==ArdPortal::LogChannel::Matter&&lines[1].second=="Matter command");
 assert(lines[2].first==ArdPortal::LogChannel::Zigbee);
 assert(lines[3].first==ArdPortal::LogChannel::Mqtt&&lines[3].second=="RX outlet = ON");
 portal.onLog(nullptr);portal.log("detached");assert(lines.size()==4);
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-log-callback-') as directory:
 path=Path(directory)
 (path/'test.cpp').write_text(fixture+function('void ArdPortal::emitLog(')+function('void ArdPortal::consoleLine(',True)+function('void ArdPortal::log(const String& message)')+function('void ArdPortal::logMqtt(const String& direction')+main)
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'tests/host'),'-I'+str(root/'src'),str(path/'test.cpp'),'-o',str(path/'test')],check=True)
 subprocess.run([str(path/'test')],check=True)
print('PASS: actual no-WWW callback dispatch, channel routing, MQTT data, detach and recursion guard')
