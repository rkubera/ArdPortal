// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#include "ArdPortalDeclarations.h"
#include "Language.h"
#include "ConfigJson.h"
using V = ArdJSON::JSONVar;

bool ArdPortal::addAppConfigPage(const __FlashStringHelper* definition) {
  if(!definition || !addAppConfigPage(String(definition)))return false;
  _dynamic.pages.useFlash(_dynamic.pages.length()-1,definition);return true;
}
bool ArdPortal::addAppConfigPage(const String& definition) {
  ArdJSON::Limits limits; limits.maxNodes=4096; limits.maxInputBytes=32768;
  V page=_json.parse(definition,nullptr,limits);
  if(page["fields"].type()!=V::Type::Array) return false;
  for(size_t i=0;i<page["fields"].length();++i) {
    String type=page["fields"][i]["type"].asString();
    if(!ardPortalBasicControl(type)) {
      V resolved=page["fields"][i];if(!ArdHa::normalize(resolved)) return false;
      page["fields"][i]["extended"]=true;if(!page["fields"][i].hasOwnProperty("persist")) page["fields"][i]["persist"]=resolved["persist"];
    }
  }
  if(!_dynamic.add(page)) return false;
#if ARDPORTAL_ENABLE_HA
  for(size_t i=0;i<page["fields"].length();++i) {
    String payload=_homeAssistant.discoveryConfig(page["fields"][i]);
    // Reserve room for the longest supported device name and discovery topic.
    if(!payload.length() || payload.length()+400>ArdMqtt::PacketCapacity-5) { _dynamic.pages.remove(_dynamic.pages.length()-1); return false; }
  }
  _homeAssistant.resetDiscovery();
#elif ARDPORTAL_ENABLE_MQTT
  _appControls.resetMqtt();
#endif
  return true;
}

