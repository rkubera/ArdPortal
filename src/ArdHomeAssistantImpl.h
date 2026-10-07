// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#include "ArdPortalFeatures.h"
#if ARDPORTAL_ENABLE_HA
#include "ArdHomeAssistant.h"
#include "ArdPortalDeclarations.h"
#include "Language.h"
using V = ArdJSON::JSONVar;
namespace {
String discoveryLabel(const V& definition) {
  String name=definition["name"].asString();
  if(name.length())return name;
  const V& names=definition["names"];
  name=names["en"].asString();
  if(!name.length()&&names.length())name=names[names.keys()[0].asString()].asString();
  return name;
}
}


void ArdHomeAssistant::resetDiscovery() {
  _haDiscovery=0; _haOnline=false; _haSince=millis();_serviceStage=0;
  _portal._appControls.resetMqtt();
#if ARDPORTAL_ENABLE_DEPENDENCIES
  _walk.reset();_walkDepth=0;_dependencyMask=0;
  const auto& definitions=_portal._dynamic;
  for(size_t p=0;p<definitions.pages.length();++p)for(size_t f=0;f<definitions.pages.fields(p);++f)if(definitions.pages.dependent(p,f))_dependencyMask|=ArdAppConfigFieldMask::forField(definitions.pages.fieldIndex(p,f));
  for(const auto* entity=definitions.entities.get();entity;entity=entity->next.get())if(entity->dependent)_dependencyMask|=ArdAppConfigFieldMask::forField(entity->index);
  _dependencyDirty=_dependencyMask;_dependencyKnown=_dependencyDiscovery=0;_dependencyRevision=_portal._appRevision;
#endif
}

void ArdHomeAssistant::registered(size_t first) {
  ArdAppConfigFieldMask states,dependencies;
  for(size_t i=first;i<_portal._dynamic.count();++i){auto bit=ArdAppConfigFieldMask::forField(i);if(!_portal._dynamic.transientAt(i))states|=bit;
#if ARDPORTAL_ENABLE_DEPENDENCIES
    if(_portal._dynamic.dependentAt(i))dependencies|=bit;
#endif
  }
  registered(first,states,dependencies);
}
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
void ArdHomeAssistant::completeDependency(bool visible) {
  auto bit=ArdAppConfigFieldMask::forField(_walkIndex);
  if(!_dependencyKnown.test(_walkIndex)||visible!=_dependencyVisible.test(_walkIndex)){if(_dependencyKnown.test(_walkIndex)||_walkIndex<_haDiscovery)_dependencyDiscovery|=bit;if(visible)_portal._appControls._stateDirty|=bit;}
  _dependencyVisible=visible?(_dependencyVisible|bit):(_dependencyVisible&~bit);_dependencyKnown|=bit;_dependencyDirty&=~bit;_walkDepth=0;_walk.reset();
}
bool ArdHomeAssistant::serviceDependencies() {
  if(_dependencyRevision!=_portal._appRevision){_dependencyRevision=_portal._appRevision;_dependencyDirty|=_dependencyMask;_walk.reset();_walkDepth=0;}
  if(!_walkDepth){_walkIndex=_dependencyDirty.firstSet();if(_walkIndex>=_portal._dynamic.count())return false;
    _walk.reset(new(std::nothrow) DependencyFrame[_portal._dynamic.count()]);if(!_walk)return true;
    _walkDepth=1;_walk[0]={uint16_t(_walkIndex),0};_walkActive=ArdAppConfigFieldMask::forField(_walkIndex);_walkComplete=0;return true;
  }
  DependencyFrame& frame=_walk[_walkDepth-1];
  if(frame.edge==2){auto bit=ArdAppConfigFieldMask::forField(frame.index);_walkActive&=~bit;_walkComplete|=bit;if(!--_walkDepth)completeDependency(true);return true;}
  V field=_portal._dynamic.at(frame.index);if(!field.isValid()||field.isUndefined()){completeDependency(false);return true;}
  const V& rule=field[frame.edge++?"visibleWhen":"_pageVisibleWhen"];if(rule.isUndefined())return true;
  if(!_portal._appControls.dependencyValueMatches(rule)){completeDependency(false);return true;}
  size_t next=_portal._dynamic.indexOf(rule["field"].asString());
  if(next>=_portal._dynamic.count()||_walkActive.test(next)){completeDependency(false);return true;}
  if(_walkComplete.test(next)||!_portal._dynamic.dependentAt(next))return true;
  if(_walkDepth>=_portal._dynamic.count()){completeDependency(false);return true;}
  _walkActive|=ArdAppConfigFieldMask::forField(next);_walk[_walkDepth++]={uint16_t(next),0};return true;
}
#endif

