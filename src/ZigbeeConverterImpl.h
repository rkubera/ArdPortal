// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ZigbeeConverterLayout.h"
#if ARDPORTAL_ZIGBEE_CONVERTER_SUPPORT
#include "ZigbeeConverterAssets.h"
#endif
#if ARDPORTAL_ZHA_QUIRK_SUPPORT
#include "ZhaQuirkAssets.h"
#endif

/** @brief Describe one registered endpoint without retaining a complete page or converter.
 * @param c Immutable endpoint mapping.
 * @return JSON metadata or empty on allocation failure.
 */
String ArdPortal::zigbeeConverterControlJson(const ArdZigbee::Control& c){
  using V=ArdJSON::JSONVar;
  using Kind=ArdZigbeeFields::Kind;
  const auto& spec=c.spec;
  if(!ArdHeap::permits(_dynamic.fieldWork(spec.field)+4096,_dynamic.fieldBlock(spec.field)))return String();
  V field=_dynamic.at(spec.field);if(!field.isValid()||field.isUndefined())return String();
  if(ArdZigbeeFields::composite(field)&&!ArdHa::normalize(field,true))return String();
  const char* kinds[]={"none","switch","slider","select","action","state","number","temperature","humidity","pressure","illuminance","flow","carbon_dioxide","pm25","wind_speed","voltage","current","power","binary","light","climate","cover","lock","siren","update"};
  V row=V::object();row["schema"]=1;row["kind"]=kinds[unsigned(spec.kind)];row["ep"]=int(spec.endpoint);
  String endpoint=field["id"].asString()+"_ep"+String(spec.endpoint);
  row["endpoint"]=endpoint;
  // Labels are startup-only in the SDK registry; derive identical metadata before and after startup.
  String label=field["name"].asString();if(!label.length())label=field["id"].asString();
  if(spec.part!=0xffff){
    String suffix;
    if(spec.part==0xfffe)suffix=spec.kind==Kind::State?"state":"current";
    else{const V& part=static_cast<const V&>(field)["controls"][size_t(spec.part)];suffix=part["key"].asString();if(!suffix.length())suffix=part["payload"].asString();}
    label+=" / ";label+=suffix;
  }
  size_t labelBytes=std::min(size_t(63),label.length());
  if(labelBytes<label.length())while(labelBytes&&(uint8_t(label[labelBytes])&0xc0)==0x80)--labelBytes;
  row["label"]=label.substring(0,labelBytes);
  if(spec.kind==Kind::Slider||spec.kind==Kind::Climate){row["min"]=spec.min;row["max"]=spec.max;row["step"]=spec.step;}
  if(spec.kind==Kind::Select||spec.kind==Kind::State){
    V options=V::array();
    if(spec.kind==Kind::State){for(uint16_t i=1;i<=spec.states;++i)if(!options.push(ArdZigbeeFields::stateOption(field,String(),i)))return String();}
    else{
      const V& source=spec.part==0xffff?static_cast<const V&>(field):static_cast<const V&>(field)["controls"][size_t(spec.part)];
      for(size_t i=0;i<source["options"].length();++i){const V& option=source["options"][i];if(!options.push(option.type()==V::Type::String?option:option["value"]))return String();}
    }
    row["options"]=std::move(options);
  }
  if(spec.kind==Kind::Climate){row["modes"]=static_cast<const V&>(field)["modes"];if(spec.fan)row["fanModes"]=static_cast<const V&>(field)["fan_modes"];}
  if(!row.isValid())return String();
  return _json.stringify(row);
}

/** @brief Queue one converter fragment, then its flash-resident implementation.
 * @return True when output is ready; false to retry after memory pressure.
 */
#if ARDPORTAL_ZIGBEE_HTTP_EXPORT_SUPPORT
bool ArdPortal::prepareZigbeeConverterPart(){
  if(!_converterControl){
    _response="\n";
#if ARDPORTAL_ZHA_QUIRK_SUPPORT
    if(_converterPython)_page=ARD_ZHA_QUIRK_TAIL;else
#endif
    {
#if ARDPORTAL_ZIGBEE_CONVERTER_SUPPORT
      _page=ARD_ZIGBEE_CONVERTER_TAIL;
#endif
    }
    _pageLength=strlen_P(_page);_pageOffset=0;
    _converterDownload=false;return true;
  }
  String text=zigbeeConverterControlJson(*_converterControl);if(!text.length())return false;
  String fragment;if(!fragment.reserve(text.length()+3))return false;
  if(_converterStage)fragment+=",";
#if ARDPORTAL_ZHA_QUIRK_SUPPORT
  if(_converterPython){
    String encoded=_json.stringify(ArdJSON::JSONVar(text));if(!encoded.length())return false;
    if(!fragment.reserve(fragment.length()+encoded.length()+14))return false;
    fragment+=F("json.loads(");fragment+=encoded;fragment+=")\n";
  }else
#endif
  {fragment+=text;fragment+="\n";}
  _response=std::move(fragment);_converterStage=1;_converterControl=_converterControl->next.get();return true;
}

#endif
