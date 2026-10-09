// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "AppConfigFields.h"
#include "AppConfigRegistrationError.h"
#include "ArdJSON.h"
#include "HaEntities.h"
#include "JsonCodec.h"
#include "JsonFieldSlices.h"
#include "DependencyConditions.h"
#include "HeapMemory.h"
#include "TopicTemplates.h"

// Definitions are immutable after registration. IDs are also MQTT command names.
class ArdDynamicPages {
public:
  ArdAppConfigFieldMask commandOwners;
  ArdAppConfigRegistrationError error;
  /**
   * @brief Record the parser or writer failure and stop further processing.
   * @param stage Registration stage recorded in the error report.
   * @param field Application field definition or identifier.
   * @param reason Restart or failure reason.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool fail(const char* stage,const String& field,const String& reason) {error={stage,field,reason};return false;}
  using V = ArdJSON::JSONVar;
  using T = V::Type;
  // Keep only a compact index and the source. Parsed pages are temporary values.
  class PageStore {
  public:
    struct Field { ARDPORTAL_NO_THROW_ALLOCATION uint32_t offset=0;uint16_t length=0,index=0,idOffset=0;bool extended:1,persist:1,transient:1;uint8_t sourceNodes=0;
#if ARDPORTAL_ENABLE_DEPENDENCIES
    bool dependent:1;
#endif
      /**
       * @brief Initialize this instance and its owned state.
       * @return No value.
       */
      Field():extended(false),persist(false),transient(false)
#if ARDPORTAL_ENABLE_DEPENDENCIES
      ,dependent(false)
#endif
      {}
    };
    struct Entry { ARDPORTAL_NO_THROW_ALLOCATION
#if ARDPORTAL_ENABLE_DEPENDENCIES
    ArdDependencies::Edges edges;
#endif
    String id,source,menu,headerPrefix,ids,condition;const __FlashStringHelper* flash=nullptr;std::unique_ptr<Field[]> fields;size_t count=0,jsonLength=0,sourceLength=0;};
  private:
    std::unique_ptr<Entry> entries[16];size_t size=0;
  public:
    ArdJsonCodec codec;
    String topicDevice;
    /**
     * @brief Read the number of elements in this JSON container.
     * @return Number of members or elements in the JSON container.
     */
    size_t length() const {return size;}
    /**
     * @brief Read the indexed field identifier from its owning page.
     * @param index Zero-based element or field index.
     * @return Reference to the requested stored value or component.
     */
    const String& id(size_t index) const {return entries[index]->id;}
    /**
     * @brief Read the number of registered application fields.
     * @param index Zero-based element or field index.
     * @return The number of registered application fields.
     */
    size_t fields(size_t index) const {return entries[index]->count;}
    /** @brief Estimate temporary memory for an indexed field without parsing it.
     * @param index Global field index.
     * @return Estimated parse/output memory, or zero for an unknown index. */
    size_t fieldWork(size_t index) const {for(size_t p=0;p<size;++p){const auto& e=*entries[p];if(e.count&&index>=e.fields[0].index&&index-e.fields[0].index<e.count){const auto& field=e.fields[index-e.fields[0].index];return ArdHeap::fieldWork(field.length,field.extended?0:field.sourceNodes,V::memberAllocationBytes());}}return 0;}

    /** @brief Largest single allocation for a field; separate from aggregate working memory. */
    size_t fieldBlock(size_t index) const {for(size_t p=0;p<size;++p){const auto& e=*entries[p];if(e.count&&index>=e.fields[0].index&&index-e.fields[0].index<e.count){const auto& field=e.fields[index-e.fields[0].index];return !field.sourceNodes?ArdHeap::fieldWork(field.length):ArdHeap::fieldBlock(field.length);}}return 0;}
    /**
     * @brief Read a field identifier from its normalized definition.
     * @param p Source or destination byte pointer.
     * @param f Field definition to inspect or normalize.
     * @return Pointer to the requested data, or nullptr when unavailable.
     */
    const char* fieldId(size_t p,size_t f) const {return entries[p]->ids.c_str()+entries[p]->fields[f].idOffset;}