String ArdHomeAssistant::discoveryConfig(const V& definition,String* error) const {
  V resolved=definition;if(ArdHa::extended(resolved)&&!ArdHa::normalize(resolved)) {if(error)*error="descriptor normalization failed";return String();}const V& f=resolved;
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
  if(component=="device_automation"||component=="tag") {c.remove("unique_id");c.remove("name");c.remove("availability_topic");c.remove("icon");}
  for(const char* key:{"unit_of_measurement","device_class","state_class","entity_category"}) if(f.hasOwnProperty(key)) c[key]=f[key];
  return _portal._json.stringify(c,false,error);
}

// DOM parsing and core transport calls cannot be preempted mid-unit.
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
bool ArdHomeAssistant::serviceOne(uint32_t now) {
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
      if(_dependencyRevision==_portal._appRevision){size_t index=_dependencyDiscovery.firstSet();if(index<_portal._dynamic.count()){
        if(_dependencyDirty.test(index))break;
        auto bit=ArdAppConfigFieldMask::forField(index);V field=_portal._dynamic.at(index);if(field.isUndefined()||!field.isValid())return true;
        String topic=String(F("homeassistant/"))+ArdHa::component(field)+"/ardui_"+_portal.chipId()+"/"+field["id"].asString()+"/config";
        String payload=_dependencyVisible.test(index)?discoveryConfig(field):String();if(_dependencyVisible.test(index)&&!payload.length())return true;
        if(_portal._mqttClient.publishRaw(topic.c_str(),payload.c_str(),true)){_dependencyDiscovery&=~bit;if(_dependencyVisible.test(index)&&!ArdHa::transient(field))_portal._appControls._stateDirty|=bit;_portal._appControls._yieldPending=true;}return true;
      }}
#endif
      break;
    case 4:
      if(_portal._mqttClient.sendBusy()){_serviceStage=0;return false;}_serviceStage=5;
      if(_portal._appControls.publishState())return true;break;
    default:
      if(_portal._mqttClient.sendBusy()){_serviceStage=0;return false;}_serviceStage=0;
      if(_haDiscovery>=_portal._dynamic.count()){if(_portal._options.discoveryIntervalMs&&uint32_t(now-_haSince)>=_portal._options.discoveryIntervalMs){_haDiscovery=0;_haSince=now;}else return false;}
      {bool visible=true;
#if ARDPORTAL_ENABLE_DEPENDENCIES
        if(_dependencyMask.test(_haDiscovery)){if(_dependencyRevision!=_portal._appRevision||!_dependencyKnown.test(_haDiscovery)||_dependencyDirty.test(_haDiscovery))return false;visible=_dependencyVisible.test(_haDiscovery);}
#endif
        V field=_portal._dynamic.at(_haDiscovery);if(field.isUndefined()||!field.isValid())return true;
        String topic=String(F("homeassistant/"))+ArdHa::component(field)+"/ardui_"+_portal.chipId()+"/"+field["id"].asString()+"/config",payload=visible?discoveryConfig(field):String();
        if(visible&&!payload.length())return true;
        if(_portal._mqttClient.publishRaw(topic.c_str(),payload.c_str(),true)){if(visible&&!ArdHa::transient(field))_portal._appControls._stateDirty|=ArdAppConfigFieldMask::forField(_haDiscovery);++_haDiscovery;_portal._appControls._yieldPending=true;}return true;
      }
  }}return false;
}

bool ArdHomeAssistant::handleStatus(const String& topic, const uint8_t* data, size_t size) {
  if (topic != "homeassistant/status") return false;
  String value; for (size_t i=0; i<size; ++i) value += char(data[i]);
  if (value == "online") { uint8_t subscribed=_portal._appControls._subscriptions; resetDiscovery(); _portal._appControls._subscriptions=subscribed; }
  return true;
}

#endif
