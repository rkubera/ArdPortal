#!/usr/bin/env python3
"""Check bounded, transition-only portal decisions for Matter Wi-Fi and Thread."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=r'''
#include "PowerDiagnostics.h"
#include <cassert>
uint32_t fakeMillis=0;
int main(){
 static_assert(sizeof(ArdPowerDiagnostics)==4,"Diagnostics must use only four bytes");
 ArdPowerDiagnostics d;
 using P=ArdPowerPolicy;using S=ArdPowerSleepType;using B=ArdPowerBlocker;
 auto update=[&](uint16_t blockers,bool admitted,bool released,bool connected=true,bool supported=true){return d.update(P::EventDriven,S::ConnectedSleep,blockers,admitted,released,connected,supported);};
 auto text=update(0,true,true);
#if ARDPORTAL_MATTER_THREAD
 assert(text.indexOf("Thread")>=0);
#else
 assert(text.indexOf("Wi-Fi")>=0);
#endif
 assert(text.indexOf("allowed")>=0);
 for(int i=0;i<1000;++i)assert(!update(0,true,true).length());
 text=update(uint16_t(B::Http)|uint16_t(B::WebSocket),false,false);
 assert(text.indexOf("HTTP")>=0&&text.indexOf("WebSocket")>=0&&text.indexOf("paused")>=0);
 assert(!update(uint16_t(B::Http)|uint16_t(B::WebSocket),false,false).length());
  for(int i=0;i<1000;++i){
  assert(!update(uint16_t(B::WebSocket),false,false).length());
  assert(!update(uint16_t(B::Http),false,false).length());
  assert(!update(uint16_t(B::Http)|uint16_t(B::WebSocket),false,false).length());
 }
 assert(!update(uint16_t(B::Storage),false,false).length());
 assert(update(0,true,true).indexOf("allowed")>=0);
 text=update(uint16_t(B::Storage),false,false);assert(text.indexOf("storage")>=0);
 assert(!update(0,false,false).length());
 assert(update(0,true,true).length());
 text=update(0,false,false);assert(text.indexOf("idle delay")>=0);
 text=update(0,true,false);assert(text.indexOf("Matter work")>=0);
 text=update(0,true,true);assert(text.indexOf("allowed")>=0);
 text=update(0,false,false,false);assert(text.indexOf("disconnected")>=0);
 assert(!update(uint16_t(B::Storage),false,false,false).length());
 text=update(0,false,false,true,false);assert(text.indexOf("ICD")>=0);
 text=d.update(P::AlwaysOn,S::ConnectedSleep,0,false,false,true,true);assert(text.indexOf("disabled")>=0);
 assert(!d.update(P::AlwaysOn,S::ConnectedSleep,uint16_t(B::Storage),false,false,true,true).length());
 text=d.update(P::Manual,S::None,0,false,false,true,true);assert(text.indexOf("disabled")>=0);
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-power-diagnostics-') as directory:
    path=Path(directory);(path/'test.cpp').write_text(source)
    for thread in (0,1):
        subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-DARDPORTAL_MATTER_THREAD='+str(thread),'-DARDPORTAL_ENABLE_WWW=1','-DARDPORTAL_SUPPORT_WEBSOCKET=1','-DARDPORTAL_ENABLE_WIFI=1','-I'+str(root/'tests/host'),'-I'+str(root/'src'),str(path/'test.cpp'),'-o',str(path/'test')],check=True)
        subprocess.run([str(path/'test')],check=True)
print('PASS: four-byte power diagnostics, Wi-Fi/Thread, decision transitions, no HTTP/WebSocket polling spam')
