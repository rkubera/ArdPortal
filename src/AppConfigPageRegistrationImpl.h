// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once

bool ArdPortal::startAppConfigPageRegistration(const String& definition) {return beginAppConfigPageRegistration(&definition,nullptr);}
bool ArdPortal::startAppConfigPageRegistration(String&& definition) {return beginAppConfigPageRegistration(&definition,nullptr,&definition);}
bool ArdPortal::startAppConfigPageRegistration(const __FlashStringHelper* definition) {return beginAppConfigPageRegistration(nullptr,definition);}
bool ArdPortal::beginAppConfigPageRegistration(const String* source,const __FlashStringHelper* flash,String* owned) {
  if(appConfigPageRegistrationBusy())return appConfigRegistrationFailed("busy","",F("another page registration is pending"));
  if(otaActive()||_rebootPending)return appConfigRegistrationFailed("busy","",F("OTA or restart is in progress"));
  _appConfigPageRegistrationState=AppConfigPageRegistrationState::Failed;
  _appConfigRegistrationError={};_appConfigPageRegistrationProcessed=_appConfigPageRegistrationTotal=0;
  if(!source&&!flash)return appConfigRegistrationFailed("input","",F("null definition"));
  if(_dynamic.pages.length()>=16)return appConfigRegistrationFailed("limits","",F("page limit: 16"));
  std::unique_ptr<ArdAppConfigPageRegistration> work(new(std::nothrow) ArdAppConfigPageRegistration());
  if(!work)return appConfigRegistrationFailed("registry","",F("out of memory allocating registration"));
  work->length=source?source->length():strlen_P(reinterpret_cast<const char*>(flash));
  if(!work->length||work->length>ArdAppConfigMaxDefinitionBytes)return appConfigRegistrationFailed("input","",String("definition is empty or exceeds the definition budget: ")+String(ArdAppConfigMaxDefinitionBytes)+" bytes");
  if(source){if(owned)work->source=std::move(*owned);else work->source=*source;if(work->source.length()!=work->length)return appConfigRegistrationFailed("input","",F("out of memory copying definition"));}
  work->flash=flash;work->first=_dynamic.count();
  work->wholePass=work->length<=2048;
  _appConfigPageRegistration=std::move(work);_appConfigPageRegistrationState=AppConfigPageRegistrationState::Pending;
  ++_appConfigPageRegistrationGeneration;
  return true;
}
void ArdPortal::finishAppConfigPageRegistration(bool success) {
  _appConfigPageRegistration.reset();
  _appConfigPageRegistrationState=success?AppConfigPageRegistrationState::Succeeded:AppConfigPageRegistrationState::Failed;
  if(success)_appConfigRegistrationError={};
  // Release all staging allocations before invoking application code. The
  // callback may queue the next page; the current loop does not process it.
  auto callback=_appConfigPageRegistrationFinished;if(callback)callback(success);
}
// Keep conversion/logging of flash-resident reasons in one place instead of
// emitting String construction and destruction at every validation branch.
void ArdPortal::failAppConfigPageRegistration(const char* stage,const String& field,const __FlashStringHelper* reason) {
  appConfigRegistrationFailed(stage,field,String(reason));finishAppConfigPageRegistration(false);
}
void ArdPortal::serviceAppConfigPageRegistration() {
  if(!_appConfigPageRegistration||_appConfigPageRegistrationDriving)return;
  _appConfigPageRegistrationDriving=true;
  const uint32_t generation=_appConfigPageRegistrationGeneration,start=millis();
  size_t unit=0;
  while(_appConfigPageRegistration&&generation==_appConfigPageRegistrationGeneration){
    if(!_appConfigPageRegistration->wholePass){
      if(unit>=_options.appConfigRegistrationMaxOperationsPerLoop)break;
      if(unit&&uint32_t(millis()-start)>=_options.appConfigRegistrationWorkBudgetMs)break;
    }
    serviceAppConfigPageRegistrationUnit();++unit;
    // Small definitions finish in this loop pass. A ninth field switches the
    // same job to budgeted work, preserving the public lifecycle in both cases.
    if(_appConfigPageRegistration&&generation==_appConfigPageRegistrationGeneration&&
       _appConfigPageRegistration->scanner.fieldCount>8)_appConfigPageRegistration->wholePass=false;
  }
  _appConfigPageRegistrationDriving=false;
}
void ArdPortal::serviceAppConfigPageRegistrationUnit() {
  if(!_appConfigPageRegistration)return;
  auto& work=*_appConfigPageRegistration;
  auto failed=[&](const char* stage,const String& field,const String& reason){appConfigRegistrationFailed(stage,field,reason);finishAppConfigPageRegistration(false);};
  if(work.stage==ArdAppConfigPageRegistration::Scan||work.stage==ArdAppConfigPageRegistration::Fields) {
    const bool scanning=work.stage==ArdAppConfigPageRegistration::Scan;
    auto source=work.reader();auto event=work.scanner.step(source,1024);
    if(event==ArdAppConfigPageScanner::Waiting)return;
    if(event==ArdAppConfigPageScanner::Failed){failed("parse","",work.scanner.reason);return;}
    if(event==ArdAppConfigPageScanner::Complete){
      if(scanning){
        const V& header=work.header;
        if(!work.scanner.fieldsSeen||!work.scanner.fieldCount){failAppConfigPageRegistration("page","",F("fields must be a nonempty array"));return;}
        if(!ArdDynamicPages::identifier(header["id"].asString())){failAppConfigPageRegistration("page","",F("invalid page id"));return;}
        if(!ArdDynamicPages::title(header)){failAppConfigPageRegistration("page","",F("invalid page name/names (1..80 bytes)"));return;}
        if(!header["order"].isInteger()){failAppConfigPageRegistration("page","",F("order must be an integer"));return;}
        for(size_t i=0;i<_dynamic.pages.length();++i)if(_dynamic.pages.id(i)==header["id"].asString()){failAppConfigPageRegistration("page","",F("duplicate page id"));return;}
        _appConfigPageRegistrationTotal=work.scanner.fieldCount;
        work.entry.reset(new(std::nothrow) ArdDynamicPages::PageStore::Entry());
        if(!work.entry){failAppConfigPageRegistration("registry","",F("out of memory allocating page"));return;}
        work.entry->count=work.scanner.fieldCount;
        work.entry->fields.reset(new(std::nothrow) ArdDynamicPages::PageStore::Field[work.entry->count]);
        if(!work.entry->fields){failAppConfigPageRegistration("registry","",F("out of memory allocating field index"));return;}
        work.entry->id=header["id"].asString();work.entry->sourceLength=work.length;work.entry->condition=header["visibleWhen"];
        if(work.entry->id!=header["id"].asString()||(header.hasOwnProperty("visibleWhen")&&!work.entry->condition.isValid())){failAppConfigPageRegistration("registry","",F("out of memory copying page metadata"));return;}
        V menu=V::object();for(const char* key:{"id","name","names","order","visibleWhen"})if(header.hasOwnProperty(key))menu[key]=header[key];
        ArdJSON::Limits limits;limits.maxNodes=4096;limits.escapeHtml=true;String reason;
        work.entry->menu=_json.stringify(menu,false,&reason,limits);if(!work.entry->menu.length()){failed("serialize","",reason);return;}
        work.stage=ArdAppConfigPageRegistration::Fields;work.scanner.reset();return;
      }
      work.stage=ArdAppConfigPageRegistration::Checks;work.checkIndex=0;return;
    }
    if(event==ArdAppConfigPageScanner::Member&&!scanning)return;
    if(event==ArdAppConfigPageScanner::Field&&scanning){
      if(work.first+work.scanner.fieldCount>ArdAppConfigMaxFields){failAppConfigPageRegistration("limits","",F("field limit: 1024"));return;}
      return; // Index pass parses each field only once, after its exact count is known.
    }
    String slice=source.slice(work.scanner.start,work.scanner.bytes);
    if(slice.length()!=work.scanner.bytes){failAppConfigPageRegistration("parse","",F("out of memory copying JSON slice"));return;}
    ArdJSON::Limits limits;limits.maxNodes=4096;limits.maxDepth=event==ArdAppConfigPageScanner::Field?14:15;
    ArdJSON::Parser parser(slice,limits);V value=parser.parse();
    if(parser.error()){failed("parse",event==ArdAppConfigPageScanner::Field?value["id"].asString():String(),String(parser.error())+" at byte "+String(work.scanner.start+parser.offset()));return;}
    if(scanning){
      work.nodes+=parser.nodes();if(work.nodes>4096){failAppConfigPageRegistration("parse","",F("node limit: 4096"));return;}
      if(event==ArdAppConfigPageScanner::Member){
        if(work.header.hasOwnProperty(work.scanner.key)){failed("parse","","duplicate root key: "+work.scanner.key);return;}
        work.header[work.scanner.key]=std::move(value);if(!work.header.isValid()){failAppConfigPageRegistration("parse","",F("out of memory storing page header"));return;}
      }else if(work.first+work.scanner.fieldCount>ArdAppConfigMaxFields){failed("limits",value["id"].asString(),"field limit: 1024");return;}
      return;
    }
    work.nodes+=parser.nodes();if(work.nodes>4096){failAppConfigPageRegistration("parse","",F("node limit: 4096"));return;}
    const size_t index=work.processed;const String id=static_cast<const V&>(value)["id"].asString();String reason;
    if(!ardPortalBasicControl(value["type"].asString())){
      V resolved=value;if(!ArdHa::normalize(resolved,false,&reason)){failed("normalize",id,reason);return;}
      if(!value.hasOwnProperty("extended"))++work.nodes;
      value["extended"]=true;if(!value.hasOwnProperty("persist")){++work.nodes;value["persist"]=resolved["persist"];}
      if(work.nodes>4096){failAppConfigPageRegistration("serialize",id,F("normalized page node limit: 4096"));return;}
    }
    if(!_dynamic.validateField(value,V::array(),false)){failed(_dynamic.error.stage,_dynamic.error.fieldId,_dynamic.error.reason);return;}
    for(size_t i=0;i<index;++i)if(id==work.idAt(i)){failAppConfigPageRegistration("field",id,F("duplicate field id in page"));return;}
#if ARDPORTAL_ENABLE_HA
    String discoveryError;String payload=_homeAssistant.discoveryConfig(value,&discoveryError);
    if(!payload.length()||payload.length()+400>ArdMqtt::PacketCapacity-5){failed("discovery",id,!payload.length()?discoveryError:String("Discovery payload exceeds MQTT packet capacity: payload=")+String(payload.length())+" reserve=400 capacity="+String(ArdMqtt::PacketCapacity-5));return;}
#endif
    // Only existing owners whose generated names might match this new ID need
    // parsing. Unrelated earlier pages/entities are never reparsed here.
    for(size_t pos=0;pos<id.length();++pos)if(id[pos]=='_'){
      size_t owner=_dynamic.indexOf(id.substring(0,pos));
      if(_dynamic.commandOwners.test(owner))work.existingCommands|=ArdAppConfigFieldMask::forField(owner);
    }
    auto& meta=work.entry->fields[index];
    const size_t idOffset=work.entry->ids.length(),idBytes=id.length()+1;
    // Binary String pool: append the terminator too. Grow in bounded blocks,
    // retaining at most 31 bytes of spare capacity, without per-field Strings.
    if(!work.entry->ids.reserve(((idOffset+idBytes+32)&~size_t(31))-1)||
       !work.entry->ids.concat(id.c_str(),idBytes)){
      failAppConfigPageRegistration("registry",id,F("out of memory growing field id pool"));return;
    }
    meta.idOffset=uint16_t(idOffset);
    meta.offset=uint32_t(work.scanner.start);meta.length=uint16_t(work.scanner.bytes);meta.index=uint16_t(work.first+index);
    const V& read=value;
    meta.extended=read["extended"].asBool();meta.persist=read["persist"].asBool();meta.transient=ArdHa::transient(value);
    if(!meta.transient)work.stateMask|=ArdAppConfigFieldMask::forField(meta.index);
    if(meta.extended||read["type"].asString()=="climate"){
      work.commandsMask|=ArdAppConfigFieldMask::forField(meta.index);
      work.checksMask|=ArdAppConfigFieldMask::forField(index);
    }
    if(value.hasOwnProperty("visibleWhen"))work.checksMask|=ArdAppConfigFieldMask::forField(index);
#if ARDPORTAL_ENABLE_DEPENDENCIES
    meta.dependent=value.hasOwnProperty("visibleWhen")||work.header.hasOwnProperty("visibleWhen");
    if(meta.dependent)work.dependencyMask|=ArdAppConfigFieldMask::forField(meta.index);
#endif
    size_t encoded=ArdJSON::JSON.measure(value,&reason,limits);if(!encoded){failed("serialize",id,reason);return;}
    work.encodedFields+=encoded+(index?1:0);++work.processed;_appConfigPageRegistrationProcessed=work.processed;return;
  }
#if ARDPORTAL_ENABLE_DEPENDENCIES
  auto conditionValid=[&](const V& condition){
    if(condition.isUndefined())return true;
    if(condition.type()!=V::Type::Object||!ArdDynamicPages::identifier(condition["field"].asString())||!condition.hasOwnProperty("equals"))return false;
    if(condition.hasOwnProperty("property")&&!ArdDynamicPages::identifier(condition["property"].asString()))return false;
    const V& expected=condition["equals"];if(expected.type()!=V::Type::String&&expected.type()!=V::Type::Boolean&&expected.type()!=V::Type::Number&&!expected.isNull())return false;
    String id=condition["field"].asString();return _dynamic.indexOf(id)<_dynamic.count()||work.indexOf(id)<work.entry->count;
  };
#endif
  if(work.stage==ArdAppConfigPageRegistration::Checks||work.stage==ArdAppConfigPageRegistration::ExistingCommands){
    const bool existing=work.stage==ArdAppConfigPageRegistration::ExistingCommands;
    auto& pending=existing?work.existingCommands:work.checksMask;
    work.checkIndex=pending.firstSet();
    if(work.checkIndex>=ArdAppConfigMaxFields){work.stage=existing?ArdAppConfigPageRegistration::Commit:ArdAppConfigPageRegistration::ExistingCommands;return;}
    V field=existing?_dynamic.at(work.checkIndex):work.field(work.checkIndex);
    if(field.isUndefined()||!field.isValid()){failed("parse",existing?_dynamic.idAt(work.checkIndex):String(work.idAt(work.checkIndex)),"cannot read indexed field (memory or JSON failure)");return;}
    String id=static_cast<const V&>(field)["id"].asString();
    if(!existing){
#if ARDPORTAL_ENABLE_DEPENDENCIES
      if(!conditionValid(static_cast<const V&>(field)["visibleWhen"])){failAppConfigPageRegistration("field",id,F("invalid visibleWhen condition or referenced field"));return;}
#else
      if(work.header.hasOwnProperty("visibleWhen")){failAppConfigPageRegistration("page","",F("dependencies are disabled"));return;}
#endif

    }
    V suffixes=V::array();
#if ARDPORTAL_ENABLE_CONTROL_CLIMATE
    if(field["type"].asString()=="climate")for(const char* suffix:{"temperature","mode","fan"})suffixes.push(suffix);
#endif
    if(ArdHa::extended(field)){String reason;V resolved=field;if(!ArdHa::normalize(resolved,true,&reason)){failed("commands",id,reason);return;}for(size_t i=0;i<resolved["controls"].length();++i)if(resolved["controls"][i]["command"].asString().length())suffixes.push(resolved["controls"][i]["command"]);suffixes.push("ack");}
    for(size_t i=0;i<suffixes.length();++i){String generated=id+"_"+suffixes[i].asString();if(work.indexOf(generated)<work.entry->count||(!existing&&_dynamic.indexOf(generated)<work.first)){failed("commands",id,"generated command/ack id collides with field: "+generated);return;}}
    pending&=~ArdAppConfigFieldMask::forField(work.checkIndex);return;
  }
#if ARDPORTAL_ENABLE_DEPENDENCIES
  if(!conditionValid(static_cast<const V&>(work.header)["visibleWhen"])){failAppConfigPageRegistration("page","",F("invalid page visibleWhen condition or referenced field"));return;}
#else
  if(work.header.hasOwnProperty("visibleWhen")){failAppConfigPageRegistration("page","",F("dependencies are disabled"));return;}
#endif
  // The only visibility change is this final move. No partial page is ever
  // available to HTTP, MQTT, dependency evaluation or Discovery.
  work.header["fields"]=V::array();ArdJSON::Limits limits;limits.maxNodes=4096;String reason;
  size_t headerBytes=ArdJSON::JSON.measure(work.header,&reason,limits);if(!headerBytes){failed("serialize","",reason);return;}
  work.entry->jsonLength=headerBytes+work.encodedFields;
  work.header.remove("fields");
  work.entry->headerPrefix=_json.stringify(work.header,false,&reason,limits);
  if(!work.entry->headerPrefix.length()){failed("serialize","",reason);return;}
  work.entry->headerPrefix.remove(work.entry->headerPrefix.length()-1);
  work.entry->headerPrefix+=F(",\"fields\":[");
  if(work.entry->headerPrefix.length()!=headerBytes-2){failAppConfigPageRegistration("serialize","",F("out of memory storing page header"));return;}
  limits.maxOutputBytes=ArdAppConfigMaxDefinitionBytes;
  size_t existingBytes=_dynamic.pages.measure(limits);
  if(!existingBytes||existingBytes+work.entry->jsonLength+(_dynamic.pages.length()?1:0)+_dynamic.entityBytes>ArdAppConfigMaxDefinitionBytes){failed("limits","",String("total definition limit: ")+String(ArdAppConfigMaxDefinitionBytes)+" bytes");return;}
  work.entry->flash=work.flash;work.entry->source=std::move(work.source);
  const size_t first=work.first;_dynamic.commandOwners|=work.commandsMask;_dynamic.totalFields+=work.entry->count;_dynamic.pages.commit(std::move(work.entry));
#if ARDPORTAL_ENABLE_HA
  _homeAssistant.registered(first,work.stateMask,work.dependencyMask);
#elif ARDPORTAL_ENABLE_MQTT
  (void)first;_appControls._stateDirty|=work.stateMask;
#else
  (void)first;
#endif
  finishAppConfigPageRegistration(true);
}
