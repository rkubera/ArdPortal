#!/usr/bin/env python3
# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
"""Run the DHT example's actual measurement code against class validation."""
import os
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sketch = (ROOT / 'examples/DHT/DHT.ino').read_text()
home = re.search(r'HOME_PAGE\[\].*?R"JSON\((.*?)\)JSON"', sketch, re.S)[1]


def function(name):
    return re.search(r'^(?:int|void) ' + name + r'\([^\n]*\) \{.*?^\}', sketch, re.M | re.S)[0]


source = r'''
#include <cassert>
#include <cmath>
#include "HaEntities.h"
using V=ArdJSON::JSONVar;
// Arduino exposes isnan globally; a standalone Linux <cmath> need not do so.
using std::isnan;
uint32_t fakeMillis=0;
struct Portal {
  bool ready=true;
  V settings=V::object();
  bool portalAndAppConfigReady() {return ready;}
  V getAppConfigValue(const char* key) {return settings[key];}
} portal;
float rawTemperature=NAN, rawHumidity=NAN;
V fields=V::object(), readings=V::object();
unsigned writes=0;
float constrain(float value,float low,float high) {return std::max(low,std::min(high,value));}
void updateMeasurement(const char* key,const V& value) {
  assert(ArdControl::validReading(fields[key],value));
  readings[key]=value;
  ++writes;
}
''' + function('roundedTenths') + '\n' + function('updateReadings') + r'''
int main() {
  V page=ArdJSON::JSON.parse(R"HOME(''' + home + r''')HOME");
  for(size_t i=0;i<page["fields"].length();++i) {
    V field=page["fields"][i];
    assert(ArdControl::validClass(field));
    assert(ArdControl::validReading(field,field["default"]));
    assert(!field.hasOwnProperty("ha"));
    fields[field["id"].asString()]=field;
    V ha=V::object(); ArdHa::classification(field,ha);
    assert(ha["state_class"].asString()=="measurement");
  }
  portal.settings["temperature_offset"]=0.0;
  portal.settings["humidity_offset"]=0.0;
  updateReadings();
  assert(readings["temperature"].isNull() && readings["humidity"].isNull());
  rawTemperature=21.24f; rawHumidity=45.26f;
  updateReadings();
  assert(readings["temperature"].type()==V::Type::Number);
  assert(fabs(readings["temperature"].asDouble()-21.2)<1e-9);
  assert(fabs(readings["humidity"].asDouble()-45.3)<1e-9);
  rawTemperature=-10.26f; rawHumidity=95;
  portal.settings["temperature_offset"]=-0.2;
  portal.settings["humidity_offset"]=20;
  updateReadings();
  assert(fabs(readings["temperature"].asDouble()+10.5)<1e-9);
  assert(readings["humidity"].asDouble()==100);
  rawHumidity=5; portal.settings["humidity_offset"]=-20;
  updateReadings(); assert(readings["humidity"].asDouble()==0);
  rawTemperature=NAN;
  updateReadings(); assert(readings["temperature"].isNull() && readings["humidity"].isNull());
  portal.ready=false;
  unsigned before=writes;
  updateReadings(); assert(writes==before);
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-dht-readings-') as directory:
    path = Path(directory)
    (path / 'test.cpp').write_text(source)
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-I' + str(ROOT / 'tests/host'), '-I' + str(ROOT / 'src'),
                    str(path / 'test.cpp'), str(ROOT/'src/JsonStream.cpp'), str(ROOT/'src/JsonDocumentStream.cpp'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
print('PASS: DHT numeric/default/null readings, calibration, rounding, humidity bounds and readiness')
