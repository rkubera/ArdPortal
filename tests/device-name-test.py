#!/usr/bin/env python3
# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
"""Exhaustively check the compressed Unicode index and UTF-8 name validation."""
import subprocess
import tempfile
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ranges = json.loads((ROOT/'src/tools/device_name_characters.json').read_text())['ranges']
source = r'''
#include "DeviceName.h"
#include <cassert>
uint32_t fakeMillis=0;
static const uint32_t expected[][2] = {RANGES};
int main(){
 size_t range=0;
 for(uint32_t cp=0;cp<0x110000;++cp){
   while(range<sizeof(expected)/sizeof(expected[0])&&cp>expected[range][1])++range;
   bool allowed=range<sizeof(expected)/sizeof(expected[0])&&cp>=expected[range][0];
   assert(ArdDeviceName::letterOrNumber(cp)==allowed);
 }
 assert(!ArdDeviceName::letterOrNumber(UINT32_MAX));
 for(const char* name:{"","ArdPortal","Salon 1","Żółć","東京","𐐀","A-B_C"})assert(ArdDeviceName::valid(String(name)));
 for(const char* name:{" ","A/B","A.B","😀","\xcc\x81","\xc0\xaf","\xed\xa0\x80","\xf4\x90\x80\x80","\xe2\x82"})assert(!ArdDeviceName::valid(String(name)));
 assert(ArdDeviceName::valid(String("12345678901234567890123456789012")));
 assert(!ArdDeviceName::valid(String("123456789012345678901234567890123")));
 String embedded="A";embedded+=char(0);embedded+="B";
 assert(!ArdDeviceName::valid(embedded));
 assert(ArdDeviceName::ap("Salon 1")=="Salon-1");
 assert(ArdDeviceName::hostname("Żółć","device")=="device");
 assert(ArdDeviceName::hostname("Salon 1","device")=="Salon-1");
}
'''.replace('RANGES', ','.join('{'+str(a)+','+str(b)+'}' for a,b in ranges))
with tempfile.TemporaryDirectory(prefix='ardportal-device-name-') as directory:
    work = Path(directory)
    (work/'test.cpp').write_text(source)
    subprocess.run(['c++','-std=c++17','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                    '-I'+str(ROOT/'tests/host'),'-I'+str(ROOT/'src'),str(work/'test.cpp'),'-o',str(work/'test')],check=True)
    subprocess.run([str(work/'test')],check=True)
print('PASS: all 1114112 Unicode code points, range/block boundaries, UTF-8 errors, name limits and hostname fallback')
