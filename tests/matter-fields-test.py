#!/usr/bin/env python3
# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
"""Matter mappings share control semantics and have no SDK dependency."""
import os
import re
import subprocess
import tempfile
from pathlib import Path
root = Path(__file__).resolve().parents[1]
source = r'''#include <cassert>
#define ARDPORTAL_DEFINE_CONTROL_DATA
#include "MatterFields.h"
#include "DynamicPages.h"
uint32_t fakeMillis=0;
using namespace ArdMatterFields;
V field(const char* json) { auto f=ArdJSON::JSON.parse(json); if (!ardPortalBasicControl(f["type"].asString())) assert(ArdControl::normalize(f)); return f; }
int main() {
 assert(kind(field(R"({"type":"switch"})"))==Kind::Outlet);
 assert(kind(field(R"({"type":"switch","class":"light"})"))==Kind::OnOffLight);
 assert(kind(field(R"({"type":"switch","class":"pump"})"))==Kind::Unsupported);
 assert(kind(field(R"({"type":"text","class":"temperature"})"))==Kind::Temperature);
 assert(kind(field(R"({"type":"text"})"))==Kind::Unsupported);
 assert(kind(field(R"({"type":"binary_sensor","class":"door"})"))==Kind::Contact);
 assert(kind(field(R"({"type":"binary_sensor","class":"moisture"})"))==Kind::Leak);
 assert(kind(field(R"({"type":"slider","class":"brightness","min":0,"max":100})"))==Kind::Dimmer);
 assert(kind(field(R"({"type":"slider","min":0,"max":100})"))==Kind::Unsupported);
 assert(kind(field(R"({"type":"slider","class":"brightness","min":1,"max":100})"))==Kind::Unsupported);
 assert(kind(field(R"({"type":"button"})"))==Kind::Unsupported);
 assert(level(-1,100)==0 && level(100,100)==254 && level(200,100)==254);
 auto f=field(R"({"type":"slider","class":"brightness","min":0,"max":100,"step":5})");
 assert(fromLevel(127,f)==50 && fromLevel(254,f)==100);
 assert(kind(field(R"({"type":"climate","min":7,"max":30,"modes":["off","heat"]})"))==Kind::Climate);
 assert(kind(field(R"({"type":"climate","min":7,"max":30,"modes":["off","cool"]})"))==Kind::Unsupported);
 assert(kind(field(R"({"type":"climate","min":16,"max":30,"modes":["off","heat_cool"]})"))==Kind::Climate);
 assert(kind(field(R"({"type":"climate","min":16,"max":30,"modes":["off","auto"]})"))==Kind::Unsupported);
 assert(kind(field(R"({"type":"switch","readonly":true})"))==Kind::Unsupported);
 auto custom=field(R"({"type":"fan","default":{"state":"IDLE","percentage":0,"oscillation":"oscillate_off","direction":"forward"},"controls":[{"key":"state","type":"switch","on":"RUN","off":"IDLE","command":""},{"key":"percentage","type":"slider","min":0,"max":100,"step":1,"command":"percentage"}]})");
 assert(state(custom,true)=="RUN" && state(custom,false)=="IDLE");
 assert(mode(0) && !mode(2));
}
'''
example = (root/'examples/MatterPortal/MatterPortal.ino').read_text()
page = re.search(r'R"JSON\((.*?)\)JSON"', example, re.S).group(1)
example_checks = r'''
 auto page=ArdJSON::JSON.parse(R"EXAMPLE(%s)EXAMPLE");
 assert(ArdDynamicPages::identifier(page["id"].asString()));
 assert(ArdDynamicPages::title(page) && page["order"].isInteger());
 assert(page["fields"].length()==3);
 ArdDynamicPages registry;
 const Kind expected[]={Kind::Outlet,Kind::Temperature,Kind::Dimmer};
 for(size_t i=0;i<page["fields"].length();++i){
  auto f=page["fields"][i];
  if(!ardPortalBasicControl(f["type"].asString())) assert(ArdControl::normalize(f));
  assert(registry.validateField(f,page["fields"]) && kind(f)==expected[i]);
 }
''' % page
fixture = (root/'tests/device/MatterControls.h').read_text()
fixture_json = re.findall(r'R"JSON\((.*?)\)JSON"', fixture, re.S)
validation_checks = r'''
 auto validation=ArdJSON::JSON.parse(R"VALIDATION(%s)VALIDATION");
 assert(ArdDynamicPages::identifier(validation["id"].asString()));
 assert(ArdDynamicPages::title(validation) && !validation.hasOwnProperty("order"));
 const Kind validationKinds[]={Kind::Outlet,Kind::OnOffLight,Kind::Dimmer,Kind::Light,
 Kind::Temperature,Kind::Humidity,Kind::Pressure,Kind::Illuminance,Kind::Contact,
 Kind::Occupancy,Kind::Leak,Kind::Fan,Kind::Climate,Kind::Climate,Kind::Climate,Kind::Cover,Kind::Unsupported,Kind::Unsupported};
 assert(validation["fields"].length()==18);
 auto high=ArdJSON::JSON.parse(R"SNAPSHOT(%s)SNAPSHOT");
 auto low=ArdJSON::JSON.parse(R"SNAPSHOT(%s)SNAPSHOT");
 for(size_t i=0;i<validation["fields"].length();++i){
  auto f=validation["fields"][i];
  if(!ardPortalBasicControl(f["type"].asString())) assert(ArdControl::normalize(f));
  assert(registry.validateField(f,validation["fields"]));
  assert(kind(f)==validationKinds[i]);
  String id=f["id"].asString();
  if(id!="sample" && id!="last_command") {
   assert(ArdDynamicPages::validValue(f,high[id]));
   assert(ArdDynamicPages::validValue(f,low[id]));
  }
 }
 for(const char* cls:{"door","window","opening"}) { auto f=field(R"({"type":"binary_sensor"})"); f["class"]=cls; assert(kind(f)==Kind::Contact); }
 for(const char* cls:{"motion","occupancy","presence"}) { auto f=field(R"({"type":"binary_sensor"})"); f["class"]=cls; assert(kind(f)==Kind::Occupancy); }
 for(const char* cls:{"pressure","atmospheric_pressure"}) { auto f=field(R"({"type":"text"})"); f["class"]=cls; assert(kind(f)==Kind::Pressure); }
 for(const char* cls:{"blind","curtain","shade","shutter","awning"}) { auto f=field(R"({"type":"cover"})"); f["class"]=cls; assert(kind(f)==Kind::Cover); }
''' % tuple(fixture_json)
example_checks += validation_checks
source = source.replace('int main() {', 'int main() {\n' + example_checks)
with tempfile.TemporaryDirectory(prefix="ardportal-matter-fields-") as directory:
    path = Path(directory)
    (path/"test.cpp").write_text(source)
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined", "-I"+str(root/"tests/host"), "-I"+str(root/"src"), str(path/"test.cpp"), str(root/'src/JsonStream.cpp'), str(root/'src/JsonDocumentStream.cpp'), "-o", str(path/"test")], check=True)
    subprocess.run([str(path/"test")], check=True)
print("Matter field mappings passed")
