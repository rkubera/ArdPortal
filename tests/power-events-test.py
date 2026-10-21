"""Run real policy and GPIO/ISR adapters against controlled SDK collaborators."""
from pathlib import Path
import subprocess
import tempfile
from power_sdk_stubs import arduino, wifi, sleep, pm, write_extra_stubs
root=Path(__file__).resolve().parents[1]
fixture=r'''
#include "ArdPower.h"
#include "PowerSignalsImpl.h"
#include <cassert>
#include <vector>
uint32_t tick=0;
// Use the actual GPIO adapter with an injectable clock for timer/deadline tests.
struct Backend:ArdPowerDetail::Signals {
 bool supportsSleep(ArdPowerSleepType){return true;}
 ArdPowerState lightSleep(uint32_t){return ArdPowerState::LightSleep;}
 static uint32_t now(){return tick;}
 static bool validDeepSleep(uint32_t ms){return ms!=0;}
 ArdPowerState apply(ArdPowerSleepType,bool,bool){return ArdPowerState::Awake;}
 void idle(uint32_t ms){idleMs=ms;}
 bool deepSleep(uint32_t){++deepCalls;return true;}
};
using Power=ArdPowerDetail::Manager<Backend>;
using Kind=ArdPowerEventKind;
std::vector<ArdPowerEvent> events;
Power* current=nullptr;
void collect(const ArdPowerEvent& event,void*){events.push_back(event);}
void replace(const ArdPowerEvent& event,void*){
 events.push_back(event);
 // Reentrant dispatch must not duplicate callbacks; registration can change safely.
 current->dispatchEvents();
 if(event.id==10){current->setTimer(10,5);current->cancelTimer(11);}
}
int main(){
 using Level=ArdPowerWakeLevel;using Scope=ArdPowerWakeScope;
 Power p;current=&p;p.onWake(collect);
 ArdPowerDetail::Context context;
 Power::Options options;options.policy=ArdPowerPolicy::EventDriven;options.sleep=ArdPowerSleepType::ConnectedSleep;options.sleepDelayMs=0;
 assert(p.configure(options));
 assert(!p.setTimer(1,0)&&!p.setTimer(1,uint32_t(INT32_MAX)+1));
 assert(p.setTimer(1,10)&&p.setTimer(2,7,true));
 tick=6;p.dispatchEvents();assert(events.empty());p.service(context);assert(idleMs==1);
 tick=7;p.dispatchEvents();assert(events.size()==1&&events.back().id==2&&events.back().missed==0);
 tick=10;p.dispatchEvents();assert(events.size()==2&&events.back().id==1);
 tick=35;p.dispatchEvents();assert(events.size()==3&&events.back().id==2&&events.back().missed==3);
 assert(p.cancelTimer(2)&&!p.cancelTimer(2));
 for(unsigned i=0;i<8;++i)assert(p.setTimer(uint8_t(i),20));
 assert(!p.setTimer(8,20));assert(p.setTimer(0,30)); // Replacement at capacity.
 for(unsigned i=0;i<8;++i)assert(p.cancelTimer(uint8_t(i)));
 tick=UINT32_MAX-3;assert(p.setTimer(3,7));tick=2;p.dispatchEvents();assert(events.size()==3);
 tick=3;p.dispatchEvents();assert(events.size()==4&&events.back().id==3);
 // Timer callbacks can replace themselves, cancel others, or dispatch recursively.
 p.onWake(replace);assert(p.setTimer(10,2)&&p.setTimer(11,20));tick=5;p.dispatchEvents();
 assert(events.back().id==10&&!p.cancelTimer(11));tick=10;p.dispatchEvents();assert(events.back().id==10);
 p.cancelTimer(10);p.onWake(collect);
 // ISR recording never invokes the callback; duplicates coalesce until dispatch.
 size_t before=events.size();p.signalFromISR(0);p.signalFromISR(0);p.signalFromISR(15);p.signalFromISR(16);
 assert(events.size()==before);p.service(context);assert(p.blockedBy(ArdPowerBlocker::Event));
 p.dispatchEvents();assert(events.size()==before+2&&events.back().kind==Kind::Interrupt&&events.back().id==15);
 // Pin registration validates scope, capacity and duplicates; held levels report once.
 assert(!p.addWakePin(255,Level::Low));
 pinLevels[4]=HIGH;assert(p.addWakePin(4,Level::Low));assert(!p.addWakePin(4,Level::High));
 assert(!p.setSleepType(ArdPowerSleepType::DeepSleep)); // Light-only GPIO must not silently lose its wake source.
 pinLevels[4]=LOW;triggerPin(4);before=events.size();assert(events.size()==before);
 p.dispatchEvents();assert(events.size()==before+1&&events.back().kind==Kind::Gpio&&events.back().id==4);
 p.service(context);assert(p.blockedBy(ArdPowerBlocker::WakePin));
 p.dispatchEvents();assert(events.size()==before+1); // Holding the button does not flood callbacks.
 pinLevels[4]=HIGH;p.dispatchEvents();pinLevels[4]=LOW;triggerPin(4);p.dispatchEvents();
 assert(events.size()==before+2);assert(p.removePin(4)&&!p.removePin(4));
 // Removed GPIO events cannot leak into a newly registered slot.
 pinLevels[4]=HIGH;assert(p.addWakePin(4,Level::Low));triggerPin(4);assert(p.removePin(4));
 assert(p.addInterruptPin(5,ArdPowerInterrupt::Falling));p.dispatchEvents();before=events.size();
 triggerPin(5);assert(events.size()==before);p.dispatchEvents();assert(events.size()==before+1&&events.back().id==5);
 p.service(context);assert(p.blockedBy(ArdPowerBlocker::Interrupt));assert(p.removePin(5));
 // A real event cancels an old deep-sleep request before any shutdown callback.
 assert(p.requestSleep(1000));p.signalFromISR(1);p.dispatchEvents();p.service(context);
 assert(!p.sleepPending()&&!deepCalls);
#if defined(ESP32)
 pinLevels[4]=HIGH;
 Power deep;
 Power::Options deepOptions;deepOptions.policy=ArdPowerPolicy::Manual;deepOptions.sleep=ArdPowerSleepType::DeepSleep;
 assert(deep.configure(deepOptions));
 assert(deep.addWakePin(4,Level::Low)); // Scope follows the selected sleep type.
 assert(!deep.addWakePin(5,Level::High,Scope::LightAndDeepSleep));
 assert(deep.requestSleep(1000));deep.cancelSleep();assert(deep.removePin(4));
 {
   ArdPowerDetail::Signals pins;assert(pins.addWakePin(4,Level::Low,Scope::LightAndDeepSleep));
   assert(pins.prepareDeepPins());pins.clearDeepPins();
   pinLevels[4]=LOW;assert(!pins.prepareDeepPins());pinLevels[4]=HIGH;
 }

 gpioFail=true;assert(!p.addWakePin(4,Level::Low));gpioFail=false;
#else
 assert(!p.addWakePin(4,Level::Low,Scope::LightAndDeepSleep));
 assert(!p.addInterruptPin(16,ArdPowerInterrupt::Change));
#endif
 assert(p.addInterruptPin(0,ArdPowerInterrupt::Rising));assert(p.addInterruptPin(2,ArdPowerInterrupt::Falling));
 assert(p.addInterruptPin(4,ArdPowerInterrupt::Change));assert(p.addInterruptPin(5,ArdPowerInterrupt::Rising));
 assert(!p.addInterruptPin(12,ArdPowerInterrupt::Change));
 assert(p.removePin(0)&&p.removePin(2)&&p.removePin(4)&&p.removePin(5));
 // Test wake from the actual ESP32 semaphore wait after an IRQ arrives mid-wait.
#if defined(ESP32)
 ArdPowerDetail::Signals signals;
 assert(signals.addInterruptPin(5,ArdPowerInterrupt::Falling));
 idleHook=[](){triggerPin(5);};signals.waitForSignal(20);idleHook=nullptr;
 assert(signals.hasSignals()&&(signals.takeSignals()&1));
#endif
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-power-events-') as directory:
 d=Path(directory);write_extra_stubs(d);(d/'soc').mkdir(exist_ok=True)
 (d/'soc/soc_caps.h').write_text('#pragma once\n')
 for name,data in {'Arduino.h':arduino,'WiFi.h':wifi,'ESP8266WiFi.h':wifi,'sdkconfig.h':'#pragma once\n','esp_sleep.h':sleep,'esp_pm.h':pm}.items():
  (d/name).write_text(data)
 (d/'test.cpp').write_text(fixture)
 for chip,wake_api in (('ESP8266','EXT0'),('ESP32','EXT0'),('ESP32','EXT1'),('ESP32','GPIO')):
  flags=[f'-D{chip}=1',f'-DSOC_PM_SUPPORT_EXT0_WAKEUP={int(wake_api=="EXT0")}',f'-DSOC_PM_SUPPORT_EXT1_WAKEUP={int(wake_api=="EXT1")}',f'-DSOC_GPIO_SUPPORT_DEEPSLEEP_WAKEUP={int(wake_api=="GPIO")}']
  executable=d/'test'
  subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                  '-I'+str(d),'-I'+str(root/'src'),*flags,str(d/'test.cpp'),'-o',str(executable)],check=True)
  subprocess.run([str(executable)],check=True)
print('PASS: actual ISR/GPIO latches, held-level suppression, deferred callbacks, timer capacity/replacement/missed periods/rollover, deep admission and wakeable wait')
