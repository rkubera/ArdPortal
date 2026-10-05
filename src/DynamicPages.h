// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ArdJSON.h"
#include "HaEntities.h"
#include "JsonCodec.h"

// Definitions are immutable after registration. IDs are also MQTT command names.
class ArdDynamicPages {
public:
  using V = ArdJSON::JSONVar;
  using T = V::Type;
  // Keep only a compact index and the source. Parsed pages are temporary values.
  class PageStore {
    struct Field {String id;bool extended=false,persist=false,transient=false;
#if ARDPORTAL_ENABLE_DEPENDENCIES
    bool dependent=false;
#endif
    };
    struct Entry {String id,source,menu;V condition;const __FlashStringHelper* flash=nullptr;std::unique_ptr<Field[]> fields;size_t count=0,jsonLength=0;};
    std::unique_ptr<Entry> entries[16];size_t size=0;
  public:
    class ReadScope;
  private:
    mutable ReadScope* scope=nullptr;
  public:
    ArdJsonCodec codec;
    size_t length() const {return size;}
    const String& id(size_t index) const {return entries[index]->id;}
    size_t fields(size_t index) const {return entries[index]->count;}
    const String& fieldId(size_t p,size_t f) const {return entries[p]->fields[f].id;}
#if ARDPORTAL_ENABLE_DEPENDENCIES
    bool dependent(size_t p,size_t f) const {return entries[p]->fields[f].dependent;}
#endif
    bool transient(size_t p,size_t f) const {return entries[p]->fields[f].transient;}
    bool belongs(size_t index,const String& key) const {for(size_t f=0;f<fields(index);++f)if(entries[index]->fields[f].id==key)return true;return false;}
    bool tryPush(const V& page) {
      if(size>=16)return false;
      std::unique_ptr<Entry> entry(new(std::nothrow) Entry());if(!entry)return false;
      entry->id=page["id"].asString();entry->count=page["fields"].length();
      entry->fields.reset(new(std::nothrow) Field[entry->count]);if(!entry->fields)return false;
      for(size_t i=0;i<entry->count;++i){const V& f=page["fields"][i];entry->fields[i].id=f["id"].asString();if(entry->fields[i].id!=f["id"].asString())return false;entry->fields[i].extended=f["extended"].asBool();entry->fields[i].persist=f["persist"].asBool();entry->fields[i].transient=ArdHa::transient(f);
#if ARDPORTAL_ENABLE_DEPENDENCIES
        entry->fields[i].dependent=page.hasOwnProperty("visibleWhen")||f.hasOwnProperty("visibleWhen");
#endif
      }
      ArdJSON::Limits limits;limits.maxNodes=4096;entry->source=codec.stringify(page,false,nullptr,limits);
      if(entry->id!=page["id"].asString()||!entry->source.length())return false;
      entry->jsonLength=entry->source.length();
      V menu=V::object();for(const char* key:{"id","name","names","order","visibleWhen"})if(page.hasOwnProperty(key))menu[key]=page[key];
      limits.escapeHtml=true;entry->menu=codec.stringify(menu,false,nullptr,limits);entry->condition=page["visibleWhen"];
      if(!entry->menu.length())return false;
      entries[size++]=std::move(entry);return true;
    }
    void remove(size_t index) {if(index>=size)return;for(size_t i=index;i+1<size;++i)entries[i]=std::move(entries[i+1]);entries[--size].reset();}
    void useFlash(size_t index,const __FlashStringHelper* source) {entries[index]->flash=source;entries[index]->source=String();}
    bool flashBacked(size_t index) const {return entries[index]->flash!=nullptr;}
    size_t ownedSourceBytes() const {size_t bytes=0;for(size_t i=0;i<size;++i)bytes+=entries[i]->source.length();return bytes;}
    V parsePage(size_t index) const {
      if(index>=size)return V();
      ArdJSON::Limits limits;limits.maxNodes=4096;limits.maxInputBytes=32768;
      const Entry& entry=*entries[index];V page=codec.parse(entry.flash?String(entry.flash):entry.source,nullptr,limits);
      if(!page.isValid()||page["fields"].length()!=entry.count)return V();
      for(size_t f=0;f<entry.count;++f)if(entry.fields[f].extended){page["fields"][f]["extended"]=true;if(!page["fields"][f].hasOwnProperty("persist"))page["fields"][f]["persist"]=entry.fields[f].persist;}
      return page;
    }
    // Request-local trees are destroyed before any HTTP transmission begins.
    class ReadScope {
      PageStore& store;ReadScope* previous;
      std::unique_ptr<V> cache[16];bool tried[16]={};V missing;
    public:
      explicit ReadScope(PageStore& value):store(value),previous(value.scope){store.scope=this;}
      ~ReadScope(){store.scope=previous;}
      const V& get(size_t index){if(index>=store.length())return missing;if(!tried[index]){tried[index]=true;cache[index].reset(new(std::nothrow) V(store.parsePage(index)));}return cache[index]?*cache[index]:missing;}
      void release(size_t index){cache[index].reset();}
      ReadScope(const ReadScope&)=delete;ReadScope& operator=(const ReadScope&)=delete;
    };
    const V* cachedPage(size_t index) const {return scope?&scope->get(index):nullptr;}
    V operator[](size_t index) const {if(const V* page=cachedPage(index))return *page;return parsePage(index);}
    const String& menu(size_t index) const {return entries[index]->menu;}
    const V& pageCondition(size_t index) const {return entries[index]->condition;}
    size_t menuLength() const {size_t n=2;for(size_t p=0;p<size;++p)n+=entries[p]->menu.length()+(p?1:0);return n;}
    size_t measure(const ArdJSON::Limits& limits,String* error=nullptr) const {
      size_t bytes=2;for(size_t i=0;i<size;++i){size_t length=entries[i]->jsonLength;if(limits.escapeHtml||limits.maxNodes!=4096){V page=(*this)[i];length=ArdJSON::JSON.measure(page,error,limits);}if(!length)return 0;bytes+=length+(i?1:0);yield();}if(bytes>limits.maxOutputBytes){if(error)*error="output limit";return 0;}return bytes;
    }
  };
  PageStore pages;
  static bool identifier(const String& id) {
    if (!id.length() || id.length()>48) return false;
    for(size_t i=0;i<id.length();++i) { char c=id[i]; if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-')) return false; }
    return true;
  }
  static bool names(const V& v) {
    if(v.type()!=T::Object || !v.length()) return false;
    V keys=v.keys();
    for(size_t i=0;i<keys.length();++i) { const V& label=v[keys[i].asString()]; if(label.type()!=T::String || !label.asString().length() || label.asString().length()>80) return false; }
    return true;
  }
  static bool title(const V& v) {
    if(v.hasOwnProperty("name")&&(v["name"].type()!=T::String||!v["name"].asString().length()||v["name"].asString().length()>80)) return false;
    if(v.hasOwnProperty("names")&&!names(v["names"])) return false;
    return v.hasOwnProperty("name")||v.hasOwnProperty("names");
  }
  static V inheritCondition(V& page,size_t index) {
#if ARDPORTAL_ENABLE_DEPENDENCIES
    if(page.hasOwnProperty("visibleWhen")) page["fields"][index]["_pageVisibleWhen"]=page["visibleWhen"];
#endif
    return std::move(page["fields"][index]);
  }
#if ARDPORTAL_ENABLE_DEPENDENCIES
  bool validCondition(const V& condition,const V& fields) const {
    if(condition.type()!=T::Object||!identifier(condition["field"].asString())||!condition.hasOwnProperty("equals"))return false;
    if(condition.hasOwnProperty("property")&&!identifier(condition["property"].asString()))return false;
    const V& expected=condition["equals"];
    if(expected.type()!=T::String&&expected.type()!=T::Boolean&&expected.type()!=T::Number&&!expected.isNull())return false;
    if(!field(condition["field"].asString()).isUndefined())return true;
    for(size_t i=0;i<fields.length();++i)if(fields[i]["id"].asString()==condition["field"].asString())return true;
    return false;
  }
#endif
  V field(const String& id) const {
    for(size_t p=0;p<pages.length();++p)if(pages.belongs(p,id)){if(const V* cached=pages.cachedPage(p)){for(size_t f=0;f<(*cached)["fields"].length();++f)if((*cached)["fields"][f]["id"].asString()==id){V field=(*cached)["fields"][f];if(cached->hasOwnProperty("visibleWhen"))field["_pageVisibleWhen"]=(*cached)["visibleWhen"];return field;}return V();}V page=pages[p];for(size_t f=0;f<page["fields"].length();++f)if(page["fields"][f]["id"].asString()==id)return inheritCondition(page,f);}
    return V();
  }
  size_t count() const {size_t n=0;for(size_t p=0;p<pages.length();++p)n+=pages.fields(p);return n;}
  V at(size_t index) const {
    for(size_t p=0;p<pages.length();++p){size_t n=pages.fields(p);if(index<n){if(const V* cached=pages.cachedPage(p)){V field=(*cached)["fields"][index];if(cached->hasOwnProperty("visibleWhen"))field["_pageVisibleWhen"]=(*cached)["visibleWhen"];return field;}V page=pages[p];return inheritCondition(page,index);}index-=n;}
    return V();
  }
  String idAt(size_t index) const {
    for(size_t p=0;p<pages.length();++p){size_t n=pages.fields(p);if(index<n)return pages.fieldId(p,index);index-=n;}
    return String();
  }
#if ARDPORTAL_ENABLE_DEPENDENCIES
  bool dependentAt(size_t index) const {
    for(size_t p=0;p<pages.length();++p){size_t n=pages.fields(p);if(index<n)return pages.dependent(p,index);index-=n;}
    return false;
  }
#endif
  bool transientAt(size_t index) const {
    for(size_t p=0;p<pages.length();++p){size_t n=pages.fields(p);if(index<n)return pages.transient(p,index);index-=n;}
    return true;
  }
  static bool contains(const V& values,const String& value) {
    for(size_t i=0;i<values.length();++i) if((values[i].type()==T::String?values[i].asString():values[i]["value"].asString())==value) return true;
    return false;
  }
  static V initial(const V& f) {
    if(ArdHa::extended(f)) {V resolved=f;if(!ArdHa::normalize(resolved,true)) return V();return resolved["default"];}
    if(!f["default"].isUndefined()) return f["default"];
    String type=f["type"].asString();
#if ARDPORTAL_ENABLE_CONTROL_SLIDER
    if(type=="slider") return f["min"];
#endif
#if ARDPORTAL_ENABLE_CONTROL_SWITCH
    if(type=="switch") return V(false);
#endif
#if ARDPORTAL_ENABLE_CONTROL_SELECT
    if(type=="select") return f["options"][0]["value"];
#endif
#if ARDPORTAL_ENABLE_CONTROL_CLIMATE
    if(type=="climate") { V v=V::object(); v["temperature"]=f["min"]; v["current_temperature"]=nullptr; if(f.hasOwnProperty("modes")) v["mode"]=f["modes"][0]; if(f.hasOwnProperty("fan_modes")) v["fan_mode"]=f["fan_modes"][0]; return v; }
#endif
    return V("");
  }
  static bool validValue(const V& f,const V& v) {
    String type=f["type"].asString();
    if(ArdHa::extended(f)) {V resolved=f;return ArdHa::normalize(resolved,true)&&ArdHa::validValue(resolved,v);}
#if ARDPORTAL_ENABLE_CONTROL_SWITCH
    if(type=="switch") return v.type()==T::Boolean;
#endif
#if ARDPORTAL_ENABLE_CONTROL_SLIDER
    if(type=="slider") { double n; if(!v.toDouble(n) || n<f["min"].asDouble() || n>f["max"].asDouble()) return false; double step=f["step"].isUndefined()?1:f["step"].asDouble(); double q=(n-f["min"].asDouble())/step; return fabs(q-round(q))<0.000001; }
#endif
#if ARDPORTAL_ENABLE_CONTROL_SELECT
    if(type=="select") return v.type()==T::String && contains(f["options"],v.asString());
#endif
#if ARDPORTAL_ENABLE_CONTROL_CLIMATE
    if(type=="climate") {
      if(v.type()!=T::Object) return false;
      double n; if(!v["temperature"].toDouble(n) || n<f["min"].asDouble() || n>f["max"].asDouble()) return false;
      double step=f["step"].isUndefined()?0.5:f["step"].asDouble(); if(fabs((n-f["min"].asDouble())/step-round((n-f["min"].asDouble())/step))>0.000001) return false;
      if(!v["current_temperature"].isUndefined() && !v["current_temperature"].isNull() && !v["current_temperature"].toDouble(n)) return false;
      if(f.hasOwnProperty("modes") ? !contains(f["modes"],v["mode"].asString()) : !v["mode"].isUndefined()) return false;
      if(f.hasOwnProperty("fan_modes") ? !contains(f["fan_modes"],v["fan_mode"].asString()) : !v["fan_mode"].isUndefined()) return false;
      if(!v["action"].isUndefined()&&(v["action"].type()!=T::String||v["action"].asString().length()>32)) return false;
      V keys=v.keys(); for(size_t i=0;i<keys.length();++i) { String k=keys[i].asString(); if(k!="temperature"&&k!="current_temperature"&&k!="mode"&&k!="fan_mode"&&k!="action") return false; }
      return true;
    }
#endif
#if ARDPORTAL_ENABLE_CONTROL_TEXT
    if(type=="text") {double number;return v.toDouble(number)||(v.type()==T::String&&v.asString().length()<=512&&strlen(v.asString().c_str())==v.asString().length());}
#endif
#if ARDPORTAL_ENABLE_CONTROL_EDIT
    if(type=="edit") return v.type()==T::String&&v.asString().length()<=128&&strlen(v.asString().c_str())==v.asString().length();
#endif
    return false;
  }
  bool add(const V& page) {
    if(pages.length()>=16 || page.type()!=T::Object || !identifier(page["id"].asString()) || !title(page) || !page["order"].isInteger() || page["fields"].type()!=T::Array || !page["fields"].length() || count()+page["fields"].length()>48) return false;
    for(size_t p=0;p<pages.length();++p) if(pages.id(p)==page["id"].asString()) return false;
    const V& fields=page["fields"];
#if ARDPORTAL_ENABLE_DEPENDENCIES
    if(page.hasOwnProperty("visibleWhen")&&!validCondition(page["visibleWhen"],fields))return false;
#else
    if(page.hasOwnProperty("visibleWhen"))return false;
#endif
    for(size_t i=0;i<fields.length();++i) {
      const V& f=fields[i]; String id=f["id"].asString(),type=f["type"].asString();
      if(id=="availability" || id=="status" || !identifier(id) || !field(id).isUndefined() || !title(f)) return false;
      for(size_t j=0;j<i;++j) if(fields[j]["id"].asString()==id) return false;
      if(!ardPortalBasicControl(type)&&(!ArdHa::extended(f)||ArdHa::descriptor(type,true).isUndefined())) return false;
      if(f.hasOwnProperty("ha")&&f["ha"].type()!=T::Object) return false;
      if(f.hasOwnProperty("icon")) { String icon=f["icon"].asString(); if(icon.indexOf("mdi:")!=0 || icon.length()>80 || icon.length()<5) return false; }
#if (ARDPORTAL_ENABLE_CONTROL_SLIDER || ARDPORTAL_ENABLE_CONTROL_CLIMATE)
    if(type=="slider" || type=="climate") { double lo,hi,step; if(!f["min"].toDouble(lo)||!f["max"].toDouble(hi)||lo>=hi) return false; if(f.hasOwnProperty("step")&&(!f["step"].toDouble(step)||step<=0)) return false; }
#endif
#if ARDPORTAL_ENABLE_CONTROL_SELECT
    if(type=="select") { const V& o=f["options"]; if(o.type()!=T::Array||!o.length()||o.length()>16) return false; for(size_t j=0;j<o.length();++j) { if(o[j]["value"].type()!=T::String || o[j]["value"].asString().length()>128 || strlen(o[j]["value"].asString().c_str())!=o[j]["value"].asString().length() || !title(o[j])) return false; for(size_t k=0;k<j;++k) if(o[j]["value"].asString()==o[k]["value"].asString()) return false; } }
#endif
#if ARDPORTAL_ENABLE_CONTROL_CLIMATE
    if(type=="climate") for(const char* key:{"modes","fan_modes"}) if(f.hasOwnProperty(key)) { const V& modes=f[key]; if(modes.type()!=T::Array||!modes.length()||modes.length()>10) return false; for(size_t j=0;j<modes.length();++j) { String m=modes[j].asString(); if(!identifier(m)) return false; if(String(key)=="modes"&&m!="off"&&m!="heat"&&m!="cool"&&m!="auto"&&m!="dry"&&m!="fan_only") return false; for(size_t k=0;k<j;++k) if(m==modes[k].asString()) return false; } }
#endif
#if ARDPORTAL_ENABLE_DEPENDENCIES
      if(f.hasOwnProperty("visibleWhen")&&!validCondition(f["visibleWhen"],fields))return false;
#else
      // Reject unsupported dependencies rather than exposing conditional fields.
      if(f.hasOwnProperty("visibleWhen")) return false;
#endif
      if(!validValue(f,initial(f))) return false;
    }
    if(!pages.tryPush(page)) return false;
    PageStore& next=pages;
    struct Rollback {PageStore& pages;bool keep=false;~Rollback(){if(!keep) pages.remove(pages.length()-1);}} rollback{pages};
    ArdJSON::Limits limits; limits.maxNodes=4096; limits.maxOutputBytes=32768;
    if(!next.measure(limits)) return false;
    // Every generated command and acknowledgement name must be unambiguous.
    for(size_t p=0;p<next.length();++p) {V definition=next[p];for(size_t i=0;i<definition["fields"].length();++i) {
      const V& f=static_cast<const V&>(definition)["fields"][i];V suffixes=V::array();
#if ARDPORTAL_ENABLE_CONTROL_CLIMATE
    if(f["type"].asString()=="climate") for(const char* suffix:{"temperature","mode","fan"}) suffixes.push(suffix);
#endif
      if(ArdHa::extended(f)) {V resolved=f;if(!ArdHa::normalize(resolved,true)) return false;for(size_t c=0;c<resolved["controls"].length();++c) if(resolved["controls"][c]["command"].asString().length()) suffixes.push(resolved["controls"][c]["command"]);suffixes.push("ack");}
      for(size_t k=0;k<suffixes.length();++k) for(size_t q=0;q<next.length();++q) if(next.belongs(q,f["id"].asString()+"_"+suffixes[k].asString())) return false;
      yield();
    }}
    rollback.keep=true; return true;
  }
};
