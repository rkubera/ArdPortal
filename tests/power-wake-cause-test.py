"""Exercise real boot-cause adapter through the public manager API."""
from pathlib import Path
import subprocess
import tempfile
from power_sdk_stubs import arduino, wifi, sleep, pm, write_extra_stubs
root=Path(__file__).resolve().parents[1]
fixture=r'''
#include "ArdPower.h"
#include "PowerSignalsImpl.h"
#include <cassert>
using Cause=ArdPower::WakeCause;
void check(Cause expected,bool deep){
 ArdPower p;
 assert(p.wakeCause()==expected&&p.wokeFromDeepSleep()==deep);
#if defined(ESP32)
 sdkWakeCause=ESP_SLEEP_WAKEUP_TIMER;sdkResetReason=ESP_RST_DEEPSLEEP;
#else
 resetInfo.reason=REASON_DEEP_SLEEP_AWAKE;
#endif
 // Later SDK state changes/light-sleep wake sources cannot overwrite the boot snapshot.
 assert(p.wakeCause()==expected&&p.wokeFromDeepSleep()==deep);
 ArdPower::Options options;options.policy=ArdPower::Policy::Periodic;
 assert(p.configure(options));assert(p.wakeCause()==expected);
}
int main(){
#if defined(ESP32)
 struct Mapping {int raw;Cause expected;};
 const Mapping resets[]={
  {ESP_RST_UNKNOWN,Cause::Unknown},{ESP_RST_POWERON,Cause::PowerOn},
  {ESP_RST_EXT,Cause::ExternalReset},{ESP_RST_SW,Cause::SoftwareReset},
  {ESP_RST_PANIC,Cause::Panic},{ESP_RST_INT_WDT,Cause::Watchdog},
  {ESP_RST_TASK_WDT,Cause::Watchdog},{ESP_RST_WDT,Cause::Watchdog},
  {ESP_RST_BROWNOUT,Cause::Brownout},{123,Cause::OtherReset}};
 for(auto entry:resets){sdkResetReason=entry.raw;sdkWakeCause=ESP_SLEEP_WAKEUP_TIMER;check(entry.expected,false);}
 const Mapping wakes[]={
  {ESP_SLEEP_WAKEUP_TIMER,Cause::Timer},{ESP_SLEEP_WAKEUP_EXT0,Cause::Gpio},
  {ESP_SLEEP_WAKEUP_EXT1,Cause::Gpio},{ESP_SLEEP_WAKEUP_GPIO,Cause::Gpio},
  {ESP_SLEEP_WAKEUP_UNDEFINED,Cause::DeepSleep},{123,Cause::OtherWake}};
 for(auto entry:wakes){sdkResetReason=ESP_RST_DEEPSLEEP;sdkWakeCause=entry.raw;check(entry.expected,true);}
#else
 struct Mapping {unsigned raw;Cause expected;bool deep;};
 const Mapping resets[]={
  {REASON_DEFAULT_RST,Cause::PowerOn,false},{REASON_WDT_RST,Cause::Watchdog,false},
  {REASON_SOFT_WDT_RST,Cause::Watchdog,false},{REASON_EXCEPTION_RST,Cause::Panic,false},
  {REASON_SOFT_RESTART,Cause::SoftwareReset,false},{REASON_DEEP_SLEEP_AWAKE,Cause::DeepSleep,true},
  {REASON_EXT_SYS_RST,Cause::ExternalReset,false},{123,Cause::Unknown,false}};
 for(auto entry:resets){resetInfo.reason=entry.raw;check(entry.expected,entry.deep);}
 nullResetInfo=true;check(Cause::Unknown,false);
#endif
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-wake-cause-') as directory:
 d=Path(directory);write_extra_stubs(d);(d/'soc').mkdir(exist_ok=True)
 (d/'soc/soc_caps.h').write_text('#pragma once\n')
 for name,data in {'Arduino.h':arduino,'WiFi.h':wifi,'ESP8266WiFi.h':wifi,
                   'sdkconfig.h':'#pragma once\n','esp_sleep.h':sleep,'esp_pm.h':pm}.items():
  (d/name).write_text(data)
 cpp=d/'test.cpp';cpp.write_text(fixture)
 for chip,auto in (('ESP8266',0),('ESP32',0),('ESP32',1)):
  exe=d/'test'
  subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
    '-I'+str(d),'-I'+str(root/'src'),f'-D{chip}=1','-DARDPORTAL_ENABLE_WIFI=0',
    f'-DCONFIG_PM_ENABLE={auto}',
    f'-DCONFIG_FREERTOS_USE_TICKLESS_IDLE={auto}',str(cpp),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True)
print('PASS: public boot/wake cause API, reset and wake mappings, ESP8266 source ambiguity and immutable snapshots')
