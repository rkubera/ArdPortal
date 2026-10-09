// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#include "ArdPortalFeatures.h"
#if ARDPORTAL_ENABLE_HA
#include "ArdHomeAssistant.h"
#include "ArdPortalDeclarations.h"
#include "Language.h"
using V = ArdJSON::JSONVar;
namespace {
/**
 * @brief Resolve the localized label used in Home Assistant discovery.
 * @param definition JSON definition to register; flash sources must remain valid for the portal lifetime.
 * @return The resulting text; an empty value indicates no available text or failure where applicable.
 */
String discoveryLabel(const V& definition) {
  String name=definition["name"].asString();
  if(name.length())return name;
  const V& names=definition["names"];
  name=names["en"].asString();
  if(!name.length()&&names.length())name=names[names.keys()[0].asString()].asString();
  return name;
}
}


/**
 * @brief Restart the Home Assistant discovery publication cursor.
 * @return No value.
 */
void ArdHomeAssistant::resetDiscovery() {
  _haDiscovery=0; _haOnline=false;_discoveryRetryPending=false; _haSince=millis();_serviceStage=0;
  _portal._appControls.resetMqtt();
#if ARDPORTAL_ENABLE_DEPENDENCIES
  _walk.reset();_walkDepth=0;_dependencyRetryPending=false;_dependencyMask=0;
  const auto& definitions=_portal._dynamic;
  for(size_t p=0;p<definitions.pages.length();++p)for(size_t f=0;f<definitions.pages.fields(p);++f)if(definitions.pages.dependent(p,f))_dependencyMask|=ArdAppConfigFieldMask::forField(definitions.pages.fieldIndex(p,f));
  for(const auto* entity=definitions.entities.get();entity;entity=entity->next.get())if(entity->dependent)_dependencyMask|=ArdAppConfigFieldMask::forField(entity->index);
  _dependencyDirty=_dependencyMask;_dependencyKnown=_dependencyDiscovery=0;_dependencyRevision=_portal._appRevision;
#endif
}

/**
 * @brief Check whether the specified page or field has been registered.
 * @param first Whether to initialize the incremental publication pass.
 * @return No value.
 */
void ArdHomeAssistant::registered(size_t first) {
  ArdAppConfigFieldMask states,dependencies;
  for(size_t i=first;i<_portal._dynamic.count();++i){auto bit=ArdAppConfigFieldMask::forField(i);if(!_portal._dynamic.transientAt(i))states|=bit;
#if ARDPORTAL_ENABLE_DEPENDENCIES
    if(_portal._dynamic.dependentAt(i))dependencies|=bit;
#endif
  }
  registered(first,states,dependencies);
}
/**
 * @brief Check whether the specified page or field has been registered.
 * @param first Whether to initialize the incremental publication pass.
 * @param states Mask of application states to publish.
 * @param dependencies Mask of dependency fields requiring work.
 * @return No value.
 */
void ArdHomeAssistant::registered(size_t first,const ArdAppConfigFieldMask& states,const ArdAppConfigFieldMask& dependencies) {
  if(!first){_haSince=millis();_portal._appControls._stateSince=millis();}
  _portal._appControls._stateDirty|=states;
#if ARDPORTAL_ENABLE_DEPENDENCIES
  _dependencyMask|=dependencies;_dependencyDirty|=dependencies;
#else
  (void)dependencies;
#endif
}

#if ARDPORTAL_ENABLE_DEPENDENCIES
/**
 * @brief Invalidate the reverse-index closure of one changed controller.
 * @param key Changed application field identifier.
 * @return No value; only a traversal whose root is affected is cancelled.
 */
void ArdHomeAssistant::dependencyChanged(const String& key){
  size_t controller=_portal._dynamic.indexOf(key);
  if(!_portal._dynamic.dependencyControllers.test(controller))return;
  ArdAppConfigFieldMask pending=ArdAppConfigFieldMask::forField(controller),seen,affected;
  while(pending){size_t index=pending.firstSet();auto bit=ArdAppConfigFieldMask::forField(index);pending&=~bit;seen|=bit;
    auto next=_portal._dynamic.dependents(index);affected|=next;pending|=next&~seen;
  }
  _dependencyDirty|=affected;
  if(_walkDepth&&affected.test(_walkIndex)){_walk.reset();_walkDepth=0;}
}

/**
 * @brief Finish the current dependency visibility transition.
 * @param visible Whether the field or page should be visible.
 * @return No value.
 */
