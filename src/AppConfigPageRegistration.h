// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "AppConfigPageScanner.h"
#include "DynamicPages.h"
struct ArdAppConfigPageRegistration {
  ARDPORTAL_NO_THROW_ALLOCATION
  using V=ArdJSON::JSONVar;
  enum Stage { Scan, Fields, Checks, ExistingCommands, Cycles, Commit } stage=Scan;
  String source,topicDevice;
  ArdHeap::Snapshot heapBefore=ArdHeap::sample();
  bool memoryDeferred=false;
  size_t maximumFieldWork=4096,maximumFieldBlock=1024;
  bool wholePass=false;
  const __FlashStringHelper* flash=nullptr;
  size_t length=0,nodes=2,first=0,processed=0,checkIndex=0,encodedFields=0;
  V header=V::object();
  ArdAppConfigFieldMask stateMask,dependencyMask,commandsMask,checksMask,existingCommands;
  ArdAppConfigPageScanner scanner;
#if ARDPORTAL_ENABLE_DEPENDENCIES
  ArdDependencies::Stack cycleStack;
  ArdAppConfigFieldMask cycleActive,cycleComplete;
  size_t cycleDepth=0,cycleRoot=0;
#endif
  std::unique_ptr<ArdDynamicPages::PageStore::Entry> entry;
  /**
   * @brief Build a field-slice reader over the staged page source.
   * @return Reader referencing the staged source and its current field limits.
   */
  ArdJsonFieldSlices reader() const {return flash?ArdJsonFieldSlices(flash,length):ArdJsonFieldSlices(source);}
  /**
   * @brief Parse the indexed staged field without parsing the complete page.
   * @param index Zero-based element or field index.
   * @return Parsed field object, or Undefined if the index or JSON is invalid.
   */
  V field(size_t index) const {
    const auto& meta=entry->fields[index];ArdJSON::Limits limits;limits.maxNodes=4096;
    V value=ArdJSON::JSON.parse(reader().slice(meta.offset,meta.length),nullptr,limits);
    if(!ArdTopicTemplates::resolve(value,topicDevice))return V();
    if(meta.extended){value["extended"]=true;if(!value.hasOwnProperty("persist"))value["persist"]=meta.persist;}
    return value;
  }
  /**
   * @brief Read a field identifier from the compact identifier pool.
   * @param index Zero-based element or field index.
   * @return The indexed identifier; an empty identifier indicates an invalid index.
   */
  const char* idAt(size_t index) const {return entry->ids.c_str()+entry->fields[index].idOffset;}
  /**
   * @brief Locate the indexed field or page with the supplied identifier.
   * @param id Page, field or language identifier.
   * @return Matching zero-based index, or the registry count when absent.
   */
  size_t indexOf(const String& id) const {for(size_t i=0;i<processed;++i)if(id==entry->ids.c_str()+entry->fields[i].idOffset)return i;return entry->count;}
};
