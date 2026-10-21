"""Compile actual native power transition/callback bodies for ED and router."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'src/ArdZigbeeImpl.h').read_text()
assert source.index('esp_zb_sleep_enable(self._sleepyProfile)') < source.index('esp_zb_init(&config)')
assert source.index('esp_zb_set_rx_on_when_idle(!self._sleepyProfile)') < source.index('esp_zb_start(false)')
assert source.index('esp_zb_start(false)') < source.index('zb_set_rx_on_when_idle(self._sleepyProfile?ZB_FALSE:ZB_TRUE)') < source.index('esp_zb_stack_main_loop()')
for name in ('ArdPortalImpl.h','ZigbeePortalImpl.h'):
    text=(root/'src'/name).read_text()
    assert text.index('prepareZigbeePower()') < text.index('_zigbee.begin(_zigbeeOptions)')
transition=source[source.index('bool ArdZigbee::canDeepSleep'):source.index('/** @brief Observe SDK signals')]
signal=source[source.index('void ArdZigbee::signal('):source.index('/** @brief Notify a real state transition')]
fixture=r'''
#include <atomic>
#include <cassert>
#include <cstdio>
#include <memory>
constexpr int ESP_OK=0,ESP_FAIL=-1;
enum esp_zb_app_signal_type_t {ESP_ZB_COMMON_SIGNAL_CAN_SLEEP,ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP,
 ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START,ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT,ESP_ZB_BDB_SIGNAL_STEERING,
 ESP_ZB_BDB_SIGNAL_TC_REJOIN_DONE,ESP_ZB_ZDO_SIGNAL_LEAVE,ESP_ZB_NWK_SIGNAL_NO_ACTIVE_LINKS_LEFT};
struct esp_zb_app_signal_t {unsigned* p_app_signal;int esp_err_status;};
constexpr int ESP_ZB_BDB_MODE_INITIALIZATION=0;
uint32_t tick=6000;uint32_t millis(){return tick;}
unsigned uxQueueMessagesWaiting(unsigned q){return q;}
bool locked=false,available=true,rx=true,enabled=false;
unsigned changes=0,sleeps=0,logs=0;
bool esp_zb_lock_acquire(int){if(!available)return false;assert(!locked);return locked=true;}
void esp_zb_lock_release(){assert(locked);locked=false;}
void esp_zb_set_rx_on_when_idle(bool value){assert(locked);rx=value;++changes;}
bool esp_zb_get_rx_on_when_idle(){assert(locked);return rx;}
void esp_zb_sleep_enable(bool value){assert(locked);enabled=value;++changes;}
void esp_zb_sleep_now(){assert(enabled);++sleeps;}
int esp_zb_bdb_start_top_level_commissioning(int){return ESP_OK;}
const char* esp_zb_zdo_signal_to_string(esp_zb_app_signal_type_t){return "signal";}
struct ArdZigbee {
 enum class State {Connected,Pairing};
 bool _begun=false,_ready=false,working=false,ota=false,_sleepyProfile=false,_powerConfigured=false;State _state=State::Connected;
 std::atomic<bool> _powerSaving{false},_deepSleepWanted{false};
 std::atomic<uint32_t> _powerIdleAt{0};std::atomic<uint16_t> _timeSequence{256};
 uint32_t _powerConnectedAt=0;bool _deepSleepLocked=false;
 unsigned _events=0,_commands=0;
 struct Control {std::unique_ptr<Control> next;bool reportQueued=false;unsigned pendingReports=0;};
 std::unique_ptr<Control> _controls;
 static ArdZigbee* _instance;
 bool busy()const{return working;}bool otaActive()const{return ota;}
 void traffic(const char*){++logs;}void event(int,int){}
 bool canDeepSleep()const;bool prepareDeepSleep();void cancelDeepSleep();
 bool powerSaving(bool,bool=false);static void signal(esp_zb_app_signal_t*);
};
ArdZigbee* ArdZigbee::_instance=nullptr;
'''
main=r'''
int main(){
 ArdZigbee z;ArdZigbee::_instance=&z;
 unsigned kind=ESP_ZB_COMMON_SIGNAL_CAN_SLEEP;esp_zb_app_signal_t sig{&kind,ESP_OK};
 assert(!z.powerSaving(true)&&!changes);z._begun=true;z._ready=true;
#if ARDPORTAL_ZIGBEE_ROUTER
 assert(!z.canDeepSleep()&&!z.prepareDeepSleep()&&!locked);
 assert(!z.powerSaving(true)&&!changes);z.signal(&sig);assert(!sleeps&&rx);
#else
 assert(z._sleepyProfile);rx=false;enabled=true; // Applied by task before SDK start.
 available=false;assert(!z.powerSaving(true));available=true;
 z.working=true;assert(!z.powerSaving(true)&&!enabled);z.working=false;
 z.ota=true;assert(!z.powerSaving(true));z.ota=false;
 assert(z.powerSaving(true)&&!rx&&enabled);unsigned applied=changes;
 assert(z.powerSaving(true)&&changes==applied);
 z.signal(&sig);assert(sleeps==1&&!logs);
 assert(z.powerSaving(true,true));z.signal(&sig);assert(sleeps==1&&z.canDeepSleep());
 z._events=1;assert(!z.canDeepSleep());z._events=0;
 z._commands=1;assert(!z.canDeepSleep());z._commands=0;
 z._timeSequence=1;assert(!z.canDeepSleep());z._timeSequence=256;
 z._controls.reset(new ArdZigbee::Control);z._controls->reportQueued=true;
 assert(!z.canDeepSleep());z._controls->reportQueued=false;z._controls->pendingReports=1;
 assert(!z.canDeepSleep());z._controls->pendingReports=0;
 available=false;assert(!z.prepareDeepSleep()&&!locked);available=true;
 assert(z.prepareDeepSleep()&&locked);z.cancelDeepSleep();assert(!locked);
 z.cancelDeepSleep();tick+=101;assert(!z.canDeepSleep());
 z.signal(&sig);assert(z.canDeepSleep());z._powerConnectedAt=tick;assert(!z.canDeepSleep());
 tick+=5000;z.signal(&sig);assert(z.canDeepSleep());
 // Runtime changes toggle sleep, never the network capability after join.
 available=false;assert(!z.powerSaving(false));available=true;
 assert(!z.powerSaving(false)&&!rx&&!enabled);
 z.signal(&sig);assert(sleeps==1&&!logs);
 z._state=ArdZigbee::State::Pairing;assert(!z.powerSaving(true)&&!rx);
 // A full-receiver device cannot become sleepy by the silently ignored SDK setter.
 ArdZigbee late;late._begun=late._ready=true;rx=true;applied=changes;
 assert(!late.powerSaving(true)&&!late._powerSaving.load()&&changes==applied);
 // Even a staged profile must be verified against the actual SDK value.
 late._sleepyProfile=true;assert(!late.powerSaving(true)&&!late._powerSaving.load()&&!enabled);

#endif
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-zigbee-power-') as directory:
 d=Path(directory);cpp=d/'test.cpp';cpp.write_text(fixture+transition+signal+main)
 for router in (0,1):
  exe=d/'test'
  subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
   '-DARDPORTAL_ENABLE_DYNAMIC_PAGES=1','-DARDPORTAL_ZIGBEE_OTA_SUPPORT=1',
   f'-DARDPORTAL_ZIGBEE_ROUTER={router}',str(cpp),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True)
print('PASS: native Zigbee power transitions, locking, pairing/OTA guards, sleep callbacks and router exclusion')
