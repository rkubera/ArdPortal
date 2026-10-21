"""Exercise actual platform adapter with SDK calls replaced by deterministic collaborators."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
from power_sdk_stubs import arduino, wifi, sleep, pm, write_extra_stubs
fixture=r'''
#include "ArdPower.h"
#include "PowerSignalsImpl.h"
#include <cassert>
using Sleep=ArdPowerSleepType;using State=ArdPowerState;
int main(){
 ArdPowerDetail::Platform platform;
 assert(platform.apply(Sleep::None,false,false)==State::Awake);
#if ARDPORTAL_ENABLE_WIFI
 assert(platform.apply(Sleep::None,true,false)==State::Awake&&WiFi.sleep==0);
 assert(platform.apply(Sleep::ConnectedSleep,true,false)==State::ModemSleep&&WiFi.sleep==1);
#if defined(ESP8266) || ARDPORTAL_POWER_AUTO_LIGHT_SLEEP
 assert(platform.apply(Sleep::ConnectedSleep,true,true)==State::ConnectedSleep);
#else
 assert(platform.apply(Sleep::ConnectedSleep,true,true)==State::ModemSleepFallback);
#endif
#if ARDPORTAL_POWER_AUTO_LIGHT_SLEEP
 assert(pmConfig.light_sleep_enable&&pmConfig.max_freq_mhz==160);
#endif
 assert(platform.apply(Sleep::ConnectedSleep,true,false)==State::ModemSleep&&WiFi.sleep==1);
#if ARDPORTAL_POWER_AUTO_LIGHT_SLEEP
 assert(!pmConfig.light_sleep_enable);
#endif
 assert(platform.apply(Sleep::None,true,false)==State::Awake&&WiFi.sleep==0);
#if defined(ESP8266)
 assert(WiFi.interval==0);
#endif
#if ARDPORTAL_POWER_AUTO_LIGHT_SLEEP
 assert(!pmConfig.light_sleep_enable&&pmConfig.max_freq_mhz==80&&pmConfig.min_freq_mhz==40);
 pmFail=true;assert(platform.apply(Sleep::ConnectedSleep,true,true)==State::Error);pmFail=false;
#endif
 WiFi.fail=true;assert(platform.apply(Sleep::ConnectedSleep,true,false)==State::Error);WiFi.fail=false;
#endif
 platform.idle(7);assert(idleMs==7);
#if defined(ESP32)
 assert(platform.supportsSleep(Sleep::LightSleep));
 // Native Zigbee light sleep uses automatic PM, never standalone sleep here.
#if ARDPORTAL_POWER_AUTO_LIGHT_SLEEP
 assert(platform.apply(Sleep::LightSleep,false,true)==State::LightSleep&&pmConfig.light_sleep_enable);
#else
 assert(platform.apply(Sleep::LightSleep,false,true)==State::ModemSleepFallback);
#endif
 assert(!lightCalls);
 assert(platform.apply(Sleep::None,false,false)==State::Awake);
#if ARDPORTAL_POWER_AUTO_LIGHT_SLEEP
 assert(!pmConfig.light_sleep_enable);
#endif
#if ARDPORTAL_ENABLE_WIFI
 WiFi.radio=0;
#endif
 assert(platform.lightSleep(7)==State::LightSleep&&lightCalls==1);
 timerFail=true;assert(platform.lightSleep(7)==State::Error&&lightCalls==1);timerFail=false;
 lightResult=ESP_ERR_SLEEP_REJECT;assert(platform.lightSleep(7)==State::Awake);lightResult=ESP_OK;
#if ARDPORTAL_ENABLE_WIFI
 WiFi.radio=1;
#endif
#else
 assert(!platform.supportsSleep(Sleep::LightSleep)&&platform.lightSleep(7)==State::Error);
#endif
#if defined(ESP32)
 #if ARDPORTAL_POWER_AUTO_LIGHT_SLEEP
 pmConfig.light_sleep_enable=true;
#endif
 timerFail=true;assert(!platform.deepSleep(1000)&&!deepCalls);timerFail=false;
#if ARDPORTAL_POWER_AUTO_LIGHT_SLEEP
 assert(pmConfig.light_sleep_enable);
#endif
#if ARDPORTAL_ENABLE_WIFI
 assert(WiFi.radio==1);
#endif
#else
 assert(!platform.validDeepSleep(100001)&&platform.validDeepSleep(100000));
#endif
 assert(!platform.deepSleep(1000)&&deepCalls==1); // mock returns; hardware never returns
#if ARDPORTAL_ENABLE_WIFI
 assert(WiFi.radio==0);
#endif
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-power-platform-') as directory:
 d=Path(directory);write_extra_stubs(d);(d/'soc').mkdir(exist_ok=True);(d/'soc/soc_caps.h').write_text('#pragma once\n')
 for name,data in {'Arduino.h':arduino,'WiFi.h':wifi,'ESP8266WiFi.h':wifi,'sdkconfig.h':'#pragma once\n','esp_sleep.h':sleep,'esp_pm.h':pm}.items():
  (d/name).write_text(data)
 (d/'test.cpp').write_text(fixture)
 for chip,auto,wifi_enabled in (('ESP8266',0,1),('ESP8266',0,0),('ESP32',0,1),('ESP32',1,1),('ESP32',0,0),('ESP32',1,0)):
  flags=[f'-D{chip}=1',f'-DARDPORTAL_ENABLE_WIFI={wifi_enabled}',f'-DCONFIG_PM_ENABLE={auto}',f'-DCONFIG_FREERTOS_USE_TICKLESS_IDLE={auto}']
  executable=d/'test'
  subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                  '-I'+str(d),'-I'+str(root/'src'),*flags,str(d/'test.cpp'),'-o',str(executable)],check=True)
  subprocess.run([str(executable)],check=True)
print('PASS: actual power adapters; ESP8266 sleep modes, ESP32 PM/fallback/restoration, SDK failures and network-free builds')
