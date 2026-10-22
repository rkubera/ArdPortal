// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "LanguageLiterals.h"
/** @brief Read export admission without consulting HTTP state. @return True for a frozen, successfully started registry. */
bool ArdPortal::zigbeeConverterExportReady()const{
  return _configurationReady&&!_rebootPending&&!otaActive()&&_converterLayoutReady&&_zigbee.nativeReady()&&_zigbeeBeginTried&&!_zigbeeMappingFailed&&!_appConfigPageRegistration&&_zigbeeSeenFields==_dynamic.count()&&_zigbeeBuildIndex==_dynamic.count();
}
/** @brief Stage one bounded asynchronous export. @param python Python or JavaScript. @param writer Consumer. @param finished Completion handler. @return True on admission. */
bool ArdPortal::beginZigbeeConverterExport(bool python,ZigbeeExportWriter writer,ZigbeeExportFinished finished){
  if(!writer||zigbeeConverterExportBusy()||!zigbeeConverterExportReady())return false;
  _apiExportPython=python;_apiExportWriter=std::move(writer);_apiExportFinished=std::move(finished);
  _apiExportControl=_zigbee.controls();_apiExportOffset=0;_apiExportFirst=true;_apiExportCancel=false;
  _apiExportText=String();_apiExportStage=1;_apiExportProgressAt=millis();return true;
}
#if ARDPORTAL_ZIGBEE_CONVERTER_SUPPORT
/** @brief Start JavaScript streaming to an application consumer. @param writer Chunk handler. @param finished Final result. @return True if queued. */
bool ArdPortal::startZigbee2MqttConverterExport(ZigbeeExportWriter writer,ZigbeeExportFinished finished){return beginZigbeeConverterExport(false,std::move(writer),std::move(finished));}
#endif
#if ARDPORTAL_ZHA_QUIRK_SUPPORT
/** @brief Start Python streaming to an application consumer. @param writer Chunk handler. @param finished Final result. @return True if queued. */
bool ArdPortal::startZhaQuirkExport(ZigbeeExportWriter writer,ZigbeeExportFinished finished){return beginZigbeeConverterExport(true,std::move(writer),std::move(finished));}
#endif
/** @brief Clear owned buffers before invoking completion, permitting a new export. @param success Result. @param error Explanation. @return No value. */
void ArdPortal::finishZigbeeConverterExport(bool success,const String& error){
  auto finished=std::move(_apiExportFinished);_apiExportWriter=nullptr;_apiExportText=String();_apiExportControl=nullptr;_apiExportStage=0;_apiExportCancel=false;
  if(finished)finished(success,error);
}
/** @brief Emit one bounded chunk without a complete-file RAM buffer. @return No value. */
void ArdPortal::serviceZigbeeConverterExport(){
  if(!zigbeeConverterExportBusy())return;
  if(_apiExportCancel){finishZigbeeConverterExport(false,ARD_LANG_NATIVE_CANCELLED);return;}
  if(!zigbeeConverterExportReady()){finishZigbeeConverterExport(false,ARD_LANG_NATIVE_ENDPOINT_LAYOUT_CHANGED_OR_PORTAL_UNAVAILABLE);return;}
  if(uint32_t(millis()-_apiExportProgressAt)>=30000){finishZigbeeConverterExport(false,ARD_LANG_NATIVE_EXPORT_STALLED_FOR_30_SECONDS);return;}
  if(_apiExportStage==1&& !_apiExportText.length()){
    String raw(ArdZigbeeIdentity::Model+1);if(raw.length()!=strlen(ArdZigbeeIdentity::Model+1))return;
    ArdJSON::JSONVar value(raw);if(!value.isValid())return;
    String model=_json.stringify(value);if(model.length()<2||model[0]!='"')return;
    String header;if(!header.reserve(model.length()+256))return;
    if(_apiExportPython){header="import json\nMODEL = ";header+=model;header+="\nFIELDS = [\n";}
    else{header="import * as m from 'zigbee-herdsman-converters/lib/modernExtend';\nimport {presets as e, access as a} from 'zigbee-herdsman-converters/lib/exposes';\nconst identity={model:";header+=model;header+="};\nconst fields=[\n";}
    _apiExportText=std::move(header);
  }
  if(_apiExportStage==2&&!_apiExportText.length()){
    if(!_apiExportControl){_apiExportStage=3;_apiExportOffset=0;}
    else{
      String row=zigbeeConverterControlJson(*_apiExportControl);if(!row.length())return;
      String fragment;
      if(_apiExportPython){String encoded=_json.stringify(ArdJSON::JSONVar(row));if(!encoded.length())return;if(!fragment.reserve(encoded.length()+17))return;if(!_apiExportFirst)fragment+=",";fragment+="json.loads(";fragment+=encoded;fragment+=")\n";}
      else{if(!fragment.reserve(row.length()+3))return;if(!_apiExportFirst)fragment+=",";fragment+=row;fragment+="\n";}
      _apiExportText=std::move(fragment);
    }
  }
  if(_apiExportStage==3){
    const char* tail=nullptr;
#if ARDPORTAL_ZHA_QUIRK_SUPPORT
    if(_apiExportPython)tail=ARD_ZHA_QUIRK_TAIL;
#endif
#if ARDPORTAL_ZIGBEE_CONVERTER_SUPPORT
    if(!_apiExportPython)tail=ARD_ZIGBEE_CONVERTER_TAIL;
#endif
    size_t length=strlen_P(tail);if(_apiExportOffset==length){finishZigbeeConverterExport(true,String());return;}
    char chunk[256];size_t bytes=std::min(sizeof(chunk),length-_apiExportOffset);memcpy_P(chunk,tail+_apiExportOffset,bytes);
    if(_apiExportWriter(chunk,bytes)){_apiExportOffset+=bytes;_apiExportProgressAt=millis();}return;
  }
  size_t bytes=std::min(size_t(256),_apiExportText.length()-_apiExportOffset);
  if(!_apiExportWriter(_apiExportText.c_str()+_apiExportOffset,bytes))return;
  _apiExportOffset+=bytes;_apiExportProgressAt=millis();
  if(_apiExportOffset==_apiExportText.length()){
    _apiExportText=String();_apiExportOffset=0;
    if(_apiExportStage==1)_apiExportStage=2;
    else{_apiExportControl=_apiExportControl->next.get();_apiExportFirst=false;}
  }
}
