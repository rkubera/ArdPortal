#include "DeviceName.h"
// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#include "ArdAppControls.h"
#include "ArdPortalDeclarations.h"
#include "Language.h"
using V = ArdJSON::JSONVar;

#if ARDPORTAL_ENABLE_DEPENDENCIES
bool ArdAppControls::dependencyValueMatches(const V& rule) const {
  if(rule.isUndefined()) return true;
  V value=_portal.getAppConfigValue(rule["field"].asString().c_str());
  if(rule.hasOwnProperty("property")) {V selected=value[rule["property"].asString()];value=std::move(selected);}
  if(value.isUndefined()||!value.isValid())return false;
  String actual=_portal._json.stringify(value),expected=_portal._json.stringify(rule["equals"]);return actual.length()&&expected.length()&&actual==expected;
}

// Walk the graph without recursive calls or retaining parsed ancestor pages.
bool ArdAppControls::dependencyFieldVisible(size_t index) const {
  if(!_portal._dynamic.dependentAt(index))return true;
  struct Frame {uint8_t index,edge;};
  Frame stack[48];size_t depth=1;stack[0]={uint8_t(index),0};
  uint64_t active=uint64_t(1)<<index,complete=0;
  while(depth) {
    Frame& frame=stack[depth-1];
    if(frame.edge==2) {uint64_t bit=uint64_t(1)<<frame.index;active&=~bit;complete|=bit;--depth;continue;}
    size_t next=_portal._dynamic.count();
    {
      V field=_portal._dynamic.at(frame.index);
      if(field.isUndefined()||!field.isValid())return false;
      const V& rule=field[frame.edge++?"visibleWhen":"_pageVisibleWhen"];
      if(!rule.isUndefined()) {
        if(!dependencyValueMatches(rule))return false;
        const String id=rule["field"].asString();
        for(size_t i=0;i<_portal._dynamic.count();++i)if(_portal._dynamic.idAt(i)==id){next=i;break;}
        if(next==_portal._dynamic.count())return false;
      }
    }
    yield();
    if(next==_portal._dynamic.count())continue;
    const uint64_t bit=uint64_t(1)<<next;
    if(active&bit)return false; // Cycles are hidden, even if their values match.
    if((complete&bit)||!_portal._dynamic.dependentAt(next))continue;
    if(depth>=48)return false;
    active|=bit;stack[depth++]={uint8_t(next),0};
  }
  return true;
}
bool ArdAppControls::dependencyMatches(const V& rule) const {
  if(rule.isUndefined())return true;
  if(!dependencyValueMatches(rule))return false;
  const String id=rule["field"].asString();
  for(size_t i=0;i<_portal._dynamic.count();++i)if(_portal._dynamic.idAt(i)==id)return dependencyFieldVisible(i);
  return false;
}

bool ArdAppControls::appFieldVisible(const V& field) const {return dependencyMatches(field["_pageVisibleWhen"])&&dependencyMatches(field["visibleWhen"]);}
#else
bool ArdAppControls::appFieldVisible(const V&) const {return true;}
#endif