void ArdHomeAssistant::completeDependency(bool visible) {
  auto bit=ArdAppConfigFieldMask::forField(_walkIndex);
  if(!_dependencyKnown.test(_walkIndex)||visible!=_dependencyVisible.test(_walkIndex)){if(_dependencyKnown.test(_walkIndex)||_walkIndex<_haDiscovery)_dependencyDiscovery|=bit;if(visible)_portal._appControls._stateDirty|=bit;}
  _dependencyVisible=visible?(_dependencyVisible|bit):(_dependencyVisible&~bit);_dependencyKnown|=bit;_dependencyDirty&=~bit;_walkDepth=0;_walk.reset();
}
/**
 * @brief Advance dependency-driven visibility and state changes.
 * @return True on success; false if validation, resource allocation or the operation fails.
 */
bool ArdHomeAssistant::serviceDependencies() {
  const size_t workIndex=_walkDepth?_walk[_walkDepth-1].index:_dependencyDirty.firstSet();
  if(!ArdHeap::permits(_portal._dynamic.fieldWork(workIndex),_portal._dynamic.fieldBlock(workIndex)))return false;
  _dependencyRevision=_portal._appRevision; // UI revisions do not invalidate the HA dependency graph.
  if(_dependencyRetryPending){if(int32_t(millis()-_dependencyRetryAt)<0)return false;_dependencyRetryPending=false;}
  if(!_walkDepth){_walkIndex=_dependencyDirty.firstSet();if(_walkIndex>=_portal._dynamic.count())return false;
    if(!_walk.reserve(1,_portal._dynamic.count())){_dependencyRetryAt=millis()+1000;_dependencyRetryPending=true;return true;}
    _walkDepth=1;_walk[0]={uint16_t(_walkIndex),0,0};_walkActive=ArdAppConfigFieldMask::forField(_walkIndex);_walkComplete=0;_walkVisible=0;return true;
  }
  auto load=[&](size_t i){return _portal._dynamic.at(i);};
  auto indexOf=[&](const String& id){return _portal._dynamic.indexOf(id);};
  auto dependent=[&](size_t i){return _portal._dynamic.dependentAt(i);};
  auto match=[&](const V& leaf){return _portal._appControls.dependencyValueMatches(leaf);};
  auto result=ArdDependencies::step(_walk,_walkDepth,_walkActive,_walkComplete,_walkVisible,
                                  _portal._dynamic.count(),load,indexOf,dependent,match);
  if(result==ArdDependencies::Result::Invalid){
    _walk.reset();_walkDepth=0;_dependencyRetryAt=millis()+1000;_dependencyRetryPending=true;
  }else if(result!=ArdDependencies::Result::Running)completeDependency(result==ArdDependencies::Result::Visible);
  return true;
}
#endif

/**
 * @brief Build the Home Assistant MQTT discovery document for an entity.
 * @param definition JSON definition to register; flash sources must remain valid for the portal lifetime.
 * @param error Output error text; populated when the operation fails.
 * @return The resulting text; an empty value indicates no available text or failure where applicable.
 */