bool ArdPortal::dynamicHttp(const String& method,const String& path) {
  if(method=="GET"&&path.indexOf("/api/entity-type?type=")==0) {
    String type=path.substring(strlen("/api/entity-type?type="));
#if ARDPORTAL_CONTROL_SUPPORT_EXTENDED
    for(size_t i=0;i<ARD_HA_SPEC_COUNT;++i) {ArdHaSpec spec;memcpy_P(&spec,&ARD_HA_SPECS[i],sizeof(spec));if(type==String(FPSTR(spec.name))) {_page=spec.json;_pageLength=strlen_P(_page);_pageOffset=0;responseHeader(200,"application/json",_pageLength,false,true);return true;}}
#endif
    reply(404,"text/plain",ArdUILanguage::text(ArdUILanguage::Key::s_191)+type);return true;
  }
  if(method=="GET"&&path.indexOf("/p/")==0) {
    for(size_t p=0;p<_dynamic.pages.length();++p) if(path=="/p/"+_dynamic.pages.id(p)) return false;
    reply(404,"text/plain",ArdUILanguage::text(ArdUILanguage::Key::s_192)+path.substring(3)); return true;
  }
#if ARDPORTAL_ENABLE_DEPENDENCIES
  if(method=="GET"&&path=="/api/page-conditions") {
    ArdDynamicPages::PageStore::ReadScope definitions(_dynamic.pages);
    String body="{";
    for(size_t p=0;p<_dynamic.pages.length();++p) {
      if(p)body+=',';
      body+=_json.stringify(V(_dynamic.pages.id(p)))+":"+(_appControls.dependencyMatches(_dynamic.pages.pageCondition(p))?"true":"false");yield();
    }
    body+='}';reply(200,"application/json",body);return true;
  }
#endif
  if(method=="GET"&&path.indexOf("/api/pages?page=")==0) {
    const String id=path.substring(strlen("/api/pages?page="));
    for(size_t p=0;p<_dynamic.pages.length();++p)if(_dynamic.pages.id(p)==id){
      const V page=_dynamic.pages[p];ArdJSON::Limits limits;limits.maxNodes=4096;
      String body=_json.stringify(page,false,nullptr,limits);
      if(!page.isValid()||page.isUndefined()||!body.length())replyMessage(503,ArdUILanguage::Key::s_169);else reply(200,"application/json",body);
      return true;
    }
    reply(404,"text/plain",ArdUILanguage::text(ArdUILanguage::Key::s_192)+id);return true;
  }
  if(method=="GET"&&path=="/api/pages") {
    // Measure without an output buffer, then emit bounded JSON slices in loop().
    ArdJSON::Limits limits;limits.maxNodes=4096;String error;
    size_t length=_dynamic.pages.measure(limits,&error);
    if(!length) {reply(503,"text/plain",ArdUILanguage::text(error.indexOf("memory")>=0?ArdUILanguage::Key::s_169:ArdUILanguage::Key::s_190));return true;}
    responseHeader(200,"application/json",length,false,true);_response+='[';
    _httpDynamicPages=true;_httpDynamicPageStarted=true;_httpDynamicPageIndex=0;_httpDynamicPageCount=_dynamic.pages.length();_httpDynamicPageOffset=0;return true;
  }
  ArdDynamicPages::PageStore::ReadScope definitions(_dynamic.pages);
  if(method=="GET"&&(path=="/api/app"||path.indexOf("/api/app?page=")==0)) {
    String pageId=path=="/api/app"?String():path.substring(strlen("/api/app?page="));
    const V* page=nullptr;
    if(pageId.length()) {for(size_t p=0;p<_dynamic.pages.length();++p) if(_dynamic.pages.id(p)==pageId) {page=&definitions.get(p);}if(!page) {reply(404,"text/plain",ArdUILanguage::text(ArdUILanguage::Key::s_192)+pageId);return true;}}
    if(page) {
      if(!page->isValid()||page->isUndefined()){replyMessage(503,ArdUILanguage::Key::s_169);return true;}
      // Serialize fields individually; do not duplicate a composite state tree.
      String body="{\"values\":{";
      for(size_t f=0;f<(*page)["fields"].length();++f) {String id=(*page)["fields"][f]["id"].asString();String value=_json.stringify(getAppConfigValue(id.c_str()));if(!value.length()) {replyMessage(503,ArdUILanguage::Key::s_169);return true;}if(f) body+=',';body+=_json.stringify(V(id))+":"+value;}
#if ARDPORTAL_ENABLE_DEPENDENCIES
      body+= F("},\"conditions\":{");bool first=true;
      for(size_t f=0;f<(*page)["fields"].length();++f) {const V& field=(*page)["fields"][f];if(!field.hasOwnProperty("visibleWhen")&&!page->hasOwnProperty("visibleWhen"))continue;if(!first)body+=',';first=false;body+=_json.stringify(field["id"])+String(F(":{\"visible\":")) +((_appControls.dependencyMatches((*page)["visibleWhen"])&&_appControls.appFieldVisible(field))?String("true"):String("false"))+"}";}
#endif
      body+=String(F("},\"revision\":")) +String(_appRevision)+"}";reply(200,"application/json",body);return true;
    }
    V body=V::object(),values=_appConfig;
    for(size_t p=0;p<_dynamic.pages.length();++p){
      for(size_t f=0;f<_dynamic.pages.fields(p);++f){const String id=_dynamic.pages.fieldId(p,f);values[id]=getAppConfigValue(id.c_str());}
      definitions.release(p);yield();
    }
    body["values"]=values; body["revision"]=_appRevision; reply(200,"application/json",_json.stringify(body,false,nullptr,[](){ArdJSON::Limits l;l.maxNodes=4096;return l;}())); return true;
  }
#if ARDPORTAL_CONTROL_SUPPORT_EXTENDED
  if(method=="POST"&&path=="/api/app/command") {
    V body=_json.parse(_request.substring(_request.indexOf("\r\n\r\n")+4));const V& request=body;
    const V& field=_dynamic.field(request["field"].asString());bool belongs=false;
    for(size_t p=0;p<_dynamic.pages.length();++p) if(_dynamic.pages.id(p)==request["page"].asString()) if(_dynamic.pages.belongs(p,request["field"].asString())) belongs=true;
    _appControls.clearControlStage();uint64_t index;if(!belongs||!request["control"].toUnsignedInteger(index)||!_appControls.appControl(field,size_t(index),request["value"],ChangeSource::Portal)) {
      const bool unavailable=belongs&&field["persist"].asBool()&&!_storage.mounted();String error=ArdUILanguage::text(unavailable?ArdUILanguage::Key::s_170:_appControls.controlStage()==6?ArdUILanguage::Key::s_207:ArdUILanguage::Key::s_177)+" ["+request["field"].asString()+" #"+String(_appControls.controlStage())+"]";log(error);reply(unavailable?503:portalAndAppConfigBusy()?409:400,"text/plain",error);return true;
    }
    replyMessage(202,ArdUILanguage::Key::s_175);return true;
  }
#endif
  if(method!="POST"||path!="/api/app") return false;
  V body=_json.parse(_request.substring(_request.indexOf("\r\n\r\n")+4));
  const V& request=body; String pageId=request["page"].asString(); const V& values=request["values"];
  bool known=false; for(size_t p=0;p<_dynamic.pages.length();++p) if(_dynamic.pages.id(p)==pageId) known=true;
  if(!known || values.type()!=V::Type::Object || !values.length()) { replyMessage(400,ArdUILanguage::Key::s_177); return true; }
  V app=_savePending?_pendingApp:_appConfig,keys=values.keys();
  for(size_t i=0;i<keys.length();++i) {
    String key=keys[i].asString(); const V& f=_dynamic.field(key); bool belongs=false;
    for(size_t p=0;p<_dynamic.pages.length();++p) if(_dynamic.pages.id(p)==pageId) if(_dynamic.pages.belongs(p,key)) belongs=true;
    if(belongs&&!_appControls.appFieldVisible(f)){replyMessage(400,ArdUILanguage::Key::s_207);return true;}
    if(!belongs || ArdHa::readonly(f) || ArdHa::extended(f) || !ArdDynamicPages::validValue(f,values[key])) { replyMessage(400,ArdUILanguage::Key::s_177); return true; }
#if ARDPORTAL_ENABLE_CONTROL_CLIMATE
  if(f["type"].asString()=="climate") { V current=getAppConfigValue(key.c_str()); app[key]=values[key]; if(!current["action"].isUndefined()) app[key]["action"]=current["action"]; else app[key].remove("action"); app[key]["current_temperature"]=current["current_temperature"].isUndefined()?V(nullptr):current["current_temperature"]; }
    else
#endif
    app[key]=values[key];
  }
  if(!_configurationReady||_saveQueued||_pendingReady||mqttTrialActive()||_httpWaitingStorage||(_savePending&&ArdPortalJson::encode(_pending,_json)!=ArdPortalJson::encode(_config,_json))) {replyMessage(409,ArdUILanguage::Key::s_107);return true;}
  if(_savePending) {_pendingApp=app;_saveSource=ChangeSource::Portal;_dirtySince=millis();}
  else if(!scheduleConfig(_config,app,ChangeSource::Portal,false)) {replyMessage(409,ArdUILanguage::Key::s_107);return true;}
  for(size_t i=0;i<keys.length();++i) {String key=keys[i].asString();_appControls.applyAppState(key,app[key],ChangeSource::Portal);}
  replyMessage(202,ArdUILanguage::Key::s_175);return true;
}


bool ArdPortal::setAppConfigStateValue(const char* key,const V& value,bool publishMqtt) { return _appControls.setAppConfigStateValue(key,value,publishMqtt); }
bool ArdPortal::queueAppConfigStatePublish(const char* key) {
#if ARDPORTAL_ENABLE_MQTT
  if(!key) return false;
  const auto& field=_dynamic.field(key);
  if(field.isUndefined() || ArdHa::transient(field)) return false;
  _appControls.markDirty(key); return true;
#else
  (void)key;return false;
#endif
}
bool ArdPortal::emitAppConfigEvent(const char* key,const V& value) { return _appControls.emitAppConfigEvent(key, value); }

