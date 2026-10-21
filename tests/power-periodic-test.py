"""Verify periodic cadence, sleep bounds, blockers, overruns and reboot periods."""
from pathlib import Path
import ast
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
# Reuse the deterministic clock/backend without executing the other test script.
tree=ast.parse((root/'tests/power-policy-test.py').read_text())
fixture=next(ast.literal_eval(node.value) for node in tree.body if isinstance(node,ast.Assign)
             and any(isinstance(target,ast.Name) and target.id=='fixture' for target in node.targets))
fixture=fixture[:fixture.index('int main(){')]
main=r'''
unsigned cycles=0,timers=0,lastMissed=0;
void event(const Power::Event& e,void*){
 if(e.kind==Power::EventKind::Periodic){assert(e.id==0);++cycles;lastMissed=e.missed;}
 else if(e.kind==Power::EventKind::Timer)++timers;
}
Power::Options periodic(Sleep sleep,uint32_t interval=100,uint32_t awake=10){
 auto o=options(Policy::Periodic,sleep);o.periodic.intervalMs=interval;o.sleepDelayMs=awake;return o;
}
int main(){
 static_assert(unsigned(Policy::Manual)==2,"Preserve enum values");
 Context wifi;wifi.wifiActive=true;wifi.wifiConnected=true;
 Power p;Backend::tick=0;auto o=periodic(Sleep::ConnectedSleep);
 assert(p.configure(o));p.onWake(event);
 Backend::tick=9;p.service(wifi);assert(!Backend::idled&&p.state()!=State::ConnectedSleep);
 Backend::tick=10;p.service(wifi);assert(Backend::idled==20);
 Backend::tick=95;p.service(wifi);assert(Backend::idled==5&&Backend::tick==100);
 Backend::idled=0;p.service(wifi);assert(!Backend::idled); // Dispatch due work first.
 p.dispatchEvents();assert(cycles==1&&!lastMissed);
 Backend::tick=109;p.service(wifi);assert(!Backend::idled);
 Backend::tick=110;p.service(wifi);assert(Backend::idled==20);
 Backend::tick=190;wifi.blockers=uint16_t(Blocker::Storage);p.service(wifi);
 Backend::tick=200;p.dispatchEvents();assert(cycles==2);p.service(wifi);
 wifi.blockers=0;Backend::tick=210;p.service(wifi);assert(Backend::idled==20);
 Backend::tick=350;p.dispatchEvents();assert(cycles==3&&!lastMissed);
 Backend::tick=650;p.dispatchEvents();assert(cycles==4&&lastMissed==2); // Next is 700, not 750.
 Backend::tick=695;p.notifyActivity();assert(p.requestSleep(1000));p.service(wifi);
 assert(Backend::idled==5&&Backend::tick==700);
 p.dispatchEvents();assert(cycles==5&&!p.sleepPending());
 // None still delivers cycles; it does not sleep or accept explicit requests.
 Backend::tick=0;assert(p.configure(periodic(Sleep::None)));assert(!p.requestSleep());
 Backend::tick=100;p.dispatchEvents();assert(cycles==6);Backend::idled=0;p.service(wifi);assert(!Backend::idled);
 // Changing policy disables the cadence; selecting Periodic starts a new schedule.
 assert(p.setPolicy(Policy::Manual));Backend::tick=1000;p.dispatchEvents();assert(cycles==6);
 assert(p.setPolicy(Policy::Periodic));Backend::tick=1099;p.dispatchEvents();assert(cycles==6);
 Backend::tick=1100;p.dispatchEvents();assert(cycles==7);
 // Existing software timers are independent and can wake earlier.
 Backend::tick=0;assert(p.configure(periodic(Sleep::ConnectedSleep)));assert(p.setTimer(7,40));
 Backend::tick=30;p.service(wifi);assert(Backend::idled==10);
 p.dispatchEvents();assert(timers==1);Backend::tick=100;p.dispatchEvents();assert(cycles==8);
 // Native Zigbee light sleep yields through the scheduler, never forced SDK sleep.
 Context zigbee;zigbee.zigbee=true;zigbee.zigbeeSleeping=true;
 Backend::tick=0;assert(p.configure(periodic(Sleep::LightSleep)));
 Backend::tick=95;unsigned light=Backend::lightCalls;p.service(zigbee);
 assert(Backend::idled==5&&Backend::lightCalls==light);
 // Standalone light sleep uses the same deadline; active Wi-Fi still blocks it.
 Backend::tick=0;assert(p.configure(periodic(Sleep::LightSleep)));
 Backend::tick=95;p.service(wifi);assert(p.blockedBy(Blocker::Network));
 Context offline;Backend::tick=110;p.dispatchEvents();Backend::tick=120;p.service(offline);
 assert(Backend::lightCalls==light+1);
 // DeepSleep duration is time remaining to the cycle, not deepSleepMs.
 Backend::tick=0;auto deep=periodic(Sleep::DeepSleep,1000,100);deep.deepSleepMs=0;
 assert(p.configure(deep));Backend::tick=100;p.service(offline);assert(Backend::slept==900);
 Backend::tick=0;assert(p.configure(deep));assert(p.wakeIn(300));Backend::tick=100;
 p.service(offline);assert(Backend::slept==200);
 Backend::tick=0;assert(p.configure(deep));Backend::tick=100;assert(p.requestSleep(5000));
 p.service(offline);assert(Backend::slept==900);
 Backend::tick=0;assert(p.configure(deep));offline.blockers=uint16_t(Blocker::Application);
 Backend::tick=500;p.service(offline);offline.blockers=0;Backend::tick=600;p.service(offline);
 assert(Backend::slept==400); // Busy work shortens sleep; it does not shift cadence.
 // Wrapping millis and missed deadlines preserve phase.
 Backend::tick=UINT32_MAX-50;assert(p.configure(periodic(Sleep::ConnectedSleep)));
 Backend::tick=48;p.service(wifi);assert(Backend::idled==1&&Backend::tick==49);
 p.dispatchEvents();assert(lastMissed==0);
 Backend::tick=249;p.dispatchEvents();assert(lastMissed==1);
 // Validate only the active periodic configuration and the applicable deep duration.
 auto invalid=periodic(Sleep::ConnectedSleep);invalid.periodic.intervalMs=0;assert(!p.configure(invalid));
 invalid.periodic.intervalMs=uint32_t(INT32_MAX)+1;assert(!p.configure(invalid));
 invalid.periodic.intervalMs=10;assert(!p.configure(invalid));
 invalid.periodic.intervalMs=11;assert(p.configure(invalid));
 invalid=periodic(Sleep::DeepSleep,100001,100);assert(!p.configure(invalid));
 invalid=options(Policy::Manual,Sleep::None);invalid.periodic.intervalMs=0;assert(p.configure(invalid));
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-periodic-') as directory:
 d=Path(directory);cpp=d/'test.cpp';cpp.write_text(fixture+main);exe=d/'test'
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                 '-I'+str(root/'src'),str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: periodic cadence, callbacks, coalescing, blockers, independent timers, native/standalone light sleep, deep-sleep bounds, rollover and validation')
