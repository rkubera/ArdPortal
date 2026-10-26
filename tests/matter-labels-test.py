#!/usr/bin/env python3
"""Exercise actual endpoint label encoding, UTF-8 bounds and SDK read errors."""
import subprocess
import tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
header=r'''
#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <utility>
using CHIP_ERROR=int;
#define ReturnErrorOnFailure(expr) do { int error=(expr);if(error)return error;} while(0)
namespace chip {
struct CharSpan {const char* text=nullptr;static CharSpan fromCharString(const char* p){return {p};}};
namespace app {
namespace DataModel {using ActionReturnStatus=int;struct ReadAttributeRequest {struct {uint32_t mAttributeId;} path;};}
namespace Clusters {namespace FixedLabel {
namespace Attributes {struct LabelList {static constexpr uint32_t Id=0;};}
namespace Structs {namespace LabelStruct {struct Type {CharSpan label,value;};}}
}}
struct AttributeValueEncoder {
 std::vector<std::pair<std::string,std::string>> values;bool fail=false;
 template<class F>int EncodeList(F f){return f(*this);}
 int Encode(const Clusters::FixedLabel::Structs::LabelStruct::Type& label)const {if(fail)return -1;const_cast<AttributeValueEncoder*>(this)->values.emplace_back(label.label.text,label.value.text);return 0;}
};
namespace Clusters {class FixedLabelCluster {public:explicit FixedLabelCluster(uint16_t){}virtual ~FixedLabelCluster()=default;virtual int ReadAttribute(const DataModel::ReadAttributeRequest&,AttributeValueEncoder&){return 42;}};}
struct ServerClusterRegistration {template<class T>explicit ServerClusterRegistration(T&){}};
}}
'''
fixture=r'''
#include <cassert>
#include "MatterLabels.h"
int main(){
 chip::app::DataModel::ReadAttributeRequest request{{0}};
 for(const auto& example:std::vector<std::pair<std::string,std::string>>{{"Outlet","Outlet"},{"ABCDEFGHIJKLMNOPQRST","ABCDEFGHIJKLMNOP"},{"abcdefghijklm😀tail","abcdefghijklm"},{"abcdefghijkl😀tail","abcdefghijkl😀"},{"",""}}){
  ArdMatterLabel label(1,example.first.c_str());assert(std::string(label.text)==example.second);assert(std::strlen(label.text)<=16);
  chip::app::AttributeValueEncoder encoder;assert(label.ReadAttribute(request,encoder)==0);assert(encoder.values.size()==2);
  assert(encoder.values[0]==std::make_pair(std::string("name"),example.second));
  assert(encoder.values[1]==std::make_pair(std::string("ha_entitylabel"),example.second));
  encoder.fail=true;assert(label.ReadAttribute(request,encoder)==-1);
 }
 ArdMatterLabel label(3,"Brightness");chip::app::AttributeValueEncoder encoder;request.path.mAttributeId=65533;assert(label.ReadAttribute(request,encoder)==42);assert(encoder.values.empty());
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-labels-') as directory:
 p=Path(directory)
 for name in ('app/clusters/fixed-label-server/fixed-label-cluster.h','app/server-cluster/ServerClusterInterfaceRegistry.h'):
  file=p/name;file.parent.mkdir(parents=True,exist_ok=True);file.write_text(header if 'fixed-label-cluster' in name else '#pragma once\n')
 (p/'test.cpp').write_text(fixture)
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(p),'-I'+str(root/'src'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: Matter endpoint names, HA label alias, 16-byte UTF-8 boundaries and label read failures')