#if ARDPORTAL_ENABLE_MQTT
void ArdAppControls::dynamicMqtt(const String& topic,const uint8_t* data,size_t size,bool retained) {
  String commandPrefix="cmnd/"+ArdDeviceName::mqtt(_portal._config.deviceName)+"/",getPrefix="get/"+ArdDeviceName::mqtt(_portal._config.deviceName)+"/";
  if(topic.indexOf(getPrefix)==0) { String id=topic.substring(getPrefix.length()); for(size_t i=0;i<_portal._dynamic.count();++i) if(_portal._dynamic.idAt(i)==id) _stateDirty|=uint64_t(1)<<i; return; }
  if(topic.indexOf(commandPrefix)!=0||retained||size>ArdMqtt::PacketCapacity-5) return;
  String command=topic.substring(commandPrefix.length()),payload; for(size_t i=0;i<size;++i) { if(!data[i]) return; payload+=char(data[i]); }
  for(size_t i=0;i<_portal._dynamic.count();++i) {
    String id=_portal._dynamic.idAt(i);if(command!=id&&command.indexOf(id+"_")!=0)continue;
    const V& f=_portal._dynamic.at(i);String type=f["type"].asString();V value;
    if(false) {}
#if ARDPORTAL_CONTROL_SUPPORT_EXTENDED
    else if(ArdHa::extended(f)) {
      if(command!=id&&command.indexOf(id+"_")!=0) continue;
      V resolved=f;if(!ArdHa::normalize(resolved)) return;const V& entity=resolved;
      for(size_t c=0;c<entity["controls"].length();++c) {
        const V& control=entity["controls"][c]; String suffix=control["command"].asString();
        if(command!=id+(suffix.length()?"_"+suffix:String())) continue;
        if(!appFieldVisible(f)){_portal.log(ArdUILanguage::text(ArdUILanguage::Key::s_207));return;}
        V input=payload;
        String kind=control["type"].asString();
        if(kind=="action"&&payload!=control["payload"].asString()) continue;
        if(kind=="slider"||kind=="action_json"||kind=="color"||entity["json_command"].asBool()) input=_portal._json.parse(payload,nullptr,[](){ArdJSON::Limits limits;limits.maxNodes=512;return limits;}());
        if(kind=="switch"&&!control.hasOwnProperty("on")) {if(payload!="ON"&&payload!="OFF") return;input=payload=="ON";}
        if(entity["json_command"].asBool()) {
          if(input.type()!=V::Type::Object) return;
          V next=_portal.getAppConfigValue(id.c_str());bool changed=false;
          for(size_t j=0;j<entity["controls"].length();++j) {const V& part=entity["controls"][j];String key=part["key"].asString();if(input.hasOwnProperty(key)) {if(!ArdHa::validControl(part,input[key])) return;next[key]=input[key];changed=true;}}
          if(!changed||!ArdDynamicPages::validValue(entity,next)) return;
          if(!entity["persist"].asBool()) {if(applyAppState(id,next,ChangeSource::Mqtt)) _stateDirty|=uint64_t(1)<<i;}
          else {acceptMqttState(id,next);}
          return;
        }
        if(!appControl(f,c,input,ChangeSource::Mqtt)) _portal.log(ArdUILanguage::text(ArdUILanguage::Key::s_177));
        return;
      }
      continue;
    }
#endif
#if ARDPORTAL_ENABLE_CONTROL_CLIMATE
    else if(type=="climate") {
      value=_mqttAppQueue.hasOwnProperty(id)?_mqttAppQueue[id]:((_portal._savePending||_portal._saveQueued||_portal._pendingReady)&&_portal._pendingApp.hasOwnProperty(id)?_portal._pendingApp[id]:_portal.getAppConfigValue(id.c_str()));
      if(command==id+"_temperature") value["temperature"]=_portal._json.parse(payload);
      else if(command==id+"_mode"&&f.hasOwnProperty("modes")) value["mode"]=payload;
      else if(command==id+"_fan"&&f.hasOwnProperty("fan_modes")) value["fan_mode"]=payload;
      else continue;
    }
#endif
    else {
      if(command!=id) continue;
#if ARDPORTAL_ENABLE_CONTROL_TEXT
      if(type=="text") continue;
#endif
      if(false) {}
#if ARDPORTAL_ENABLE_CONTROL_SLIDER
      else if(type=="slider") value=_portal._json.parse(payload);
#endif
#if ARDPORTAL_ENABLE_CONTROL_SWITCH
      else if(type=="switch") {if(payload!="ON"&&payload!="OFF")return;value=payload=="ON";}
#endif
#if ARDPORTAL_ENABLE_CONTROL_EDIT || ARDPORTAL_ENABLE_CONTROL_SELECT
      else value=payload;
#endif
    }
    if(!appFieldVisible(f)){_portal.log(ArdUILanguage::text(ArdUILanguage::Key::s_207));return;}
    if(ArdDynamicPages::validValue(f,value)) { acceptMqttState(id,value); }
    else _portal.log(ArdUILanguage::text(ArdUILanguage::Key::s_177));
    return;
  }
}
#endif

#if ARDPORTAL_ENABLE_MQTT
uint64_t ArdAppControls::stateMask() const {
  uint64_t mask=0;
  for(size_t i=0;i<_portal._dynamic.count();++i) if(!_portal._dynamic.transientAt(i)) mask|=uint64_t(1)<<i;
  return mask;
}
#endif