String ArdHomeAssistant::discoveryConfig(const V& definition,String* error) const {
  V resolved;const V* source=&definition;
  if(!definition.isValid()){if(error)*error="invalid discovery definition";return String();}
  if(ArdHa::extended(definition)){
    resolved=definition;
    if(!resolved.isValid()||!ArdHa::normalize(resolved)||!resolved.isValid()){
      if(error) *error="descriptor normalization failed";
      return String();
    }
    source=&resolved;
  }
  const V& f=*source;
  String id=f["id"].asString(),type=f["type"].asString(),state=_portal._mqttClient.mqttTopic("stat",id.c_str()),command=_portal._mqttClient.mqttTopic("cmnd",id.c_str());
  V c=V::object(); c["unique_id"]="ardui_"+_portal.chipId()+"_"+id;
  c["name"]=discoveryLabel(f);
  if(f.hasOwnProperty("icon")) c["icon"]=f["icon"];
  c["availability_topic"]=_portal._mqttClient.mqttTopic("stat","availability");
  V device=V::object(),ids=V::array(); ids.push("ardui_"+_portal.chipId()); device["identifiers"]=ids; device["name"]=_portal._config.deviceName; device["manufacturer"]=_portal._config.deviceManufacturer; device["model"]=_portal._config.deviceDescription; c["device"]=device;
  if(!ArdHa::extended(f)) {
#if ARDPORTAL_ENABLE_CONTROL_CLIMATE
    if(type=="climate") {
    c["temperature_command_topic"]=command+"_temperature"; c["temperature_state_topic"]=state; c["temperature_state_template"]="{{ value_json.temperature }}";
    c["current_temperature_topic"]=state; c["current_temperature_template"]="{{ value_json.current_temperature | default('', true) }}";
    c["min_temp"]=f["min"]; c["max_temp"]=f["max"]; c["temp_step"]=f["step"].isUndefined()?V(0.5):f["step"]; c["temperature_unit"]="C";
    if(f.hasOwnProperty("modes")) { c["modes"]=f["modes"]; c["mode_command_topic"]=command+"_mode"; c["mode_state_topic"]=state; c["mode_state_template"]="{{ value_json.mode }}"; }
    if(f.hasOwnProperty("fan_modes")) { c["fan_modes"]=f["fan_modes"]; c["fan_mode_command_topic"]=command+"_fan"; c["fan_mode_state_topic"]=state; c["fan_mode_state_template"]="{{ value_json.fan_mode }}"; }
    } else
#endif
    {
    c["state_topic"]=state;
    if(type!="text") c["command_topic"]=command;
#if ARDPORTAL_ENABLE_CONTROL_SLIDER
  if(type=="slider") { c["min"]=f["min"]; c["max"]=f["max"]; c["step"]=f["step"].isUndefined()?V(1):f["step"]; c["mode"]="slider"; }
#endif
#if ARDPORTAL_ENABLE_CONTROL_EDIT
  if(type=="edit") { c["min"]=0; c["max"]=128; }
#endif
#if ARDPORTAL_ENABLE_CONTROL_SELECT
  if(type=="select") {
    V options=V::array(),labels=V::object(),values=V::object();bool unique=true;
    for(size_t i=0;i<f["options"].length();++i) {
      const V& option=f["options"][i];String value=option["value"].asString();
      String label=discoveryLabel(option);
      if(!label.length())label=value;
      if(values.hasOwnProperty(label))unique=false;
      options.push(label);labels[value]=label;values[label]=value;
    }
    if(unique) {
      c["options"]=options;
      c["value_template"]="{{ "+_portal._json.stringify(labels)+".get(value, value) }}";
      c["command_template"]="{{ "+_portal._json.stringify(values)+".get(value, value) }}";
    } else {
      // HA requires unambiguous option labels; duplicate names use raw values.
      options=V::array();for(size_t i=0;i<f["options"].length();++i)options.push(f["options"][i]["value"]);
      c["options"]=options;
    }
  }
#endif
    }
  }
  const V& extra=f["ha"]; V keys=extra.keys();
  for(size_t i=0;i<keys.length();++i) {
    String key=keys[i].asString(); if(key=="unique_id"||key=="device"||key=="name"||key=="availability_topic") continue;
    V value=extra[key]; if(value.type()==V::Type::String) {String token=value.asString(); if(token=="$state") value=state;else if(token=="$command") value=command;else if(token.indexOf("$command:")==0) value=command+"_"+token.substring(9);}
    c[key]=value;
  }
  String component=ArdHa::component(f);
#if ARDPORTAL_ENABLE_CONTROL_SWITCH
  if(type=="switch") {
    // Keep HA mode defaults and explicit field options; choose a toggle icon.
    if(!c["state_topic"].asString().length())c["state_topic"]=state;
    if(!c.hasOwnProperty("icon"))c["icon"]="mdi:toggle-switch";
  }
#endif
  if(component=="device_automation"||component=="tag") {c.remove("unique_id");c.remove("name");c.remove("availability_topic");c.remove("icon");}
  for(const char* key:{"unit_of_measurement","device_class","state_class","entity_category"}) if(f.hasOwnProperty(key)) c[key]=f[key];
  return _portal._json.stringify(c,false,error);
}

// DOM parsing and core transport calls cannot be preempted mid-unit.
/**
 * @brief Publish the next pending Home Assistant discovery entry within the work budget.
 * @param now Current time used to evaluate deadlines.
 * @return No value.
 */
void ArdHomeAssistant::serviceDiscovery(uint32_t now) {
  if(!_portal._appControls.canServiceMqtt()){
#if ARDPORTAL_ENABLE_DEPENDENCIES
    _walk.reset();_walkDepth=0;
#endif
    return;
  }
  uint32_t start=millis();for(uint16_t operations=0;operations<_portal._options.haMaxOperationsPerLoop;++operations){
    if(operations&&uint32_t(millis()-start)>=_portal._options.haWorkBudgetMs)break;
    if(!serviceOne(now))break;
  }
}
/**
 * @brief Perform one pending Home Assistant publication or dependency work unit.
 * @param now Current time used to evaluate deadlines.
 * @return True if a work unit was performed; false if there is no eligible work.
 */
