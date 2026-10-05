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
  _haDiscovery=0; _haOnline=false; _haSince=millis();
  _portal._appControls.resetMqtt();
#if ARDPORTAL_ENABLE_DEPENDENCIES
  _dependencyMask=0;size_t index=0;
  for(size_t p=0;p<_portal._dynamic.pages.length();++p) for(size_t f=0;f<_portal._dynamic.pages.fields(p);++f,++index) if(_portal._dynamic.pages.dependent(p,f)) _dependencyMask|=uint64_t(1)<<index;
  _dependencyDirty=_dependencyMask;_dependencyKnown=_dependencyDiscovery=0;_dependencyRevision=_portal._appRevision;
#endif
}

#if ARDPORTAL_ENABLE_DEPENDENCIES
void ArdHomeAssistant::serviceDependencies() {
  if(_dependencyRevision!=_portal._appRevision) {_dependencyRevision=_portal._appRevision;_dependencyDirty|=_dependencyMask;}
  for(size_t i=0;i<_portal._dynamic.count();++i) {uint64_t bit=uint64_t(1)<<i;if(!(_dependencyDirty&bit)) continue;
    V field=_portal._dynamic.at(i);if(!field.isValid()||field.isUndefined()) return;
    bool visible=_portal._appControls.appFieldVisible(field);
    if(!(_dependencyKnown&bit)||visible!=bool(_dependencyVisible&bit)) {_dependencyDiscovery|=bit;if(visible) _portal._appControls._stateDirty|=bit;}
    _dependencyVisible=visible?(_dependencyVisible|bit):(_dependencyVisible&~bit);
    _dependencyKnown|=bit;_dependencyDirty&=~bit;return;
  }
}

#endif
String ArdHomeAssistant::discoveryConfig(const V& definition) const {
  V resolved=definition;if(ArdHa::extended(resolved)&&!ArdHa::normalize(resolved)) return String();const V& f=resolved;
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
  return _portal._json.stringify(c);
}

void ArdHomeAssistant::serviceDiscovery(uint32_t now) {
  if(!_portal._appControls.canServiceMqtt()) return;
  _portal._appControls.prepareMqtt(now);
#if ARDPORTAL_ENABLE_DEPENDENCIES
  serviceDependencies();
#endif
  if(_portal._mqttClient.sendBusy()) return;
  if(_portal._appControls.publishCommands()) return;
  if(!_haOnline) { String topic=_portal._mqttClient.mqttTopic("stat","availability"); if(_portal._mqttClient.publishRaw(topic.c_str(),"online",true)) { _haOnline=true; _portal._appControls._yieldPending=true; } return; }
#if ARDPORTAL_ENABLE_DEPENDENCIES
  for(size_t i=0;i<_portal._dynamic.count();++i) {uint64_t bit=uint64_t(1)<<i;
    if(_dependencyDiscovery&bit) {V field=_portal._dynamic.at(i);if(field.isUndefined()) return;String topic=String(F("homeassistant/")) +ArdHa::component(field)+"/ardui_"+_portal.chipId()+"/"+field["id"].asString()+"/config";
      String payload=(_dependencyVisible&bit)?discoveryConfig(field):String();if((_dependencyVisible&bit)&&!payload.length())return;
      if(_portal._mqttClient.publishRaw(topic.c_str(),payload.c_str(),true)){_dependencyDiscovery&=~bit;_portal._appControls._yieldPending=true;}return;}
  }
#endif
  if(_portal._appControls.publishState()) return;
  if(_haDiscovery>=_portal._dynamic.count()) { if(_portal._options.discoveryIntervalMs && uint32_t(now-_haSince)>=_portal._options.discoveryIntervalMs) { _haDiscovery=0; _haSince=now; } else return; }
  const V& f=_portal._dynamic.at(_haDiscovery);if(f.isUndefined()||!f.isValid())return; String component=ArdHa::component(f);
  String topic=String(F("homeassistant/")) +component+"/ardui_"+_portal.chipId()+"/"+f["id"].asString()+"/config",payload=_portal._appControls.appFieldVisible(f)?discoveryConfig(f):String();
  if(_portal._appControls.appFieldVisible(f)&&!payload.length())return;
  if(_portal._mqttClient.publishRaw(topic.c_str(),payload.c_str(),true)) { ++_haDiscovery; _portal._appControls._yieldPending=true; }
}

bool ArdHomeAssistant::handleStatus(const String& topic, const uint8_t* data, size_t size) {
  if (topic != "homeassistant/status") return false;
  String value; for (size_t i=0; i<size; ++i) value += char(data[i]);
  if (value == "online") { uint8_t subscribed=_portal._appControls._subscriptions; resetDiscovery(); _portal._appControls._subscriptions=subscribed; }
  return true;
}

#endif
