// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include "PowerEvents.h"

enum class ArdPowerPolicy : uint8_t { AlwaysOn, EventDriven, Manual, Periodic };
enum class ArdPowerSleepType : uint8_t { None, ConnectedSleep, LightSleep, DeepSleep };
enum class ArdPowerState : uint8_t { Unmanaged, Awake, ModemSleep, ConnectedSleep, ModemSleepFallback, LightSleep, DeepSleep, Error };
enum class ArdPowerBlocker : uint16_t {
  None=0, Startup=1, Storage=2, Ota=4, Restart=8, AccessPoint=16,
  Network=32, Http=64, Mqtt=128, WebSocket=256, Application=512, Zigbee=1024, WakePin=2048, Interrupt=4096, Event=8192
};

namespace ArdPowerDetail {
struct Context {
  uint16_t blockers=0;
  bool wifiActive=false, wifiConnected=false, zigbee=false, zigbeeSleeping=false, zigbeeDeepSleep=false;
  bool (*prepareDeepSleep)(void*)=nullptr;
  void (*afterDeepSleepAttempt)(void*)=nullptr;
  void (*beforeDeepSleep)(void*)=nullptr;
  void* owner=nullptr;
};

/** Fixed-capacity policy. Callbacks run in loop; only signalFromISR is an ISR API. */
template<class Backend> class Manager {
public:
  using Policy=ArdPowerPolicy;
  using SleepType=ArdPowerSleepType;
  using State=ArdPowerState;
  using Blocker=ArdPowerBlocker;
  using WakeCause=ArdPowerWakeCause;
  using WakeLevel=ArdPowerWakeLevel;
  using WakeScope=ArdPowerWakeScope;
  using Interrupt=ArdPowerInterrupt;
  using Event=ArdPowerEvent;
  using EventKind=ArdPowerEventKind;
  using WakeCallback=void (*)(const Event&,void*);
  static constexpr uint8_t MaxTimers=8;
  struct ConnectedSleepOptions {
    bool allowCpuSleep=true; // False saves radio power without enabling CPU light-sleep.
  };
  struct PeriodicOptions {
    uint32_t intervalMs=60000; // Time between scheduled application cycles.
  };
  struct Options {
    Policy policy=Policy::AlwaysOn;
    SleepType sleep=SleepType::None;
    ConnectedSleepOptions connectedSleep;
    PeriodicOptions periodic;
    uint32_t deepSleepMs=60000; // EventDriven/manual default; Periodic uses the remaining cycle.
    uint32_t sleepDelayMs=1000; // Inactivity delay before automatic sleep; explicit requests skip it.
    uint16_t maxIdleMs=20; // At most 100 ms so portal timers and sockets remain serviced.
  };
  Manager()=default;
  Manager(const Manager&)=delete;
  Manager& operator=(const Manager&)=delete;
  bool configure(const Options& options) {
    if(uint8_t(options.policy)>uint8_t(Policy::Periodic)||uint8_t(options.sleep)>uint8_t(SleepType::DeepSleep)||
       !_backend.supportsSleep(options.sleep)||options.sleepDelayMs>INT32_MAX||
       !options.maxIdleMs||options.maxIdleMs>100)return false;
    if(options.policy==Policy::Periodic&&(!options.periodic.intervalMs||
       options.periodic.intervalMs>INT32_MAX||options.sleepDelayMs>=options.periodic.intervalMs))return false;
    if(options.sleep==SleepType::DeepSleep) {
      const uint32_t ms=options.policy==Policy::Periodic?options.periodic.intervalMs:options.deepSleepMs;
      if(!ms||!Backend::validDeepSleep(ms)||!_backend.deepSourcesReady())return false;
      for(const auto& timer:_timers)if(timer.used)return false;
    }
    _options=options;_periodicAt=Backend::now()+options.periodic.intervalMs;
    _pending=false;_requestStarted=false;notifyActivity();return true;
  }
  bool setPolicy(Policy policy){Options options=_options;options.policy=policy;return configure(options);}
  bool setSleepType(SleepType type){Options options=_options;options.sleep=type;return configure(options);}
  Policy policy()const{return _options.policy;}
  SleepType sleepType()const{return _options.sleep;}
  /** Immutable cause of the current boot; available before begin()/configure(). */
  WakeCause wakeCause()const{return _backend.wakeCause();}
  bool wokeFromDeepSleep()const{
    const auto cause=wakeCause();
    return cause==WakeCause::Timer||cause==WakeCause::Gpio||cause==WakeCause::DeepSleep||cause==WakeCause::OtherWake;
  }
  State state()const{return _state;}
  uint16_t blockers()const{return _blockers;}
  bool blockedBy(Blocker blocker)const{return (_blockers&uint16_t(blocker))!=0;}
  /** Report completed work: restart sleepDelayMs and clear the previous wakeIn() limit. */
  void notifyActivity(){_lastActivity=Backend::now();_deadlineSet=false;_sleepFailed=false;}
  /** Tell the manager: "I need to run my code in ms milliseconds; do not sleep past then."
   * Does not run code or invoke a callback. Each call replaces the previous time limit.
   * After it expires, sleep is blocked until updated or cleared by notifyActivity().
   * Deep-sleep wake restarts setup(); this is not a real-time execution guarantee. */
  bool wakeIn(uint32_t ms) {
    if(ms>INT32_MAX)return false;
    _deadline=Backend::now()+ms;_deadlineSet=true;return true;
  }
  /** Install a handler executed only from portal.loop(), including timer events. */
  void onWake(WakeCallback callback,void* argument=nullptr){_callback=callback;_callbackArgument=argument;}
  bool addWakePin(uint8_t pin,WakeLevel level){return addWakePin(pin,level,_options.sleep==SleepType::DeepSleep?WakeScope::LightAndDeepSleep:WakeScope::LightSleep);}
  bool addWakePin(uint8_t pin,WakeLevel level,WakeScope scope){
    if(_options.sleep==SleepType::DeepSleep&&scope!=WakeScope::LightAndDeepSleep)return false;
    return _backend.addWakePin(pin,level,scope);
  }
  bool addInterruptPin(uint8_t pin,Interrupt trigger){return _options.sleep!=SleepType::DeepSleep&&_backend.addInterruptPin(pin,trigger);}
  bool removePin(uint8_t pin){return _backend.removePin(pin);}
  /** This is the only manager API callable from an ISR. Sources 0..15 coalesce. */
  void __attribute__((always_inline)) signalFromISR(uint8_t source){_backend.signalFromISR(source);}
  /** Register or replace a one-shot/cyclic software timer. IDs need not be consecutive. */
  bool setTimer(uint8_t id,uint32_t ms,bool repeat=false) {
    if(!ms||ms>INT32_MAX||_options.sleep==SleepType::DeepSleep)return false;
    Timer* slot=nullptr;
    for(auto& timer:_timers)if(timer.used&&timer.id==id){slot=&timer;break;}
    if(!slot)for(auto& timer:_timers)if(!timer.used){slot=&timer;break;}
    if(!slot)return false;
    *slot={Backend::now()+ms,repeat?ms:0,id,true};return true;
  }
  bool cancelTimer(uint8_t id){for(auto& timer:_timers)if(timer.used&&timer.id==id){timer.used=false;return true;}return false;}
  bool dispatching()const{return _dispatching;}
  /** Snapshot due events so callbacks may safely add/cancel timers and remove pins. */
  void dispatchEvents() {
    if(_dispatching)return;
    Event events[MaxTimers+21];uint8_t count=0;
    const uint32_t now=Backend::now(),signals=_backend.takeSignals();
    for(uint8_t bit=0;bit<24;++bit)if(signals&(uint32_t(1)<<bit)) {
      Event event{EventKind::Interrupt,0,now,0};
      if(_backend.describeSignal(bit,event))events[count++]=event;
    }
    for(auto& timer:_timers)if(timer.used&&int32_t(now-timer.due)>=0) {
      uint32_t missed=0;
      if(timer.period){missed=uint32_t(now-timer.due)/timer.period;timer.due+=uint32_t(uint64_t(missed+1)*timer.period);}
      else timer.used=false;
      events[count++]={EventKind::Timer,timer.id,now,missed};
    }
    if(_options.policy==Policy::Periodic&&int32_t(now-_periodicAt)>=0){
      const uint32_t missed=uint32_t(now-_periodicAt)/_options.periodic.intervalMs;
      _periodicAt+=uint32_t(uint64_t(missed+1)*_options.periodic.intervalMs);
      events[count++]={EventKind::Periodic,0,now,missed};
    }
    if(!count)return;
    notifyActivity();_pending=false; // A real event takes precedence over an old sleep request.
    _dispatching=true;
    for(uint8_t i=0;i<count;++i)if(_callback)_callback(events[i],_callbackArgument);
    _dispatching=false;
  }
  /** Counted application lock. Call only from the same task as portal.loop(). */
  bool lock(){if(_locks==UINT16_MAX)return false;++_locks;return true;}
  bool unlock(){if(!_locks)return false;--_locks;notifyActivity();return true;}
  /** Explicit request for the selected sleep type; AlwaysOn refuses requests. */
  bool requestSleep(uint32_t ms=0) {
    if(_options.policy==Policy::AlwaysOn||_options.sleep==SleepType::None||_pending)return false;
    if(!ms)ms=_options.sleep==SleepType::DeepSleep?
      (_options.policy==Policy::Periodic?_options.periodic.intervalMs:_options.deepSleepMs):_options.maxIdleMs;
    if(_options.sleep==SleepType::DeepSleep) {
      if(!Backend::validDeepSleep(ms)||!_backend.deepSourcesReady())return false;
    }else if(ms>INT32_MAX)return false;
    _sleepMs=ms;_pending=true;_requestStarted=false;_sleepFailed=false;return true;
  }
  void cancelSleep(){_pending=false;_requestStarted=false;_lastActivity=Backend::now();_sleepFailed=false;}
  bool sleepPending()const{return _pending;}

  // Called by ArdPortal only, after its storage/network services have run.
  void service(const Context& context) {
    const uint32_t now=Backend::now();
    const SleepType type=_options.sleep;
    if(_pending&&_requestStarted&&int32_t(_requestUntil-now)<=0){_pending=false;_requestStarted=false;}
    _blockers=context.blockers;
    if(_backend.wakePinsActive())_blockers|=uint16_t(Blocker::WakePin);
    if(_backend.interruptOnly())_blockers|=uint16_t(Blocker::Interrupt);
    if(_dispatching||_backend.hasSignals())_blockers|=uint16_t(Blocker::Event);
    if(type==SleepType::DeepSleep&&!_backend.deepSourcesReady())_blockers|=uint16_t(Blocker::WakePin);
    if(type==SleepType::LightSleep&&context.wifiActive)_blockers|=uint16_t(Blocker::Network);
    if(_locks)_blockers|=uint16_t(Blocker::Application);
    // Native Zigbee owns radio/CPU sleep. Never force sleep beneath that stack.
    if(context.zigbee&&!((context.zigbeeSleeping&&(type==SleepType::ConnectedSleep||type==SleepType::LightSleep))||
       (context.zigbeeDeepSleep&&type==SleepType::DeepSleep)))_blockers|=uint16_t(Blocker::Zigbee);
    if(_blockers)_lastActivity=now;
    const bool periodic=_options.policy==Policy::Periodic;
    const int32_t untilPeriodic=periodic?int32_t(_periodicAt-now):INT32_MAX;
    const bool automatic=_options.policy==Policy::EventDriven||periodic;
    const bool sleepAllowed=(_pending||(automatic&&uint32_t(now-_lastActivity)>=_options.sleepDelayMs))&&
      untilPeriodic>0;
    const SleepType effective=_options.policy==Policy::AlwaysOn?SleepType::None:type;
    const bool connected=((effective==SleepType::ConnectedSleep&&_options.connectedSleep.allowCpuSleep)||
      (effective==SleepType::LightSleep&&context.zigbeeSleeping))&&!_blockers&&
      (context.wifiConnected||context.zigbeeSleeping)&&(!context.wifiActive||context.wifiConnected)&&sleepAllowed;
    _state=_backend.apply(effective,context.wifiActive,connected);
    if(_state!=State::Error&&context.zigbeeSleeping&&!context.wifiActive&&effective==SleepType::ConnectedSleep&&!connected)_state=State::ModemSleep;
    if(_state==State::Error)return;
    if(_sleepFailed){_state=State::Error;return;}
    if(_options.policy==Policy::AlwaysOn||type==SleepType::None||_blockers)return;
    if(!sleepAllowed)return;

    if(type==SleepType::DeepSleep) {
      uint32_t ms=_pending?_sleepMs:(periodic?uint32_t(untilPeriodic):_options.deepSleepMs);
      if(periodic&&uint32_t(untilPeriodic)<ms)ms=uint32_t(untilPeriodic);
      if(_deadlineSet) {
        const int32_t remaining=int32_t(_deadline-now);
        if(remaining<=0)return;
        if(uint32_t(remaining)<ms)ms=uint32_t(remaining);
      }
      if(context.prepareDeepSleep&&!context.prepareDeepSleep(context.owner)){
        _blockers|=uint16_t(Blocker::Zigbee);return;
      }
      _pending=false;_requestStarted=false;_state=State::DeepSleep;
      if(context.beforeDeepSleep)context.beforeDeepSleep(context.owner);
      const bool slept=_backend.deepSleep(ms);
      if(context.afterDeepSleepAttempt)context.afterDeepSleepAttempt(context.owner);
      if(!slept){_state=State::Error;_sleepFailed=true;}
      return;
    }
    // Radio-only connected sleep never adds automatic CPU idle waits.
    if(type==SleepType::ConnectedSleep&&!_options.connectedSleep.allowCpuSleep&&!_pending)return;
    bool hasDeadline=_deadlineSet||periodic;
    for(const auto& timer:_timers)if(timer.used)hasDeadline=true;
    if(!_pending&&!hasDeadline&&!_backend.hasSources()&&!_callback)return;
    uint32_t ms=_options.maxIdleMs;
    if(periodic&&uint32_t(untilPeriodic)<ms)ms=uint32_t(untilPeriodic);
    if(_pending) {
      if(!_requestStarted){_requestUntil=now+_sleepMs;_requestStarted=true;}
      const int32_t remaining=int32_t(_requestUntil-now);
      if(remaining<=0){_pending=false;_requestStarted=false;return;}
      if(uint32_t(remaining)<ms)ms=uint32_t(remaining);
    }
    if(_deadlineSet) {
      const int32_t remaining=int32_t(_deadline-now);
      if(remaining<=0)return;
      if(uint32_t(remaining)<ms)ms=uint32_t(remaining);
    }
    for(const auto& timer:_timers)if(timer.used) {
      const int32_t remaining=int32_t(timer.due-now);
      if(remaining<=0)return;
      if(uint32_t(remaining)<ms)ms=uint32_t(remaining);
    }
    if(type==SleepType::LightSleep&&!context.zigbee) {
      _state=_backend.lightSleep(ms);
      if(_state==State::Error){_pending=false;_requestStarted=false;_sleepFailed=true;}
    }else _backend.idle(ms);
  }

private:
  struct Timer {uint32_t due=0,period=0;uint8_t id=0;bool used=false;};
  Timer _timers[MaxTimers];
  WakeCallback _callback=nullptr;
  void* _callbackArgument=nullptr;
  bool _dispatching=false;
  Backend _backend;
  Options _options;
  State _state=State::Unmanaged;
  uint32_t _lastActivity=0,_deadline=0,_sleepMs=0,_requestUntil=0,_periodicAt=0;
  uint16_t _blockers=0,_locks=0;
  bool _deadlineSet=false,_pending=false,_requestStarted=false,_sleepFailed=false;
};
}
