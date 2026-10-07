// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#include "ArdPortalDeclarations.h"
#include "Language.h"
#include "ConfigJson.h"
using V = ArdJSON::JSONVar;

bool ArdPortal::addAppConfigEntity(const __FlashStringHelper* definition) {
#if ARDPORTAL_ENABLE_HA
  if(!definition||!addAppConfigEntity(String(definition)))return false;
  _dynamic.entities->flash=definition;_dynamic.entities->source=String();return true;
#else
  (void)definition;return false;
#endif
}
bool ArdPortal::addAppConfigEntity(const String& definition) {
  if(appConfigPageRegistrationBusy())return appConfigRegistrationFailed("busy","","another page registration is pending");
#if ARDPORTAL_ENABLE_HA
  ArdJSON::Limits limits;limits.maxNodes=4096;
  V field=_json.parse(definition,nullptr,limits);
  if(field.type()!=V::Type::Object)return false;
  if(!ardPortalBasicControl(field["type"].asString())){V resolved=field;if(!ArdHa::normalize(resolved))return false;field["extended"]=true;if(!field.hasOwnProperty("persist"))field["persist"]=resolved["persist"];}
  String payload=_homeAssistant.discoveryConfig(field);if(!payload.length()||payload.length()+400>ArdMqtt::PacketCapacity-5)return false;
  V page=V::object(),fields=V::array();page["id"]="ha_entities";page["name"]="HA entities";page["order"]=0;fields.push(field);page["fields"]=std::move(fields);
  size_t first=_dynamic.count();if(!_dynamic.addEntity(page))return false;
  _homeAssistant.registered(first);return true;
#else
  (void)definition;return false;
#endif
}

#include "AppConfigPageRegistrationImpl.h"

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
    String body="{";
    for(size_t p=0;p<_dynamic.pages.length();++p) {
      if(p)body+=',';
      body+=_json.stringify(V(_dynamic.pages.id(p)))+":"+(_appControls.dependencyMatches(_dynamic.pages.pageCondition(p))?"true":"false");yield();
    }
    body+='}';reply(200,"application/json",body);return true;
  }
#endif
  if(method=="GET"&&path=="/api/page-catalog") {
    responseHeader(200,"application/json",_dynamic.pages.menuLength(),false,true);
    _httpDynamicPages=true;_httpDynamicMode=3;_httpDynamicStage=0;
    _httpDynamicPageIndex=0;_httpDynamicPageCount=_dynamic.pages.length();_httpDynamicPageOffset=0;return true;
  }
  if(method=="GET"&&path.indexOf("/api/pages?page=")==0) {
    const String id=path.substring(strlen("/api/pages?page="));
    for(size_t p=0;p<_dynamic.pages.length();++p)if(_dynamic.pages.id(p)==id){
      responseHeader(200,"application/json",_dynamic.pages.jsonLength(p),false,true);
      _httpDynamicPages=true;_httpDynamicMode=1;_httpDynamicStage=0;_httpDynamicField=0;
      _httpDynamicPageIndex=p;_httpDynamicPageCount=p+1;_httpDynamicPageOffset=0;return true;
    }
    reply(404,"text/plain",ArdUILanguage::text(ArdUILanguage::Key::s_192)+id);return true;
  }
  if(method=="GET"&&path=="/api/pages") {
    // Measure without an output buffer, then emit bounded JSON slices in loop().
    ArdJSON::Limits limits;limits.maxNodes=4096;limits.maxOutputBytes=ArdAppConfigMaxDefinitionBytes;String error;
    size_t length=_dynamic.pages.measure(limits,&error);
    if(!length) {reply(503,"text/plain",ArdUILanguage::text(error.indexOf("memory")>=0?ArdUILanguage::Key::s_169:ArdUILanguage::Key::s_190));return true;}
    responseHeader(200,"application/json",length,false,true);
    _httpDynamicPages=true;_httpDynamicMode=0;_httpDynamicStage=0;_httpDynamicField=0;_httpDynamicPageStarted=true;_httpDynamicPageIndex=0;_httpDynamicPageCount=_dynamic.pages.length();_httpDynamicPageOffset=0;return true;
  }
  if(method=="GET"&&(path=="/api/app"||path.indexOf("/api/app?page=")==0)) {
    String pageId=path=="/api/app"?String():path.substring(strlen("/api/app?page="));
    if(pageId.length()) {
      for(size_t p=0;p<_dynamic.pages.length();++p)if(_dynamic.pages.id(p)==pageId){
        // Connection-close framing permits unknown state lengths without a full
        // measurement pass or a composite state allocation.
        responseHeader(200,"application/json",size_t(-1),false,true);
        _httpDynamicPages=true;_httpDynamicMode=2;_httpDynamicStage=0;_httpDynamicField=0;
        _httpDynamicConditionFirst=true;_httpDynamicRevision=_appRevision;
        _httpDynamicPageIndex=p;_httpDynamicPageCount=p+1;_httpDynamicPageOffset=0;return true;
      }
      reply(404,"text/plain",ArdUILanguage::text(ArdUILanguage::Key::s_192)+pageId);return true;
    }
    V body=V::object(),values=_appConfig;
    for(size_t i=0;i<_dynamic.count();++i){const String id=_dynamic.idAt(i);values[id]=getAppConfigValue(id.c_str());yield();}
    body["values"]=values;body["revision"]=_appRevision;String output=_json.stringify(body,false,nullptr,snapshotLimits());
    if(!output.length())replyMessage(503,ArdUILanguage::Key::s_169);else reply(200,"application/json",output);return true;
  }
