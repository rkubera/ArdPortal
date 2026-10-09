#include "DeviceName.h"
// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#include "ArdAppControls.h"
#include "ArdPortalDeclarations.h"
#include "Language.h"
using V = ArdJSON::JSONVar;

#if ARDPORTAL_ENABLE_DEPENDENCIES
/**
 * @brief Compare the referenced application value or property with the dependency expectation.
 * @param rule Dependency rule to evaluate.
 * @return True when the selected value matches the rule; false otherwise.
 */
bool ArdAppControls::dependencyValueMatches(const V& rule) const {
  auto match=[&](const V& leaf) {
  V value=_portal.getAppConfigValue(leaf["field"].asString().c_str());
  if(leaf.hasOwnProperty("property")) {V selected=value[leaf["property"].asString()];value=std::move(selected);}
  if(value.isUndefined()||!value.isValid())return false;
  String actual=_portal._json.stringify(value,false,nullptr,snapshotLimits()),expected=_portal._json.stringify(leaf["equals"]);return actual.length()&&expected.length()&&actual==expected;
  };
  return ArdDependencies::evaluate(rule,match);
}

// Walk the graph without recursive calls or retaining parsed ancestor pages.
/**
 * @brief Evaluate field visibility through the indexed dependency chain.
 * @param index Zero-based element or field index.
 * @return True when the complete dependency chain permits visibility; false otherwise.
 */
bool ArdAppControls::dependencyFieldVisible(size_t index) const {
  if(index>=_portal._dynamic.count())return false;
  if(!_portal._dynamic.dependentAt(index))return true;
  const size_t count=_portal._dynamic.count();
  ArdDependencies::Stack stack;
  if(!stack.reserve(1,count))return false;
  size_t depth=1;stack[0]={uint16_t(index),0,0};
  ArdAppConfigFieldMask active=ArdAppConfigFieldMask::forField(index),complete=0,visible=0;
  auto load=[&](size_t i){return _portal._dynamic.at(i);};
  auto indexOf=[&](const String& id){return _portal._dynamic.indexOf(id);};
  auto dependent=[&](size_t i){return _portal._dynamic.dependentAt(i);};
  auto match=[&](const V& leaf){return dependencyValueMatches(leaf);};
  while(depth){
    auto result=ArdDependencies::step(stack,depth,active,complete,visible,count,load,indexOf,dependent,match);
    if(result!=ArdDependencies::Result::Running)return result==ArdDependencies::Result::Visible;
    yield();
  }
  return false;
}
/**
 * @brief Evaluate a visibility dependency against the current application values.
 * @param rule Dependency rule to evaluate.
 * @return True when the referenced field is visible and its value matches the rule.
 */
bool ArdAppControls::dependencyMatches(const V& rule) const {
  auto match=[&](const V& leaf) {
    if(!dependencyValueMatches(leaf))return false;
    size_t index=_portal._dynamic.indexOf(leaf["field"].asString());
    return index<_portal._dynamic.count()&&dependencyFieldVisible(index);
  };
  return ArdDependencies::evaluate(rule,match);
}

/**
 * @brief Evaluate the field visibility condition against current application values.
 * @param field Application field definition or identifier.
 * @return True when the field is visible; false when its dependency condition is not satisfied.
 */
bool ArdAppControls::appFieldVisible(const V& field) const {return dependencyMatches(field["_pageVisibleWhen"])&&dependencyMatches(field["visibleWhen"]);}
#else
/**
 * @brief Evaluate the field visibility condition against current application values.
 * Input: const V&.
 * @return True when the field is visible; false when its dependency condition is not satisfied.
 */
bool ArdAppControls::appFieldVisible(const V&) const {return true;}
#endif

#if ARDPORTAL_ENABLE_MQTT
/**
 * @brief Route an incoming MQTT application command.
 * @param topic MQTT topic to publish, subscribe or match.
 * @param data Data buffer or value used by the operation.
 * @param size Number of bytes or elements.
 * @param retained Whether the MQTT publication is retained.
 * @return No value.
 */