bool ArdHomeAssistant::serviceOne(uint32_t now) {
  auto defer=[&](){_discoveryRetryAt=now+1000;_discoveryRetryPending=true;};
  if(_discoveryRetryPending&&int32_t(now-_discoveryRetryAt)>=0)_discoveryRetryPending=false;
  for(unsigned skipped=0;skipped<6;++skipped){switch(_serviceStage){
    case 0:_portal._appControls.prepareMqtt(now);_serviceStage=1;return true;
    case 1:
      if(_portal._mqttClient.sendBusy()){_serviceStage=0;return false;}
      if(_portal._appControls.publishCommands())return true;
      if(!_haOnline){String topic=_portal._mqttClient.mqttTopic("stat","availability");if(_portal._mqttClient.publishRaw(topic.c_str(),"online",true)){_haOnline=true;_portal._appControls._yieldPending=true;}return true;}
      _serviceStage=2;break;
    case 2:_serviceStage=3;
#if ARDPORTAL_ENABLE_DEPENDENCIES
      if(serviceDependencies())return true;
#endif
      break;
    case 3:
      if(_portal._mqttClient.sendBusy()){_serviceStage=0;return false;}_serviceStage=4;
#if ARDPORTAL_ENABLE_DEPENDENCIES
      {size_t index=_dependencyDiscovery.firstSet();if(index<_portal._dynamic.count()){
        if(_dependencyDirty.test(index)||_discoveryRetryPending||!ArdHeap::permits(_portal._dynamic.fieldWork(index),_portal._dynamic.fieldBlock(index)))break;
        auto bit=ArdAppConfigFieldMask::forField(index);V field=_portal._dynamic.at(index);if(field.isUndefined()||!field.isValid()){defer();return true;}
        String topic=String(F("homeassistant/"))+ArdHa::component(field)+"/ardui_"+_portal.chipId()+"/"+field["id"].asString()+"/config";
        String payload=_dependencyVisible.test(index)?discoveryConfig(field):String();if(_dependencyVisible.test(index)&&!payload.length()){defer();return true;}
        if(_portal._mqttClient.publishRaw(topic.c_str(),payload.c_str(),true)){_dependencyDiscovery&=~bit;if(_dependencyVisible.test(index)&&!ArdHa::transient(field))_portal._appControls._stateDirty|=bit;_portal._appControls._yieldPending=true;}else defer();return true;
      }}
#endif
      break;
    case 4:
      if(_portal._mqttClient.sendBusy()){_serviceStage=0;return false;}_serviceStage=5;
      if(_portal._appControls.publishState())return true;break;
    default:
      if(_portal._mqttClient.sendBusy()){_serviceStage=0;return false;}_serviceStage=0;
      if(_haDiscovery>=_portal._dynamic.count()){if(_portal._options.discoveryIntervalMs&&uint32_t(now-_haSince)>=_portal._options.discoveryIntervalMs){_haDiscovery=0;_haSince=now;}else return false;}
      if(_discoveryRetryPending||!ArdHeap::permits(_portal._dynamic.fieldWork(_haDiscovery),_portal._dynamic.fieldBlock(_haDiscovery)))return false;
      {bool visible=true;
#if ARDPORTAL_ENABLE_DEPENDENCIES
        if(_dependencyMask.test(_haDiscovery)){if(!_dependencyKnown.test(_haDiscovery)||_dependencyDirty.test(_haDiscovery))return false;visible=_dependencyVisible.test(_haDiscovery);}
#endif
        V field=_portal._dynamic.at(_haDiscovery);if(field.isUndefined()||!field.isValid()){defer();return true;}
        String topic=String(F("homeassistant/"))+ArdHa::component(field)+"/ardui_"+_portal.chipId()+"/"+field["id"].asString()+"/config",payload=visible?discoveryConfig(field):String();
        if(visible&&!payload.length()){defer();return true;}
        if(_portal._mqttClient.publishRaw(topic.c_str(),payload.c_str(),true)){if(visible&&!ArdHa::transient(field))_portal._appControls._stateDirty|=ArdAppConfigFieldMask::forField(_haDiscovery);++_haDiscovery;_portal._appControls._yieldPending=true;}else defer();return true;
      }
  }}return false;
}

/**
 * @brief Handle the Home Assistant birth/status topic and schedule discovery when HA comes online.
 * @param topic MQTT topic to publish, subscribe or match.
 * @param data Data buffer or value used by the operation.
 * @param size Number of bytes or elements.
 * @return True if the message belongs to the Home Assistant status topic; false otherwise.
 */
bool ArdHomeAssistant::handleStatus(const String& topic, const uint8_t* data, size_t size) {
  if (topic != "homeassistant/status") return false;
  String value; for (size_t i=0; i<size; ++i) value += char(data[i]);
  if (value == "online") { uint8_t subscribed=_portal._appControls._subscriptions; resetDiscovery(); _portal._appControls._subscriptions=subscribed; }
  return true;
}

#endif
