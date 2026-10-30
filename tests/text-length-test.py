#!/usr/bin/env python3
"""Check JSON string limits at registration and at value updates."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = r'''
#include <cassert>
#define ARDPORTAL_DEFINE_CONTROL_DATA
#include "DynamicPages.h"
uint32_t fakeMillis=0;
using V=ArdJSON::JSONVar;
int main() {
 ArdDynamicPages registry;
 for (const char* type : {"edit", "text"}) {
  V f=V::object(); f["id"]="message"; f["name"]="Message"; f["type"]=type;
  size_t baseline=String(type)=="edit"?128:512;
  String value; for(size_t i=0;i<baseline;++i)value+='a';
  assert(registry.validateField(f,V::array()));
  assert(ArdDynamicPages::validValue(f,value));
  value+='a'; assert(!ArdDynamicPages::validValue(f,value));
  f["maxLength"]=1024;
  assert(registry.validateField(f,V::array()));
  while(value.length()<1024)value+='a';
  assert(ArdDynamicPages::validValue(f,value));
  value+='a'; assert(!ArdDynamicPages::validValue(f,value));
  f["default"]=value; assert(!registry.validateField(f,V::array())); f.remove("default");
  f["maxLength"]=4;
  assert(ArdDynamicPages::validValue(f,"ąą"));
  assert(!ArdDynamicPages::validValue(f,"ąąa"));
  String nul="a";nul+=char(0);nul+='b';assert(!ArdDynamicPages::validValue(f,nul));
  for(const char* invalid:{"0","-1","1.5","8193","\"1024\"","true","null"}) {
   f["maxLength"]=ArdJSON::JSON.parse(invalid);
   assert(!registry.validateField(f,V::array()));
   assert(!ArdDynamicPages::validValue(f,"a"));
  }
  f["maxLength"]=8192;assert(registry.validateField(f,V::array()));
 }
 auto f=ArdJSON::JSON.parse(R"({"id":"reading","name":"Reading","type":"text","class":"temperature","maxLength":100})");
 assert(!registry.validateField(f,V::array()));
 f=ArdJSON::JSON.parse(R"({"id":"toggle","name":"Toggle","type":"switch","maxLength":100})");
 assert(!registry.validateField(f,V::array()));
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-text-length-') as directory:
    path = Path(directory)
    (path/'test.cpp').write_text(source)
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', '-I'+str(root/'tests/host'), '-I'+str(root/'src'), str(path/'test.cpp'), str(root/'src/JsonStream.cpp'), str(root/'src/JsonDocumentStream.cpp'), '-o', str(path/'test')], check=True)
    subprocess.run([str(path/'test')], check=True)
print('PASS: text/edit defaults, custom limits, UTF-8, NUL, invalid definitions and defaults')