#if ARDPORTAL_ENABLE_MQTT
bool ArdAppControls::queueAppEmission(const String& topic,const String& payload) {
#if ARDPORTAL_CONTROL_SUPPORT_EMISSIONS
  if(!_portal.mqttConnected()||_emissionCount==8||topic.length()+payload.length()+2>ArdMqtt::PacketCapacity-5) return false;
  AppEmission& e=_appEmissions[(_emissionHead+_emissionCount)%8];e.topic=topic;e.payload=payload;++_emissionCount;return true;
#else
  (void)topic;(void)payload;return false;
#endif
}
#endif

bool ArdAppControls::applyAppState(const String& key,const V& value,ChangeSource source,bool publishMqtt) {
  const V& f=_portal._dynamic.field(key);if(f.isUndefined()||ArdHa::transient(f)||!ArdDynamicPages::validValue(f,value)) return false;
  V previous=_portal.getAppConfigValue(key.c_str());_appState[key]=value;if(!_appState.isValid()) return false;
  bool changed=ArdJSON::JSON.stringify(previous)!=ArdJSON::JSON.stringify(value);
  if(changed) {++_portal._appRevision;if(publishMqtt)markDirty(key);if(_portal._appChanged) _portal._appChanged(key,value,source);}
  return true;
}

bool ArdAppControls::setAppStateValue(const char* key,const V& value,bool publishMqtt) {return key&&applyAppState(key,value,ChangeSource::Application,publishMqtt);}

bool ArdAppControls::emitAppEvent(const char* key,const V& value) {
#if ARDPORTAL_CONTROL_SUPPORT_EVENTS
  if(!key) return false;
  const V& f=_portal._dynamic.field(key);if((f["type"].asString()!="event"&&f["type"].asString()!="device_trigger"&&f["type"].asString()!="tag")||!ArdDynamicPages::validValue(f,value)) return false;
#if ARDPORTAL_ENABLE_MQTT
  String payload=value.type()==V::Type::String?value.asString():_portal._json.stringify(value);
  if(!queueAppEmission(_portal._mqttClient.mqttTopic("stat",key),payload)) return false;
#endif
  _appState[key]=value;++_portal._appRevision;return true;
#else
  (void)key;(void)value;return false;
#endif
}

bool ArdAppControls::appControl(const V& definition,size_t index,const V& value,ChangeSource source) {
#if ARDPORTAL_CONTROL_SUPPORT_EXTENDED
  _appControlStage=1;if(!appFieldVisible(definition)) {_appControlStage=6;return false;}V resolved=definition;if(!ArdHa::normalize(resolved,true)) return false;const V& f=resolved;
  _appControlStage=2;if(!ArdHa::extended(f)||index>=f["controls"].length()) return false;
  const V& c=f["controls"][index];String kind=c["type"].asString(),key=f["id"].asString(),suffix=c["command"].asString();
#if ARDPORTAL_CONTROL_SUPPORT_ACTIONS
  if(kind.indexOf("action")==0) {
    if(!_portal._appCommand) return false;
    if(kind=="action" && _portal._json.stringify(value)!=_portal._json.stringify(c["payload"])) return false;
    if(kind=="action_text"&&!ArdHa::string(value)) return false;
    if(kind=="action_json"&&(!value.isValid()||value.isUndefined()||_portal._json.stringify(value,false,nullptr,[](){ArdJSON::Limits limits;limits.maxNodes=512;return limits;}()).length()>1500)) return false;
    if(!_portal._appCommand(key,suffix,value,source)) return false;
#if ARDPORTAL_ENABLE_MQTT
    if(_portal.mqttConnected()) {
      V ack=V::object();ack["command"]=suffix;ack["value"]=value;ack["accepted"]=true;
      // The callback accepts an action; it must report actual hardware state separately.
      if(!ArdHa::transient(f)) {V state=_portal.getAppConfigValue(key.c_str());String payload=state.type()==V::Type::String?state.asString():_portal._json.stringify(state);queueAppEmission(_portal._mqttClient.mqttTopic("stat",key.c_str()),payload);}
      queueAppEmission(_portal._mqttClient.mqttTopic("stat",(key+(ArdHa::transient(f)?String():String("_ack"))).c_str()),_portal._json.stringify(ack));
    }
#endif
    // Acceptance depends on the local callback, even if MQTT delivery is unavailable.
    return true;
  }
#endif
  _appControlStage=3;if(!ArdHa::validControl(c,value)) return false;
  // Reuse the resolved schema instead of normalizing it again in getAppConfigValue.
  const V& states=_appState;const V& saved=_portal._appConfig;
  V next=states.hasOwnProperty(key)?states[key]:saved[key];
  if(!ArdHa::validValue(f,next)) next=f["default"];
  String property=c["key"].asString();if(property.length()) next[property]=value;else next=value;
  _appControlStage=4;if(!ArdHa::validValue(f,next)) return false;
  if(!f["persist"].asBool()) {if(!applyAppState(key,next,source)) return false;markDirty(key);return true;}
  _appControlStage=5;V app=_portal._savePending?_portal._pendingApp:_portal._appConfig;app[key]=next;
#if ARDPORTAL_ENABLE_MQTT
  if(source==ChangeSource::Mqtt) return acceptMqttState(key,next);
#endif
  if(_portal._saveQueued||_portal._pendingReady||_portal._storage.busy()||_portal.mqttTrialActive()) return false;
  if(_portal._savePending) {if(_portal._saveSource!=source) return false;_portal._pendingApp=app;_portal._dirtySince=millis();}
  else if(!_portal.scheduleConfig(_portal._config,app,source,false)) return false;
  return applyAppState(key,next,source);
#else
  (void)definition;(void)index;(void)value;(void)source;return false;
#endif
}

