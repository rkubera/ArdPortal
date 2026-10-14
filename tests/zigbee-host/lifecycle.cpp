#include <cassert>
#define ARDPORTAL_ZIGBEE_ROUTER 0
#include "ArdZigbeeImpl.h"
uint32_t fakeMillis=0;
time_t fakeUnixTime=0;
/** @brief Deliver a native signal and advance actual module code. @param module Module. @param type Signal. @param ok Result. @return No value. */
void signal(ArdZigbee& module,esp_zb_app_signal_type_t type,bool ok=true){uint32_t value=type;esp_zb_app_signal_t event{&value,ok?ESP_OK:ESP_FAIL};ArdZigbee::signal(&event);module.loop(fakeMillis);}
/** @brief Test real cooperative state machine against controlled SDK responses. @return Zero on success. */
int main(int argc,char** argv){
 ArdZigbee::Options defaults;
 {ArdZigbee failed;queueBudget=0;assert(!failed.begin(defaults)&&failed.state()==ArdZigbee::State::Failed);}
 {ArdZigbee failed;queueBudget=1;assert(!failed.begin(defaults));}
 queueBudget=-1;
 {ArdZigbee failed;taskResult=0;assert(!failed.begin(defaults));}
 taskResult=pdPASS;
 if(argc>1&&String(argv[1])=="saved-network"){ArdZigbee saved;factoryNew=false;unsigned called=0;saved.onFinished([&](ArdZigbee::Operation op,bool ok,const String&){assert(op==ArdZigbee::Operation::Rejoin&&ok);++called;});assert(saved.begin(defaults));signal(saved,ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT);assert(called==1&&steering==0&&resets==0&&saved.hasNetwork());puts("PASS: automatic saved-network boot rejoin preserves identity");return 0;}
 if(argc>1&&String(argv[1])=="time-service"){
  ArdZigbee clock;ArdZigbee::Options options;factoryNew=false;assert(clock.begin(options));signal(clock,ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT);
  fakeMillis=1000;clock.loop(fakeMillis);assert(timeReads==1);
  fakeMillis=10999;clock.loop(fakeMillis);assert(timeReads==1);
  fakeMillis=11000;clock.loop(fakeMillis);assert(timeReads==1);
  fakeMillis=3611000;clock.loop(fakeMillis);assert(timeReads==2);
  puts("PASS: asynchronous coordinator Time probe and hourly timeout retry");return 0;
 }
 ArdZigbee z;ArdZigbee::Options opts;opts.timeoutMs=100;opts.attemptTimeoutMs=25;opts.retryMs=5;opts.maxAttempts=2;
 unsigned results=0;bool success=false;ArdZigbee::Operation completed;
 bool lifecycleMessages[2]={false,false};z.onLifecycleDiagnostic(lifecycleMessages,[](void* context,const String& message){auto* seen=static_cast<bool*>(context);if(message=="Zigbee paired and connected")seen[0]=true;if(message=="Zigbee rejoined saved network")seen[1]=true;});
 z.onFinished([&](ArdZigbee::Operation op,bool ok,const String&){++results;success=ok;completed=op;});
 fakeMillis=10;assert(z.begin(opts)&&z.busy());z.loop(9);assert(z.busy());assert(!z.request(ArdZigbee::Operation::Pair,opts));
 signal(z,ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START);assert(steering==1&&z.busy()&&z.state()==ArdZigbee::State::Pairing);
 factoryNew=false;signal(z,ESP_ZB_BDB_SIGNAL_STEERING);assert(results==1&&success&&z.hasNetwork());
 assert(z.request(ArdZigbee::Operation::Rejoin,opts));z.loop(fakeMillis);assert(initialization==1&&resets==0);
 signal(z,ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT);assert(results==2&&success&&completed==ArdZigbee::Operation::Rejoin);
 assert(lifecycleMessages[0]&&lifecycleMessages[1]);
 // Link loss waits before rejoining and preserves identity across exhausted attempts.
 signal(z,ESP_ZB_NWK_SIGNAL_NO_ACTIVE_LINKS_LEFT);assert(z.state()==ArdZigbee::State::Failed&&!z.busy());
 fakeMillis+=29999;z.loop(fakeMillis);assert(initialization==1);
 ++fakeMillis;z.loop(fakeMillis);assert(initialization==2&&z.busy()&&resets==0);
 fakeMillis+=100;z.loop(fakeMillis);assert(!z.busy()&&z.state()==ArdZigbee::State::Failed);
 fakeMillis+=29999;z.loop(fakeMillis);assert(initialization==2);
 ++fakeMillis;z.loop(fakeMillis);assert(initialization==3&&z.busy()&&resets==0);
 // Complete recovery and restore the callback count for the remaining lifecycle checks.
 signal(z,ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT);results=2;
 assert(z.request(ArdZigbee::Operation::ResetPairing,opts));z.loop(fakeMillis);assert(resets==1);
 factoryNew=true;signal(z,ESP_ZB_ZDO_SIGNAL_LEAVE);assert(steering==2&&!z.hasNetwork());
 assert(z.stopPairing());z.loop(fakeMillis);assert(results==3&&!success&&z.state()==ArdZigbee::State::Idle);
 fakeMillis+=50;z.loop(fakeMillis);assert(steering==2);
 assert(z.request(ArdZigbee::Operation::Pair,opts));z.loop(fakeMillis);assert(steering==3);
 signal(z,ESP_ZB_BDB_SIGNAL_STEERING,false);fakeMillis+=5;z.loop(fakeMillis);assert(steering==4);
 signal(z,ESP_ZB_BDB_SIGNAL_STEERING,false);fakeMillis+=5;z.loop(fakeMillis);assert(results==4&&!success);
 assert(z.request(ArdZigbee::Operation::Pair,opts));lockAvailable=false;fakeMillis+=100;z.loop(fakeMillis);assert(results==5&&!success);lockAvailable=true;
 assert(z.request(ArdZigbee::Operation::Pair,opts));z.loop(fakeMillis);
 assert(z.request(ArdZigbee::Operation::FactoryReset,opts));z.loop(fakeMillis);
 assert(results==6&&z.busy());signal(z,ESP_ZB_BDB_SIGNAL_STEERING,false);fakeMillis+=5;z.loop(fakeMillis);
 assert(results==7&&success&&completed==ArdZigbee::Operation::FactoryReset&&!z.busy());
 assert(!z.request(ArdZigbee::Operation::Rejoin,opts));
 assert(z.request(ArdZigbee::Operation::Pair,opts));z.loop(fakeMillis);factoryNew=false;signal(z,ESP_ZB_BDB_SIGNAL_STEERING);
 auto before=steering;assert(z.request(ArdZigbee::Operation::FactoryReset,opts));z.loop(fakeMillis);assert(z.busy()&&resets==2);factoryNew=true;signal(z,ESP_ZB_ZDO_SIGNAL_LEAVE);assert(!z.busy()&&!z.hasNetwork()&&steering==before&&completed==ArdZigbee::Operation::FactoryReset);
 const auto* line=z.nextLine(0);assert(line&&line->text.length());
 fakeUnixTime=1704067200;z.diagnostic("Clock ready");fakeUnixTime=0;z.diagnostic("Clock unavailable");
 bool utc=false,uptime=false;for(uint32_t cursor=0;const auto* record=z.nextLine(cursor);){cursor=record->id;if(record->text=="[2024-01-01T00:00:00Z] Clock ready")utc=true;if(record->text.indexOf(" ms] Clock unavailable")>=0)uptime=true;}assert(utc&&uptime);
 String longText;for(size_t i=0;i<300;++i)longText+='x';z.diagnostic(longText);
 const ArdZigbee::Line* newest=nullptr;for(uint32_t cursor=0;const auto* record=z.nextLine(cursor);){cursor=record->id;newest=record;}assert(newest&&newest->text.length()<256&&newest->text.substring(newest->text.length()-220)==longText.substring(0,220));
 assert(z.request(ArdZigbee::Operation::Pair,opts));z.loop(fakeMillis);
 fakeMillis+=100;z.loop(fakeMillis);assert(!z.busy()&&z.pairing());
 auto pairAttempts=steering;fakeMillis+=29999;z.loop(fakeMillis);assert(steering==pairAttempts);
 ++fakeMillis;z.loop(fakeMillis);assert(z.busy()&&steering==pairAttempts+1);
 fakeMillis+=100;z.loop(fakeMillis);assert(z.pairing()&&!z.busy());
 assert(z.stopPairing());fakeMillis+=30001;z.loop(fakeMillis);assert(!z.pairing()&&!z.busy()&&steering==pairAttempts+1);
 coexistResult=ESP_FAIL;queuedTask(queuedTaskArg);z.loop(fakeMillis);assert(z.state()==ArdZigbee::State::Failed&&z.error()=="Could not enable Wi-Fi/Zigbee radio coexistence");
 coexistResult=ESP_OK;queuedTask(queuedTaskArg);assert(coexistEnabled&&coexistCalls==2&&rxOnWhenIdle);
 puts("PASS: automatic boot pairing, saved-identity rejoin, reset-only pairing, stop without retries, bounded retry/timeout, factory-reset supersession and independent history");
}