#if ARDPORTAL_ENABLE_DEPENDENCIES
    /**
     * @brief Mark or test a field in the dependency mask.
     * @param p Source or destination byte pointer.
     * @param f Field definition to inspect or normalize.
     * @return True on success; false if validation, resource allocation or the operation fails.
     */
    bool dependent(size_t p,size_t f) const {return entries[p]->fields[f].dependent;}
#endif
    /**
     * @brief Check whether the field uses a transient event instead of retained state.
     * @param p Source or destination byte pointer.
     * @param f Field definition to inspect or normalize.
     * @return True if the field uses a transient event instead of retained state; false otherwise.
     */
    bool transient(size_t p,size_t f) const {return entries[p]->fields[f].transient;}
    /**
     * @brief Check whether the indexed field belongs to the supplied page.
     * @param index Zero-based element or field index.
     * @param key Configuration key or JSON object member name.
     * @return True if the indexed field belongs to the supplied page; false otherwise.
     */
    bool belongs(size_t index,const String& key) const {for(size_t f=0;f<fields(index);++f)if(key==fieldId(index,f))return true;return false;}
    /**
     * @brief Atomically append the validated page and its field indexes to the registry.
     * @param entry Indexed page, field or journal entry to process.
     * @return No value.
     */
#if ARDPORTAL_ENABLE_DEPENDENCIES
    /**
     * @brief Collect immediate dependents from registered page indexes.
     * @param controller Changed field index.
     * @param result In/out dependent mask.
     * @return No value.
     */
    void dependents(size_t controller,ArdAppConfigFieldMask& result) const {for(size_t i=0;i<size;++i)entries[i]->edges.dependents(controller,result);}