#if ARDPORTAL_ENABLE_MQTT
void ArdAppControls::disconnected() {
#if ARDPORTAL_CONTROL_SUPPORT_EMISSIONS
  for (auto& emission : _appEmissions) emission = AppEmission();
  _emissionCount = _emissionHead = 0;
#endif
}
#endif

#if ARDPORTAL_ENABLE_MQTT
void ArdAppControls::yieldAfterPublish() {
  if (_yieldPending) { _yieldPending=false; yield(); }
}
#endif

void ArdAppControls::applyAppConfig(V app) {
  ChangeSource source=_portal._saveSource; V previous=std::move(_portal._appConfig); _portal._appConfig=std::move(app); ++_portal._appRevision;
  V keys=previous.keys(), next=_portal._appConfig.keys();
  for(size_t i=0;i<next.length();++i) if(!previous.hasOwnProperty(next[i].asString())) keys.push(next[i]);
  for(size_t i=0;i<keys.length();++i) {
    String key=keys[i].asString(); V old=_appState.hasOwnProperty(key)?static_cast<const V&>(_appState)[key]:static_cast<const V&>(previous)[key];
    if(_appState.hasOwnProperty(key)&&_portal._json.stringify(_appState[key])==_portal._json.stringify(static_cast<const V&>(_portal._appConfig)[key])) _appState.remove(key);
    V value=_portal.getAppConfigValue(key.c_str());
    if(ArdJSON::JSON.stringify(old)==ArdJSON::JSON.stringify(value)) continue;
    markDirty(key);
    // MQTT intent was already dispatched on receipt. A flash commit is not a new command.
    if(source!=ChangeSource::Mqtt && _portal._appChanged) { V copy=value; _portal._appChanged(key,copy,source); }
  }
}

#if ARDPORTAL_ENABLE_MQTT
void ArdAppControls::acknowledgeSave(bool saved) {
  if (saved) _stateDirty |= _mqttAckInFlight;
  _mqttAckInFlight = 0;
}
#endif

