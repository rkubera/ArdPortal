// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "AppConfigPageScanner.h"
#include "DynamicPages.h"
struct ArdAppConfigPageRegistration {
  using V=ArdJSON::JSONVar;
  enum Stage { Scan, Fields, Checks, ExistingCommands, Commit } stage=Scan;
  String source;
  bool wholePass=false;
  const __FlashStringHelper* flash=nullptr;
  size_t length=0,nodes=2,first=0,processed=0,checkIndex=0,encodedFields=0;
  V header=V::object();
  ArdAppConfigFieldMask stateMask,dependencyMask,commandsMask,checksMask,existingCommands;
  ArdAppConfigPageScanner scanner;
  std::unique_ptr<ArdDynamicPages::PageStore::Entry> entry;
  ArdJsonFieldSlices reader() const {return flash?ArdJsonFieldSlices(flash,length):ArdJsonFieldSlices(source);}
  V field(size_t index) const {
    const auto& meta=entry->fields[index];ArdJSON::Limits limits;limits.maxNodes=4096;
    V value=ArdJSON::JSON.parse(reader().slice(meta.offset,meta.length),nullptr,limits);
    if(meta.extended){value["extended"]=true;if(!value.hasOwnProperty("persist"))value["persist"]=meta.persist;}
    return value;
  }
  const char* idAt(size_t index) const {return entry->ids.c_str()+entry->fields[index].idOffset;}
  size_t indexOf(const String& id) const {for(size_t i=0;i<processed;++i)if(id==entry->ids.c_str()+entry->fields[i].idOffset)return i;return entry->count;}
};