#endif
    void commit(std::unique_ptr<Entry> entry) {entries[size++]=std::move(entry);}
    /**
     * @brief Locate a field index by its identifier.
     * @param p Source or destination byte pointer.
     * @param f Field definition to inspect or normalize.
     * @return Locate a field index by its identifier.
     */
    size_t fieldIndex(size_t p,size_t f) const {return entries[p]->fields[f].index;}
    /**
     * @brief Resolve a field value from runtime state, stored configuration or its definition default.
     * @param p Source or destination byte pointer.
     * @param f Field definition to inspect or normalize.
     * @return A field value from runtime state, stored configuration or its definition default.
     */
    /** @brief Read an explicit basic default without parsing HA or presentation metadata.
     * Return false for extended definitions, missing defaults or allocation failure. */
    bool explicitInitial(const char* id,V& result) const {
      for(size_t p=0;p<size;++p)for(size_t f=0;f<entries[p]->count;++f)if(!strcmp(fieldId(p,f),id)){
        const auto& e=*entries[p];const auto& meta=e.fields[f];if(meta.extended)return false;
        String source=e.flash?ArdJsonFieldSlices(e.flash,e.sourceLength).slice(meta.offset,meta.length):e.source.substring(meta.offset,meta.offset+meta.length);
        if(source.length()!=meta.length)return false;ArdJsonFieldSlices slices(source);bool found=false;
        bool ok=slices.members(0,source.length(),[&](const String& key,size_t start,size_t bytes){if(key=="default"){result=codec.parse(slices.slice(start,bytes));found=result.isValid()&&!result.isUndefined();}return true;});
        return ok&&found;
      }
      return false;
    }
    /** @brief Parse only visibility rules for an HTTP condition response. */
    V fieldConditions(size_t p,size_t f) const {
      const auto& e=*entries[p];const auto& meta=e.fields[f];V result=V::object();
      String source=e.flash?ArdJsonFieldSlices(e.flash,e.sourceLength).slice(meta.offset,meta.length):e.source.substring(meta.offset,meta.offset+meta.length);
      if(source.length()!=meta.length)return V();ArdJsonFieldSlices slices(source);
      if(!slices.members(0,source.length(),[&](const String& key,size_t start,size_t bytes){if(key=="visibleWhen")result[key]=codec.parse(slices.slice(start,bytes));return result.isValid();}))return V();
      if(e.condition.length())result["_pageVisibleWhen"]=codec.parse(e.condition);
      return result.isValid()?result:V();
    }
    /** @brief Stream a basic definition from its immutable, validated source. */
    String fieldHttpText(size_t p,size_t f) const {
      const Entry& entry=*entries[p];const Field& meta=entry.fields[f];
      String source=entry.flash?ArdJsonFieldSlices(entry.flash,entry.sourceLength).slice(meta.offset,meta.length):entry.source.substring(meta.offset,meta.offset+meta.length);
      if(source.length()!=meta.length)return String();return ArdTopicTemplates::fieldHttpJson(source,topicDevice,meta.extended,meta.persist);
    }
    /** @brief Aggregate source/output buffers for definition streaming, without DOM nodes. */
    size_t fieldHttpWork(size_t index) const {for(size_t p=0;p<size;++p){const auto& e=*entries[p];if(e.count&&index>=e.fields[0].index&&index-e.fields[0].index<e.count){const auto& field=e.fields[index-e.fields[0].index];return ArdHeap::fieldWork(field.length,1,0)+512+(field.extended?field.length+256:0);}}return 2048;}
    V fieldValue(size_t p,size_t f) const {
      const Entry& entry=*entries[p];V value;
      {const Field& meta=entry.fields[f];String source=entry.flash?ArdJsonFieldSlices(entry.flash,entry.sourceLength).slice(meta.offset,meta.length):entry.source.substring(meta.offset,meta.offset+meta.length);ArdJSON::Limits limits;limits.maxNodes=4096;value=codec.parse(source,nullptr,limits);}
      if(value.isUndefined()||!value.isValid()||!ArdTopicTemplates::resolve(value,topicDevice))return V();
      if(entry.fields[f].extended){value["extended"]=true;if(!value.hasOwnProperty("persist"))value["persist"]=entry.fields[f].persist;}
      if(entry.condition.length())value["_pageVisibleWhen"]=codec.parse(entry.condition);
      return value;
    }
    /**
     * @brief Check whether the page source is stored in program memory.
     * @param index Zero-based element or field index.
     * @return True if reads use program memory instead of an owned RAM string.
     */
    bool flashBacked(size_t index) const {return entries[index]->flash!=nullptr;}
    /**
     * @brief Count the RAM bytes owned by this page source.
     * @return Number of RAM source bytes owned by the registry entry; zero for flash-backed sources.
     */
    size_t ownedSourceBytes() const {size_t bytes=0;for(size_t i=0;i<size;++i)bytes+=entries[i]->source.length();return bytes;}
    /**
     * @brief Parse a complete stored page definition when explicitly requested.
     * @param index Zero-based element or field index.
     * @return The parsed page object, or Undefined on an invalid index or JSON failure.
     */
    V parsePage(size_t index) const {
      if(index>=size)return V();
      ArdJSON::Limits limits;limits.maxNodes=4096;limits.maxInputBytes=ArdAppConfigMaxDefinitionBytes;
      const Entry& entry=*entries[index];V page=codec.parse(entry.flash?String(entry.flash):entry.source,nullptr,limits);
      if(!page.isValid()||page["fields"].length()!=entry.count)return V();
      for(size_t f=0;f<entry.count;++f)if(entry.fields[f].extended){page["fields"][f]["extended"]=true;if(!page["fields"][f].hasOwnProperty("persist"))page["fields"][f]["persist"]=entry.fields[f].persist;}
      for(size_t f=0;f<entry.count;++f)if(!ArdTopicTemplates::resolve(page["fields"][f],topicDevice))return V();
      return page;
    }
    /**
     * @brief Access the requested indexed value; mutable JSON access can create a missing member.
     * @param index Zero-based element or field index.
     * @return Access the requested indexed value; mutable JSON access can create a missing member.
     */
    V operator[](size_t index) const {return parsePage(index);}
    /**
     * @brief Build the common HTTP response header prefix.
     * @param index Zero-based element or field index.
     * @return Reference to the requested stored value or component.
     */
    const String& headerPrefix(size_t index) const {return entries[index]->headerPrefix;}
    /**
     * @brief Measure the serialized JSON response length.
     * @param index Zero-based element or field index.
     * @return The serialized JSON response length.
     */
    size_t jsonLength(size_t index) const {return entries[index]->jsonLength;}
    /**
     * @brief Serialize the registered page navigation catalog.
     * @param index Zero-based element or field index.
     * @return Reference to the requested stored value or component.
     */
    const String& menu(size_t index) const {return entries[index]->menu;}
    /**
     * @brief Evaluate whether a page should be visible under its dependency condition.
     * @param index Zero-based element or field index.
     * @return Reference to the requested stored value or component.
     */
    V pageCondition(size_t index) const {return entries[index]->condition.length()?codec.parse(entries[index]->condition):V();}
    /**
     * @brief Measure the serialized page navigation catalog length.
     * @return The serialized page navigation catalog length.
     */
    size_t menuLength() const {size_t n=2;for(size_t p=0;p<size;++p)n+=entries[p]->menu.length()+(p?1:0);return n;}
    /**
     * @brief Measure the serialized value or response without retaining its output.
     * @param limits JSON parsing or serialization resource limits.
     * @param error Output error text; populated when the operation fails.
     * @return Required serialized byte count, or zero on failure with error text where supplied.
     */
    size_t measure(const ArdJSON::Limits& limits,String* error=nullptr) const {
      size_t bytes=2;for(size_t i=0;i<size;++i){size_t length=entries[i]->jsonLength;if(limits.escapeHtml||limits.maxNodes!=4096){V page=(*this)[i];length=ArdJSON::JSON.measure(page,error,limits);}if(!length)return 0;bytes+=length+(i?1:0);yield();}if(bytes>limits.maxOutputBytes){if(error)*error="output limit";return 0;}return bytes;
    }
  };
  PageStore pages;
  /**
   * @brief Validate a page or field identifier against its length and character rules.
   * @param id Page, field or language identifier.
   * @return True if the identifier has an allowed length and characters.
   */
  static bool identifier(const String& id) {
    if (!id.length() || id.length()>48) return false;
    for(size_t i=0;i<id.length();++i) { char c=id[i]; if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-')) return false; }
    return true;
  }
  /**
   * @brief Read the localized names from a definition.
   * @param v Value or mask to compare, combine or store.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  static bool names(const V& v) {
    if(v.type()!=T::Object || !v.length()) return false;
    V keys=v.keys();
    for(size_t i=0;i<keys.length();++i) { const V& label=v[keys[i].asString()]; if(label.type()!=T::String || !label.asString().length() || label.asString().length()>80) return false; }
    return true;
  }
  /**
   * @brief Resolve the page title from its localized names and fallback name.
   * @param v Value or mask to compare, combine or store.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  static bool title(const V& v) {
    if(v.hasOwnProperty("name")&&(v["name"].type()!=T::String||!v["name"].asString().length()||v["name"].asString().length()>80)) return false;
    if(v.hasOwnProperty("names")&&!names(v["names"])) return false;
    return v.hasOwnProperty("name")||v.hasOwnProperty("names");
  }
  /**
   * @brief Copy an enclosing visibility condition to a nested control.
   * @param page Application page definition or identifier.
   * @param index Zero-based element or field index.
   * @return Copy an enclosing visibility condition to a nested control.
   */
  static V inheritCondition(V& page,size_t index) {
#if ARDPORTAL_ENABLE_DEPENDENCIES
    if(page.hasOwnProperty("visibleWhen")) page["fields"][index]["_pageVisibleWhen"]=page["visibleWhen"];
#endif
    return std::move(page["fields"][index]);
  }
#if ARDPORTAL_ENABLE_DEPENDENCIES
  /**
   * @brief Validate the shape and referenced identifiers of a visibility condition.
   * @param condition Visibility dependency condition to validate or evaluate.
   * @param fields Array of field definitions.
   * @return True if the condition is supported; false otherwise.
   */
  bool validCondition(const V& condition,const V& fields) const {
    auto resolve=[&](const V& leaf) {
      if(!identifier(leaf["field"].asString()))return false;
      if(leaf.hasOwnProperty("property")&&!identifier(leaf["property"].asString()))return false;
      const String id=leaf["field"].asString();
      if(indexOf(id)<count())return true;
      for(size_t i=0;i<fields.length();++i)if(fields[i]["id"].asString()==id)return true;
      return false;
    };
    size_t nodes=0;return ArdDependencies::validate(condition,resolve,nodes);
  }
#endif
  struct Entity { ARDPORTAL_NO_THROW_ALLOCATION
#if ARDPORTAL_ENABLE_DEPENDENCIES
    ArdDependencies::Edges edges;
#endif
    String id,source;const __FlashStringHelper* flash=nullptr;uint16_t index=0;bool extended=false,persist=false,transient=false,dependent=false;std::unique_ptr<Entity> next;};
#if ARDPORTAL_ENABLE_DEPENDENCIES
  ArdAppConfigFieldMask dependencyControllers;
  /**
   * @brief Collect immediate dependents from page and HA-only reverse indexes.
   * @param controller Changed controller field index.
   * @return Mask of its immediate dependent fields; empty for unused controllers.
   */
  ArdAppConfigFieldMask dependents(size_t controller) const {
    ArdAppConfigFieldMask result;if(!dependencyControllers.test(controller))return result;
    pages.dependents(controller,result);for(const Entity* e=entities.get();e;e=e->next.get())e->edges.dependents(controller,result);return result;
  }
#endif
  std::unique_ptr<Entity> entities;size_t totalFields=0,entityBytes=0;
  /** @brief Estimate a definition working set without copying JSON.
   * @param index Global field index.
   * @return Temporary parse/serialization estimate. */
  size_t fieldWork(size_t index) const {size_t bytes=pages.fieldWork(index);if(bytes)return bytes;for(const Entity* e=entities.get();e;e=e->next.get())if(e->index==index)return ArdHeap::fieldWork(e->flash?strlen_P(reinterpret_cast<const char*>(e->flash)):e->source.length());return 4096;}
  /** @brief Largest single allocation needed for a registered definition. */
  size_t fieldBlock(size_t index) const {size_t bytes=pages.fieldBlock(index);return bytes?bytes:fieldWork(index);}

  /**
   * @brief Release the resources owned by this instance.
   * @return No value.
   */
  ~ArdDynamicPages(){while(entities){auto next=std::move(entities->next);entities=std::move(next);}}
  /**
   * @brief Locate the indexed field or page with the supplied identifier.
   * @param id Page, field or language identifier.
   * @return Matching zero-based index, or the registry count when absent.
   */
  size_t indexOf(const String& id) const {
    for(size_t p=0;p<pages.length();++p)for(size_t f=0;f<pages.fields(p);++f)if(id==pages.fieldId(p,f))return pages.fieldIndex(p,f);
    for(const Entity* e=entities.get();e;e=e->next.get())if(e->id==id)return e->index;return count();
  }
  /**
   * @brief Resolve the current value published for a Home Assistant entity.
   * @param entity Whether this registration is for a HA-only entity.
   * @return The current value published for a Home Assistant entity.
   */
  V entityValue(const Entity& entity) const {
    ArdJSON::Limits limits;limits.maxNodes=4096;V value=ArdJSON::JSON.parse(entity.flash?String(entity.flash):entity.source,nullptr,limits);
    if(value.isUndefined()||!value.isValid()||!ArdTopicTemplates::resolve(value,pages.topicDevice))return V();
    if(entity.extended){value["extended"]=true;if(!value.hasOwnProperty("persist"))value["persist"]=entity.persist;}return value;
  }
  /**
   * @brief Read the indexed definition of a single field.
   * @param id Page, field or language identifier.
   * @return The indexed definition of a single field.
   */
  V field(const String& id) const {
    for(size_t p=0;p<pages.length();++p)for(size_t f=0;f<pages.fields(p);++f)if(id==pages.fieldId(p,f))return pages.fieldValue(p,f);
    for(const Entity* e=entities.get();e;e=e->next.get())if(e->id==id)return entityValue(*e);return V();
  }
  /**
   * @brief Read the number of registered pages or entities.
   * @return Number of registered fields or entries, as defined by this registry.
   */
  size_t count() const {return totalFields;}
  /**
   * @brief Access a JSON member or array element at the requested position.
   * @param index Zero-based element or field index.
   * @return Access a JSON member or array element at the requested position.
   */
  V at(size_t index) const {
    for(size_t p=0;p<pages.length();++p)for(size_t f=0;f<pages.fields(p);++f)if(pages.fieldIndex(p,f)==index)return pages.fieldValue(p,f);
    for(const Entity* e=entities.get();e;e=e->next.get())if(e->index==index)return entityValue(*e);return V();
  }
  /**
   * @brief Read a field identifier from the compact identifier pool.
   * @param index Zero-based element or field index.
   * @return The indexed identifier; an empty identifier indicates an invalid index.
   */
  String idAt(size_t index) const {
    for(size_t p=0;p<pages.length();++p)for(size_t f=0;f<pages.fields(p);++f)if(pages.fieldIndex(p,f)==index)return pages.fieldId(p,f);
    for(const Entity* e=entities.get();e;e=e->next.get())if(e->index==index)return e->id;return String();
  }
#if ARDPORTAL_ENABLE_DEPENDENCIES
  /**
   * @brief Read the dependent field at the requested index.
   * @param index Zero-based element or field index.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool dependentAt(size_t index) const {
    for(size_t p=0;p<pages.length();++p)for(size_t f=0;f<pages.fields(p);++f)if(pages.fieldIndex(p,f)==index)return pages.dependent(p,f);
    for(const Entity* e=entities.get();e;e=e->next.get())if(e->index==index)return e->dependent;return false;
  }
#endif
  /**
   * @brief Check transient at.
   * @param index Zero-based element or field index.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool transientAt(size_t index) const {
    for(size_t p=0;p<pages.length();++p)for(size_t f=0;f<pages.fields(p);++f)if(pages.fieldIndex(p,f)==index)return pages.transient(p,f);
    for(const Entity* e=entities.get();e;e=e->next.get())if(e->index==index)return e->transient;return true;
  }
  /**
   * @brief Check whether the registry already contains the requested identifier.
   * @param values Application values or translation substitutions.
   * @param value Input value, or output destination when passed by mutable reference.
   * @return True if the identifier already exists in the registry.
   */
  static bool contains(const V& values,const String& value) {
    for(size_t i=0;i<values.length();++i) if((values[i].type()==T::String?values[i].asString():values[i]["value"].asString())==value) return true;
    return false;
  }
  /**
   * @brief Build or read the initial field value.
   * @param f Field definition to inspect or normalize.
   * @return Or read the initial field value.
   */
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
  /**
   * @brief Validate a value against the application control type and constraints.
   * @param f Field definition to inspect or normalize.
   * @param v Value or mask to compare, combine or store.
   * @return True if the supplied value satisfies the field constraints.
   */
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
  /**
   * @brief Validate a field definition and record a detailed registration error.
   * @param f Field definition to inspect or normalize.
   * @param fields Array of field definitions.
   * @param checkDependencies Whether dependency validation is required.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool validateField(const V& f,const V& fields,bool checkDependencies=true) {
      (void)fields;(void)checkDependencies;
      String id=f["id"].asString(),type=f["type"].asString();
      if(id=="availability"||id=="status")return fail("field",id,"reserved id");
      if(!identifier(id))return fail("field",id,"invalid id: use 1..48 ASCII letters, digits, - or _");
      if(indexOf(id)<count())return fail("field",id,"duplicate field id");
      if(!title(f))return fail("field",id,"invalid name/names (1..80 bytes)");

      if(!ardPortalBasicControl(type)&&(!ArdHa::extended(f)||ArdHa::descriptor(type,true).isUndefined())) return fail("field",id,"unsupported or disabled control type");
      if(f.hasOwnProperty("ha")&&f["ha"].type()!=T::Object) return fail("field",id,"ha must be an object");
      if(f.hasOwnProperty("icon")) { String icon=f["icon"].asString(); if(icon.indexOf("mdi:")!=0 || icon.length()>80 || icon.length()<5) return fail("field",id,"invalid icon: expected mdi: name, 5..80 bytes"); }
#if (ARDPORTAL_ENABLE_CONTROL_SLIDER || ARDPORTAL_ENABLE_CONTROL_CLIMATE)
    if(type=="slider" || type=="climate") { double lo,hi,step; if(!f["min"].toDouble(lo)||!f["max"].toDouble(hi)||lo>=hi) return fail("field",id,"invalid min/max: expected numbers and min < max"); if(f.hasOwnProperty("step")&&(!f["step"].toDouble(step)||step<=0)) return fail("field",id,"step must be a positive number"); }
#endif
#if ARDPORTAL_ENABLE_CONTROL_SELECT
    if(type=="select") { const V& o=f["options"]; if(o.type()!=T::Array||!o.length()||o.length()>16) return fail("field",id,"options must contain 1..16 entries"); for(size_t j=0;j<o.length();++j) { if(o[j]["value"].type()!=T::String || o[j]["value"].asString().length()>128 || strlen(o[j]["value"].asString().c_str())!=o[j]["value"].asString().length() || !title(o[j])) return fail("field",id,"invalid option value/title"); for(size_t k=0;k<j;++k) if(o[j]["value"].asString()==o[k]["value"].asString()) return fail("field",id,"duplicate option value"); } }
#endif
#if ARDPORTAL_ENABLE_CONTROL_CLIMATE
    if(type=="climate") for(const char* key:{"modes","fan_modes"}) if(f.hasOwnProperty(key)) { const V& modes=f[key]; if(modes.type()!=T::Array||!modes.length()||modes.length()>10) return fail("field",id,"modes/fan_modes must contain 1..10 entries"); for(size_t j=0;j<modes.length();++j) { String m=modes[j].asString(); if(!identifier(m)) return fail("field",id,"invalid mode identifier"); if(String(key)=="modes"&&m!="off"&&m!="heat"&&m!="cool"&&m!="auto"&&m!="dry"&&m!="fan_only") return fail("field",id,"unsupported climate mode"); for(size_t k=0;k<j;++k) if(m==modes[k].asString()) return fail("field",id,"duplicate mode"); } }
#endif
#if ARDPORTAL_ENABLE_DEPENDENCIES
      if(checkDependencies&&f.hasOwnProperty("visibleWhen")&&!validCondition(f["visibleWhen"],fields))return fail("field",id,"invalid visibleWhen condition or referenced field");
#else
      // Reject unsupported dependencies rather than exposing conditional fields.
      if(f.hasOwnProperty("visibleWhen")) return fail("field",id,"dependencies are disabled");
#endif
      if(!validValue(f,initial(f))) return fail("field",id,"invalid default value for control");
    return true;
  }
  /**
   * @brief Check add entity.
   * @param page Application page definition or identifier.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool addEntity(const V& page) {
    error={};
    if(page.type()!=T::Object)return fail("page","","page must be an object");
    if(!identifier(page["id"].asString()))return fail("page","","invalid page id");
    if(!title(page))return fail("page","","invalid page name/names (1..80 bytes)");
    if(!page["order"].isInteger())return fail("page","","order must be an integer");
    if(page["fields"].type()!=T::Array||!page["fields"].length())return fail("page","","fields must be a nonempty array");
    if(count()+page["fields"].length()>ArdAppConfigMaxFields)return fail("limits","","field limit: 1024");
    const V& fields=page["fields"];
#if ARDPORTAL_ENABLE_DEPENDENCIES
    if(page.hasOwnProperty("visibleWhen")&&!validCondition(page["visibleWhen"],fields))return fail("page","","invalid page visibleWhen condition");
#else
    if(page.hasOwnProperty("visibleWhen"))return fail("page","","dependencies are disabled");
#endif
    for(size_t i=0;i<fields.length();++i) {
      if(!validateField(fields[i],fields))return false;
      for(size_t j=0;j<i;++j)if(fields[j]["id"].asString()==fields[i]["id"].asString())return fail("field",fields[i]["id"].asString(),"duplicate field id in page");
    }
#if ARDPORTAL_ENABLE_DEPENDENCIES
    for(size_t i=0;i<fields.length();++i) {
      for(const char* key:{"visibleWhen","_pageVisibleWhen"}) {
        const V& rule=fields[i][key];
        for(size_t ordinal=0;;++ordinal){size_t skip=ordinal;const V* leaf=ArdDependencies::leaf(rule,skip);
          if(!leaf)break;
          if((*leaf)["field"].asString()==fields[i]["id"].asString())return fail("field",fields[i]["id"].asString(),"dependency cycle");
        }
      }
    }
#endif
    const size_t previousCount=totalFields;
    {
      if(fields.length()!=1)return fail("registry","","expected one HA-only field");
      std::unique_ptr<Entity> entry(new(std::nothrow) Entity());if(!entry)return fail("registry","","out of memory allocating entity");
      entry->id=fields[0]["id"].asString();ArdJSON::Limits limits;limits.maxNodes=4096;entry->source=ArdJSON::JSON.stringify(fields[0],false,nullptr,limits);if(!entry->source.length())return fail("registry",entry->id,"entity serialization failed");
      entry->index=uint16_t(totalFields);entry->extended=fields[0]["extended"].asBool();entry->persist=fields[0]["persist"].asBool();entry->transient=ArdHa::transient(fields[0]);entry->dependent=fields[0].hasOwnProperty("visibleWhen");
#if ARDPORTAL_ENABLE_DEPENDENCIES
      auto resolve=[&](const String& id){return indexOf(id);};
      if(!entry->edges.addField(fields[0],entry->index,resolve))return fail("registry",entry->id,"out of memory indexing dependencies");
#endif
      entityBytes+=entry->source.length();entry->next=std::move(entities);entities=std::move(entry);
    }
    totalFields+=fields.length();
    struct Rollback {ArdDynamicPages& owner;size_t previous;bool keep=false;~Rollback(){if(keep)return;owner.totalFields=previous;owner.entityBytes-=owner.entities->source.length();auto next=std::move(owner.entities->next);owner.entities=std::move(next);}} rollback{*this,previousCount};
    ArdJSON::Limits limits;limits.maxNodes=4096;limits.maxOutputBytes=ArdAppConfigMaxDefinitionBytes;
    size_t bytes=pages.measure(limits);if(!bytes)return fail("registry","","definition measurement failed");
    if(bytes+entityBytes>ArdAppConfigMaxDefinitionBytes)return fail("limits","",String("total definition limit: ")+String(ArdAppConfigMaxDefinitionBytes)+" bytes");
    // Every generated command and acknowledgement name must be unambiguous.
    for(size_t i=0;i<count();++i) {V f=at(i);V suffixes=V::array();
#if ARDPORTAL_ENABLE_CONTROL_CLIMATE
      if(f["type"].asString()=="climate")for(const char* suffix:{"temperature","mode","fan"})suffixes.push(suffix);
#endif
      if(ArdHa::extended(f)){V resolved=f;if(!ArdHa::normalize(resolved,true))return fail("commands",f["id"].asString(),"descriptor normalization failed");for(size_t c=0;c<resolved["controls"].length();++c)if(resolved["controls"][c]["command"].asString().length())suffixes.push(resolved["controls"][c]["command"]);suffixes.push("ack");}
      for(size_t k=0;k<suffixes.length();++k)if(indexOf(f["id"].asString()+"_"+suffixes[k].asString())<count())return fail("commands",f["id"].asString(),"generated command/ack id collides with field: "+f["id"].asString()+"_"+suffixes[k].asString());yield();
    }
    if(fields[0]["extended"].asBool()||fields[0]["type"].asString()=="climate")commandOwners|=ArdAppConfigFieldMask::forField(previousCount);
#if ARDPORTAL_ENABLE_DEPENDENCIES
    entities->edges.controllers(dependencyControllers);
#endif
    rollback.keep=true; return true;
  }

};