void ArdAppControls::dynamicMqtt(const String& topic,const uint8_t* data,size_t size,bool retained) {
  String commandPrefix="cmnd/"+ArdDeviceName::mqtt(_portal._config.deviceName)+"/",getPrefix="get/"+ArdDeviceName::mqtt(_portal._config.deviceName)+"/";
  if(topic.indexOf(getPrefix)==0) { String id=topic.substring(getPrefix.length()); for(size_t i=0;i<_portal._dynamic.count();++i) if(_portal._dynamic.idAt(i)==id) _stateDirty|=ArdAppConfigFieldMask::forField(i); return; }
  if(topic.indexOf(commandPrefix)!=0||retained||size>ArdMqtt::PacketCapacity-5) return;
  String command=topic.substring(commandPrefix.length()),payload; for(size_t i=0;i<size;++i) { if(!data[i]) return; payload+=char(data[i]); }
  size_t exact=_portal._dynamic.indexOf(command);
  for(size_t i=exact<_portal._dynamic.count()?exact:0;i<_portal._dynamic.count();++i) {
    if(exact<_portal._dynamic.count()&&i!=exact)break;
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
          if(!entity["persist"].asBool()) {if(applyAppState(id,next,ChangeSource::Mqtt)) _stateDirty|=ArdAppConfigFieldMask::forField(i);}
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
/**
 * @brief Expose the field mask used for pending state publication.
 * @return The field mask used for pending state publication.
 */
ArdAppConfigFieldMask ArdAppControls::stateMask() const {
  ArdAppConfigFieldMask mask=0;
  const auto& definitions=_portal._dynamic;
  for(size_t p=0;p<definitions.pages.length();++p)for(size_t f=0;f<definitions.pages.fields(p);++f)if(!definitions.pages.transient(p,f))mask|=ArdAppConfigFieldMask::forField(definitions.pages.fieldIndex(p,f));
  for(const auto* entity=definitions.entities.get();entity;entity=entity->next.get())if(!entity->transient)mask|=ArdAppConfigFieldMask::forField(entity->index);
  return mask;
}
#endif

#if ARDPORTAL_ENABLE_MQTT
/**
 * @brief Queue a transient application MQTT message in the bounded emission buffer.
 * @param topic MQTT topic to publish, subscribe or match.
 * @param payload Message bytes or text to send or decode.
 * @return True if queued; false when MQTT is unavailable or the bounded queue cannot accept it.
 */
bool ArdAppControls::queueAppEmission(const String& topic,const String& payload) {
#if ARDPORTAL_CONTROL_SUPPORT_EMISSIONS
  if(!_portal.mqttConnected()||_emissionCount==8||topic.length()+payload.length()+2>ArdMqtt::PacketCapacity-5) return false;
  AppEmission& e=_appEmissions[(_emissionHead+_emissionCount)%8];e.topic=topic;e.payload=payload;++_emissionCount;return true;
#else
  (void)topic;(void)payload;return false;
#endif
}
#endif

/**
 * @brief Validate and apply an incoming runtime application state change.
 * @param key Configuration key or JSON object member name.
 * @param value Input value, or output destination when passed by mutable reference.
 * @param source Input source or origin of a configuration change, as indicated by its type.
 * @param publishMqtt Whether to queue MQTT state publication for this update.
 * @return True on success; false if validation, resource allocation or the operation fails.
 */
bool ArdAppControls::applyAppState(const String& key,const V& value,ChangeSource source,bool publishMqtt) {
  const V& f=_portal._dynamic.field(key);if(f.isUndefined()||ArdHa::transient(f)||!ArdDynamicPages::validValue(f,value)) return false;
  V previous=_portal.getAppConfigValue(key.c_str());
  String before=ArdJSON::JSON.stringify(previous,false,nullptr,snapshotLimits());
  String after=ArdJSON::JSON.stringify(value,false,nullptr,snapshotLimits());
  // An unchanged value can come from the persisted config or field default.
  // Do not materialize a second runtime member just to repeat that value.
  // Failed serialization must never be interpreted as equality.
  if(!after.length()||(!previous.isUndefined()&&!before.length()))return false;
  if(!previous.isUndefined()&&before==after)return true;
  _appState[key]=value;if(!_appState.isValid()) return false;
  {++_portal._appRevision;
#if ARDPORTAL_ENABLE_HA && ARDPORTAL_ENABLE_DEPENDENCIES
    _portal._homeAssistant.dependencyChanged(key);
#endif
    if(publishMqtt)markDirty(key);if(_portal._appChanged) _portal._appChanged(key,value,source);}
  return true;
}

/**
 * @brief Update a runtime application value and optionally queue its MQTT state.
 * @param key Configuration key or JSON object member name.
 * @param value Input value, or output destination when passed by mutable reference.
 * @param publishMqtt Whether to queue MQTT state publication for this update.
 * @return True on success; false if validation, resource allocation or the operation fails.
 */
bool ArdAppControls::setAppConfigStateValue(const char* key,const V& value,bool publishMqtt) {return key&&applyAppState(key,value,ChangeSource::Application,publishMqtt);}

/**
 * @brief Emit a transient application event through the field MQTT topic.
 * @param key Configuration key or JSON object member name.
 * @param value Input value, or output destination when passed by mutable reference.
 * @return True on success; false if validation, resource allocation or the operation fails.
 */
bool ArdAppControls::emitAppConfigEvent(const char* key,const V& value) {
#if ARDPORTAL_CONTROL_SUPPORT_EVENTS
  if(!key) return false;
  const V& f=_portal._dynamic.field(key);if((f["type"].asString()!="event"&&f["type"].asString()!="device_trigger"&&f["type"].asString()!="tag")||!ArdDynamicPages::validValue(f,value)) return false;
#if ARDPORTAL_ENABLE_MQTT
  String payload=value.type()==V::Type::String?value.asString():_portal._json.stringify(value,false,nullptr,snapshotLimits());
  if(!queueAppEmission(_portal._mqttClient.mqttTopic("stat",key),payload)) return false;
#endif
  _appState[key]=value;++_portal._appRevision;
#if ARDPORTAL_ENABLE_HA && ARDPORTAL_ENABLE_DEPENDENCIES
  _portal._homeAssistant.dependencyChanged(key);
#endif
  return true;
#else
  (void)key;(void)value;return false;
#endif
}

/**
 * @brief Validate and dispatch an application control action to the registered command handler.
 * @param definition JSON definition to register; flash sources must remain valid for the portal lifetime.
 * @param index Zero-based element or field index.
 * @param value Input value, or output destination when passed by mutable reference.
 * @param source Input source or origin of a configuration change, as indicated by its type.
 * @return True if the action was accepted; false for an invalid, hidden or unsupported action.
 */
bool ArdAppControls::appControl(const V& definition,size_t index,const V& value,ChangeSource source) {
#if ARDPORTAL_CONTROL_SUPPORT_EXTENDED
  _appControlStage=1;if(!appFieldVisible(definition)) {_appControlStage=6;return false;}V resolved=definition;if(!ArdHa::normalize(resolved,true)) return false;const V& f=resolved;
  _appControlStage=2;if(!ArdHa::extended(f)||index>=f["controls"].length()) return false;
  const V& c=f["controls"][index];String kind=c["type"].asString(),key=f["id"].asString(),suffix=c["command"].asString();
#if ARDPORTAL_CONTROL_SUPPORT_ACTIONS
  if(kind.indexOf("action")==0) {
    if(!_portal._appCommand) return false;
    if(kind=="action" && _portal._json.stringify(value,false,nullptr,snapshotLimits())!=_portal._json.stringify(c["payload"])) return false;
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
  _appControlStage=5;V patch=V::object();patch[key]=next;
#if ARDPORTAL_ENABLE_MQTT
  if(source==ChangeSource::Mqtt) return acceptMqttState(key,next);
#endif
  if(_portal._saveQueued||_portal._pendingReady||_portal._storage.busy()||_portal.mqttTrialActive()) return false;
  if(!_portal.scheduleAppPatch(std::move(patch),source))return false;
  return applyAppState(key,next,source);
#else
  (void)definition;(void)index;(void)value;(void)source;return false;
#endif
}

#if ARDPORTAL_ENABLE_MQTT
/**
 * @brief Record a closed MQTT transport and arrange reconnection.
 * @return No value.
 */
void ArdAppControls::disconnected() {
#if ARDPORTAL_CONTROL_SUPPORT_EMISSIONS
  for (auto& emission : _appEmissions) emission = AppEmission();
  _emissionCount = _emissionHead = 0;
#endif
}
#endif

#if ARDPORTAL_ENABLE_MQTT
/**
 * @brief Yield after publishing an MQTT discovery or state message.
 * @return No value.
 */
void ArdAppControls::yieldAfterPublish() {
  if (_yieldPending) { _yieldPending=false; yield(); }
}
#endif

/**
 * @brief Validate and apply an incoming application configuration change.
 * @param app Application configuration object.
 * @return No value.
 */
void ArdAppControls::applyAppConfig(V app) {
  ChangeSource source=_portal._saveSource; V previous=std::move(_portal._appConfig); _portal._appConfig=std::move(app); ++_portal._appRevision;
  V keys=previous.keys(), next=_portal._appConfig.keys();
  for(size_t i=0;i<next.length();++i) if(!previous.hasOwnProperty(next[i].asString())) keys.push(next[i]);
  for(size_t i=0;i<keys.length();++i) {
    String key=keys[i].asString(); V old=_appState.hasOwnProperty(key)?static_cast<const V&>(_appState)[key]:static_cast<const V&>(previous)[key];
    if(_appState.hasOwnProperty(key)&&_portal._json.stringify(_appState[key],false,nullptr,snapshotLimits())==_portal._json.stringify(static_cast<const V&>(_portal._appConfig)[key],false,nullptr,snapshotLimits())) _appState.remove(key);
    V value=_portal.getAppConfigValue(key.c_str());
    if(ArdJSON::JSON.stringify(old,false,nullptr,snapshotLimits())==ArdJSON::JSON.stringify(value,false,nullptr,snapshotLimits())) continue;
#if ARDPORTAL_ENABLE_HA && ARDPORTAL_ENABLE_DEPENDENCIES
    _portal._homeAssistant.dependencyChanged(key);
#endif
    markDirty(key);
    // MQTT intent was already dispatched on receipt. A flash commit is not a new command.
    if(source!=ChangeSource::Mqtt && _portal._appChanged) { V copy=value; _portal._appChanged(key,copy,source); }
  }
}


void ArdAppControls::applyAppPatch(V patch){
  // Snapshot only affected effective values, never every saved field/key.
  V old=V::object(),keys=patch.keys();
  patch.forEachObjectMember([&](const String& key,const V&){old[key]=_portal.getAppConfigValue(key.c_str());return old.isObjectPatchValid();});
  if(!_portal._appConfig.applyObjectPatch(std::move(patch)))return;
  ++_portal._appRevision;
  for(size_t i=0;i<keys.length();++i){String key=keys[i].asString();
    const V& saved=_portal._appConfig;
    if(_appState.hasOwnProperty(key)){
      String runtime=_portal._json.stringify(_appState[key],false,nullptr,snapshotLimits()),stored=_portal._json.stringify(saved[key],false,nullptr,snapshotLimits());
      if(runtime.length()&&stored.length()&&runtime==stored)_appState.remove(key);
    }
    V value=_portal.getAppConfigValue(key.c_str());
    const V& previous=static_cast<const V&>(old)[key];
    String before=_portal._json.stringify(previous,false,nullptr,snapshotLimits()),after=_portal._json.stringify(value,false,nullptr,snapshotLimits());
    if((previous.isUndefined()&&value.isUndefined())||(before.length()&&after.length()&&before==after))continue;
#if ARDPORTAL_ENABLE_HA && ARDPORTAL_ENABLE_DEPENDENCIES
    _portal._homeAssistant.dependencyChanged(key);
#endif
    markDirty(key);
    if(_portal._saveSource!=ChangeSource::Mqtt&&_portal._appChanged)_portal._appChanged(key,value,_portal._saveSource);
  }
}
#if ARDPORTAL_ENABLE_MQTT
/**
 * @brief Publish the application state after a successful accepted update.
 * @param saved Result of the persistent save operation.
 * @return No value.
 */
void ArdAppControls::acknowledgeSave(bool saved) {
  if (saved) _stateDirty |= _mqttAckInFlight;
  _mqttAckInFlight = 0;
}
#endif

/**
 * @brief Mark affected entities or fields for incremental publication.
 * @param key Configuration key or JSON object member name.
 * @return No value.
 */
void ArdAppControls::markDirty(const String& key) {
#if ARDPORTAL_ENABLE_MQTT
  size_t index=_portal._dynamic.indexOf(key);if(index<_portal._dynamic.count())_stateDirty|=ArdAppConfigFieldMask::forField(index);
#else
  (void)key;
#endif
}
#if ARDPORTAL_ENABLE_MQTT
/**
 * @brief Check whether MQTT is connected and portal operations permit application publication.
 * @return True if application MQTT work may proceed; false otherwise.
 */
bool ArdAppControls::canServiceMqtt() const {
  return _portal._dynamic.count() && _portal.mqttConnected() && !_portal.mqttTrialActive() && !_portal.otaActive() && !_portal._rebootPending;
}
/**
 * @brief Reset subscription and publication cursors for the next MQTT connection.
 * @return No value.
 */
void ArdAppControls::resetMqtt() {
  _subscriptions=0; _stateDirty=stateMask(); _stateSince=millis();
}
/**
 * @brief Apply an incoming MQTT state value to its application field.
 * @param key Configuration key or JSON object member name.
 * @param value Input value, or output destination when passed by mutable reference.
 * @return True on success; false if validation, resource allocation or the operation fails.
 */
bool ArdAppControls::acceptMqttState(const String& key,V value) {
  // Own the command value: application callbacks can replace the reported state.
  if(!applyAppState(key,value,ChangeSource::Mqtt)) return false;
  markDirty(key);
  const V& field=_portal._dynamic.field(key);
  if(field.hasOwnProperty("persist")&&!field["persist"].asBool())return true;
  _mqttAppQueue[key]=value;
  if(!_mqttAppQueue.isValid()) return false;
  size_t index=_portal._dynamic.indexOf(key);if(index<_portal._dynamic.count())_mqttAckPending|=ArdAppConfigFieldMask::forField(index);
  return true;
}
/**
 * @brief Apply the MQTT endpoint and identity settings before connection.
 * @param now Current time used to evaluate deadlines.
 * @return No value.
 */
void ArdAppControls::prepareMqtt(uint32_t now) {
  // Only persistence waits for flash. Accepted commands have already run.
  if(_mqttAppQueue.length()&&!_portal.portalAndAppConfigBusy()&&!_portal._httpWaitingStorage) {
    if(_portal.scheduleAppPatch(_mqttAppQueue,ChangeSource::Mqtt)) {
      _mqttAckInFlight=_mqttAckPending; _mqttAckPending=0;
      _mqttAppQueue=V::object();
    }
  }
  if(_portal._options.appStateIntervalMs && uint32_t(now-_stateSince)>=_portal._options.appStateIntervalMs) {
    _stateDirty=stateMask(); _stateSince=now;
  }
}
/**
 * @brief Publish application command-topic subscriptions incrementally.
 * @return True when the subscription pass advances or is complete; false when it cannot proceed.
 */
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
/**
 * @brief Publish the next pending application state within the cooperative publication budget.
 * @return True if publication work was performed or advanced; false if it could not proceed.
 */
bool ArdAppControls::publishState() {
  size_t f=_stateDirty.firstSet();if(f>=_portal._dynamic.count())return false;
  {
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
      payload=value.type()==V::Type::String?value.asString():_portal._json.stringify(value,false,nullptr,snapshotLimits());
    if(_portal._mqttClient.publishRaw(topic.c_str(),payload.c_str(),true)) { _stateDirty&=~(ArdAppConfigFieldMask::forField(f)); _yieldPending=true; } return true;
  }
  return false;
}
/**
 * @brief Publish pending application state within the cooperative work budget.
 * @param now Current time used to evaluate deadlines.
 * @return No value.
 */
void ArdAppControls::serviceMqttValues(uint32_t now) {
  if(!canServiceMqtt()) return;
  prepareMqtt(now);
  if(_portal._mqttClient.sendBusy() || publishCommands()) return;
  publishState();
}
#endif