#if ARDPORTAL_CONTROL_SUPPORT_EXTENDED
  if(method=="POST"&&path=="/api/app/command") {
    V body=_json.parse(_request.substring(_request.indexOf("\r\n\r\n")+4),nullptr,snapshotLimits());const V& request=body;
    const V& field=_dynamic.field(request["field"].asString());bool belongs=false;
    for(size_t p=0;p<_dynamic.pages.length();++p) if(_dynamic.pages.id(p)==request["page"].asString()) if(_dynamic.pages.belongs(p,request["field"].asString())) belongs=true;
    _appControls.clearControlStage();uint64_t index;if(!belongs||!request["control"].toUnsignedInteger(index)||!_appControls.appControl(field,size_t(index),request["value"],ChangeSource::Portal)) {
      const bool unavailable=belongs&&field["persist"].asBool()&&!_storage.mounted();String error=ArdUILanguage::text(unavailable?ArdUILanguage::Key::s_170:_appControls.controlStage()==6?ArdUILanguage::Key::s_207:ArdUILanguage::Key::s_177)+" ["+request["field"].asString()+" #"+String(_appControls.controlStage())+"]";log(error);reply(unavailable?503:portalAndAppConfigBusy()?409:400,"text/plain",error);return true;
    }
    replyMessage(202,ArdUILanguage::Key::s_175);return true;
  }
#endif
  if(method!="POST"||path!="/api/app") return false;
  V body=_json.parse(_request.substring(_request.indexOf("\r\n\r\n")+4),nullptr,snapshotLimits());
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


// Produce one field per loop; retained text is at most one serialized field.
bool ArdPortal::prepareDynamicHttpPart() {
  ArdJSON::Limits limits;limits.maxNodes=4096;
  if(_httpDynamicStage==0){
    if((_httpDynamicMode==0||_httpDynamicMode==3)&&!_httpDynamicPageCount){_httpDynamicText="[]";_httpDynamicPages=false;return true;}
    if(_httpDynamicMode==3){_httpDynamicText="["+_dynamic.pages.menu(0);_httpDynamicStage=1;return true;}
    _httpDynamicText=_httpDynamicMode==2?String(F("{\"values\":{")):
      (_httpDynamicMode==0?String("["):String())+_dynamic.pages.headerPrefix(_httpDynamicPageIndex);
    _httpDynamicStage=1;return _httpDynamicText.length();
  }
  if(_httpDynamicMode==3){
    ++_httpDynamicPageIndex;
    if(_httpDynamicPageIndex<_httpDynamicPageCount)_httpDynamicText=","+_dynamic.pages.menu(_httpDynamicPageIndex);
    else{_httpDynamicText="]";_httpDynamicPages=false;}
    return true;
  }
  const size_t p=_httpDynamicPageIndex;
  if(_httpDynamicStage==1 && _httpDynamicField<_dynamic.pages.fields(p)){
    const size_t f=_httpDynamicField++;
    if(_httpDynamicMode==2){
      const char* id=_dynamic.pages.fieldId(p,f);
      String value=_json.stringify(getAppConfigValue(id),false,nullptr,snapshotLimits());
      if(!value.length())return false;
      _httpDynamicText=(f?String(","):String())+_json.stringify(V(id))+":"+value;
    }else{
      V field=_dynamic.pages.fieldValue(p,f);field.remove("_pageVisibleWhen");
      if(!field.isValid()||field.isUndefined())return false;
      String value=_json.stringify(field,false,nullptr,limits);if(!value.length())return false;
      _httpDynamicText=(f?String(","):String())+value;
    }
    return true;
  }
  if(_httpDynamicMode!=2){
    ++_httpDynamicPageIndex;_httpDynamicField=0;
    if(_httpDynamicPageIndex<_httpDynamicPageCount){_httpDynamicText="]},"+_dynamic.pages.headerPrefix(_httpDynamicPageIndex);return true;}
    _httpDynamicText=_httpDynamicMode==0?String("]}]"):String("]}");_httpDynamicPages=false;return true;
  }
  if(_httpDynamicStage==1){
    _httpDynamicField=0;_httpDynamicStage=2;
#if ARDPORTAL_ENABLE_DEPENDENCIES
    _httpDynamicText=F("},\"conditions\":{");return true;
#endif
  }
#if ARDPORTAL_ENABLE_DEPENDENCIES
  if(_httpDynamicField<_dynamic.pages.fields(p)){
    const size_t f=_httpDynamicField++;
    if(!_dynamic.pages.dependent(p,f)){_httpDynamicText=" ";return true;}
    V field=_dynamic.pages.fieldValue(p,f);if(!field.isValid()||field.isUndefined())return false;
    _httpDynamicText=(_httpDynamicConditionFirst?String():String(","))+_json.stringify(V(_dynamic.pages.fieldId(p,f)))+F(":{\"visible\":")+
      (_appControls.appFieldVisible(field)?String("true"):String("false"))+"}";
    _httpDynamicConditionFirst=false;return true;
  }
#endif
  _httpDynamicText=String(F("},\"revision\":"))+String(_httpDynamicRevision)+"}";
  _httpDynamicPages=false;return true;
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