void ArdAppControls::markDirty(const String& key) {
#if ARDPORTAL_ENABLE_MQTT
  for(size_t i=0;i<_portal._dynamic.count();++i) if(_portal._dynamic.idAt(i)==key) _stateDirty|=uint64_t(1)<<i;
#else
  (void)key;
#endif
}
#if ARDPORTAL_ENABLE_MQTT
bool ArdAppControls::canServiceMqtt() const {
  return _portal._dynamic.count() && _portal.mqttConnected() && !_portal.mqttTrialActive() && !_portal.otaActive() && !_portal._rebootPending;
}
void ArdAppControls::resetMqtt() {
  _subscriptions=0; _stateDirty=stateMask(); _stateSince=millis();
}
bool ArdAppControls::acceptMqttState(const String& key,V value) {
  // Own the command value: application callbacks can replace the reported state.
  if(!applyAppState(key,value,ChangeSource::Mqtt)) return false;
  markDirty(key);
  _mqttAppQueue[key]=value;
  if(!_mqttAppQueue.isValid()) return false;
  for(size_t i=0;i<_portal._dynamic.count();++i)
    if(_portal._dynamic.idAt(i)==key) _mqttAckPending|=uint64_t(1)<<i;
  return true;
}
void ArdAppControls::prepareMqtt(uint32_t now) {
  // Only persistence waits for flash. Accepted commands have already run.
  if(_mqttAppQueue.length()&&!_portal.storageBusy()&&!_portal._httpWaitingStorage) {
    V app=_portal._appConfig,keys=_mqttAppQueue.keys();
    for(size_t i=0;i<keys.length();++i) {
      String key=keys[i].asString(); app[key]=_mqttAppQueue[key];
    }
    if(_portal.scheduleConfig(_portal._config,app,ChangeSource::Mqtt,false)) {
      _mqttAckInFlight=_mqttAckPending; _mqttAckPending=0;
      _mqttAppQueue=V::object();
    }
  }
  if(_portal._options.appStateIntervalMs && uint32_t(now-_stateSince)>=_portal._options.appStateIntervalMs) {
    _stateDirty=stateMask(); _stateSince=now;
  }
}
bool ArdAppControls::publishCommands() {
#if ARDPORTAL_CONTROL_SUPPORT_EMISSIONS
  if(_emissionCount) { const AppEmission& e=_appEmissions[_emissionHead]; if(_portal._mqttClient.publishRaw(e.topic.c_str(),e.payload.c_str(),false)) { _appEmissions[_emissionHead]=AppEmission();_emissionHead=(_emissionHead+1)%8;--_emissionCount;_yieldPending=true; } return true; }
#endif
  if(_subscriptions<(ARDPORTAL_ENABLE_HA?3:2)) { String filter;
#if ARDPORTAL_ENABLE_HA
    if(_subscriptions==2) filter="homeassistant/status"; else {
#endif
    filter=_portal._mqttClient.mqttTopic(_subscriptions==0?"cmnd":"get","+");
#if ARDPORTAL_ENABLE_HA
    }
#endif
    if(_portal._mqttClient.subscribeRaw(filter.c_str())) ++_subscriptions;
    return true;
  }
  return false;
}
bool ArdAppControls::publishState() {
  for(size_t f=0;f<_portal._dynamic.count();++f) if(_stateDirty&(uint64_t(1)<<f)) {
    const V& field=_portal._dynamic.at(f); String id=field["id"].asString(),topic=_portal._mqttClient.mqttTopic("stat",id.c_str()); V value=_portal.getAppConfigValue(id.c_str());
    String payload;
#if ARDPORTAL_ENABLE_CONTROL_SWITCH || ARDPORTAL_ENABLE_CONTROL_BINARY_SENSOR
    if(
#if ARDPORTAL_ENABLE_CONTROL_SWITCH
       field["type"].asString()=="switch" ||
#endif
#if ARDPORTAL_ENABLE_CONTROL_BINARY_SENSOR
       field["type"].asString()=="binary_sensor" ||
#endif
       false) payload=value.asBool()?"ON":"OFF";
    else
#endif
      payload=value.type()==V::Type::String?value.asString():_portal._json.stringify(value);
    if(_portal._mqttClient.publishRaw(topic.c_str(),payload.c_str(),true)) { _stateDirty&=~(uint64_t(1)<<f); _yieldPending=true; } return true;
  }
  return false;
}
void ArdAppControls::serviceMqttValues(uint32_t now) {
  if(!canServiceMqtt()) return;
  prepareMqtt(now);
  if(_portal._mqttClient.sendBusy() || publishCommands()) return;
  publishState();
}
#endif
